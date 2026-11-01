#include <Arduino.h>
#include <LSM6DS3.h>
#include <Wire.h>
#include <ArduinoBLE.h>
#include <utility/ATT.h>
#include <mbed.h>
#include <rtos.h>

#ifndef DEVICE_NAME
#error "DEVICE_NAME is set per board in platformio.ini, build with: pio run -e pressure1"
#endif

// 一帧在 BLE 上的格式，改动时要同步改 app 里的 SAMPLE_STRUCT
struct __attribute__((packed)) Sample {
  uint32_t ms;       // 板载时钟
  uint16_t seq;      // 每采样一次 +1，缺号即丢帧
  uint16_t adc[6];   // 12 位原始值
  int16_t  imu[6];   // ax ay az gx gy gz 原始值，IMU 不可用时为 0
};

const uint32_t SAMPLE_PERIOD_MS = 20;  // 50 Hz
const int RING_SIZE = 256;
const int MAX_PACKET = 244;            // 247 字节 MTU 减去 ATT 头
const int MAX_SAMPLES = (MAX_PACKET - 1) / sizeof(Sample);
const int MIN_SAMPLES = 3;             // 旧固件一行 100 多字节能完整收到，3 帧不会被截断

BLEService uartService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
BLECharacteristic txChar("6E400003-B5A3-F393-E0A9-E50E24DCCA9E", BLERead | BLENotify, MAX_PACKET);

const int sensorPins[6] = {A0, A1, A2, A3, A4, A5};
bool imuOK = false;
LSM6DS3 imu(I2C_MODE, 0x6A);

// 采样线程写 ringHead，主循环写 ringTail
Sample ring[RING_SIZE];
volatile uint32_t ringHead = 0, ringTail = 0;
rtos::Thread sampler(osPriorityAboveNormal, 2048);

void readSample(Sample &s) {
  s.ms = millis();
  for (int i = 0; i < 6; i++) s.adc[i] = analogRead(sensorPins[i]);
  if (imuOK) {
    s.imu[0] = imu.readRawAccelX();
    s.imu[1] = imu.readRawAccelY();
    s.imu[2] = imu.readRawAccelZ();
    s.imu[3] = imu.readRawGyroX();
    s.imu[4] = imu.readRawGyroY();
    s.imu[5] = imu.readRawGyroZ();
  } else {
    memset(s.imu, 0, sizeof(s.imu));
  }
}

// BLE 发送会阻塞，采样放在独立线程才能保持固定频率
void samplerLoop() {
  uint16_t seq = 0;
  auto next = rtos::Kernel::Clock::now();
  while (true) {
    Sample s;
    readSample(s);
    s.seq = seq++;
    if (ringHead - ringTail < RING_SIZE) {  // 缓冲满时丢弃，接收端靠 seq 发现
      ring[ringHead % RING_SIZE] = s;
      ringHead = ringHead + 1;
    }
    next += std::chrono::milliseconds(SAMPLE_PERIOD_MS);
    rtos::ThisThread::sleep_until(next);
  }
}

// 通知超过 MTU 会被库静默截断，所以按协商结果决定每包帧数
int samplesPerPacket() {
  uint16_t mtu = 23;
  for (uint16_t handle = 0; handle < 16; handle++) mtu = max(mtu, ATT.mtu(handle));
  return constrain((mtu - 3 - 1) / (int)sizeof(Sample), MIN_SAMPLES, MAX_SAMPLES);
}

void sendPacket() {
  static uint8_t packet[MAX_PACKET];
  uint32_t queued = ringHead - ringTail;
  int count = min((uint32_t)samplesPerPacket(), queued);
  if (count == 0) {
    delay(1);
    return;
  }
  packet[0] = count;
  for (int i = 0; i < count; i++) {
    memcpy(packet + 1 + i * sizeof(Sample), &ring[(ringTail + i) % RING_SIZE], sizeof(Sample));
  }
  txChar.writeValue(packet, 1 + count * sizeof(Sample));
  ringTail = ringTail + count;
}

void printSample(const Sample &s) {
  String data = "";
  for (int i = 0; i < 6; i++) {
    data += "S" + String(i+1) + ":" + String(s.adc[i] * (3.3f / 4095.0f), 2) + " ";
  }
  if (imuOK) {
    const char *names[6] = {"AX", "AY", "AZ", "GX", "GY", "GZ"};
    for (int i = 0; i < 6; i++) {
      float v = i < 3 ? imu.calcAccel(s.imu[i]) : imu.calcGyro(s.imu[i]);
      data += String(names[i]) + ":" + String(v, i < 3 ? 3 : 2) + " ";
    }
  }
  Serial.println(data);
}

void setup() {
  pinMode(LED_RED,   OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE,  OUTPUT);
  digitalWrite(LED_RED,   HIGH);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_BLUE,  HIGH);

  pinMode(PIN_LSM6DS3TR_C_POWER, OUTPUT);
  digitalWrite(PIN_LSM6DS3TR_C_POWER, HIGH);
  delay(10);

  analogReadResolution(12);
  for (int i = 0; i < 6; i++) {
    pinMode(sensorPins[i], INPUT_PULLDOWN);
  }

  imu.settings.gyroRange  = 2000;  // 250 不是合法值，库一直按 2000 配置，app 按此换算
  imu.settings.accelRange = 4;
  imuOK = (imu.begin() == 0);

  Serial.begin(115200);

  // 不管是否插 USB 都开 BLE，串口只作调试输出
  if (BLE.begin()) {
    BLE.setLocalName(DEVICE_NAME);
    BLE.setAdvertisedService(uartService);
    uartService.addCharacteristic(txChar);
    BLE.addService(uartService);
    BLE.setConnectionInterval(6, 12);  // 7.5-15 ms，只是请求，实际由电脑决定
    BLE.advertise();
    digitalWrite(LED_BLUE, LOW);
    delay(200);
    digitalWrite(LED_BLUE, HIGH); // 蓝灯灭=等待BLE连接
  }

  sampler.start(samplerLoop);
}

void loop() {
  BLEDevice central = BLE.central();
  if (central) {
    digitalWrite(LED_BLUE, LOW); // 蓝灯常亮=已连接
    ringTail = ringHead;         // 不发连接之前积压的数据
    while (central.connected()) {
      sendPacket();
    }
    digitalWrite(LED_BLUE, HIGH);
    BLE.advertise();
  } else if (ringHead != ringTail) {
    uint32_t latest = ringHead - 1;
    if (Serial) printSample(ring[latest % RING_SIZE]);
    ringTail = ringHead;
    delay(20);
  }
}
