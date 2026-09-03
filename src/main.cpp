#include <Arduino.h>
#include <LSM6DS3.h>
#include <Wire.h>
#include <ArduinoBLE.h>

BLEService uartService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
BLEStringCharacteristic txChar("6E400003-B5A3-F393-E0A9-E50E24DCCA9E", BLERead | BLENotify, 200);

const int sensorPins[6] = {A0, A1, A2, A3, A4, A5};
bool imuOK = false;
LSM6DS3 imu(I2C_MODE, 0x6A);

float axf=0,ayf=0,azf=0,gxf=0,gyf=0,gzf=0;
const float ALPHA=0.2f, GALPHA=0.1f;

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

  imu.settings.gyroRange  = 250;
  imu.settings.accelRange = 4;
  imuOK = (imu.begin() == 0);

  Serial.begin(115200);
  // 等2秒，USB串口能连上就用串口，否则用BLE
  unsigned long t = millis();
  while (!Serial && millis() - t < 2000);

  if (Serial) {
    // 串口模式
    digitalWrite(LED_BLUE, LOW);
  } else {
    // BLE 模式
    if (BLE.begin()) {
      BLE.setLocalName("Pressure-2");
      BLE.setAdvertisedService(uartService);
      uartService.addCharacteristic(txChar);
      BLE.addService(uartService);
      BLE.advertise();
      digitalWrite(LED_BLUE, LOW);
      delay(200);
      digitalWrite(LED_BLUE, HIGH); // 蓝灯灭=等待BLE连接
    }
  }
}

String buildData() {
  String data = "";
  for (int i = 0; i < 6; i++) {
    int raw = analogRead(sensorPins[i]);
    float v = raw * (3.3f / 4095.0f);
    data += "S" + String(i+1) + ":" + String(v, 2) + " ";
  }
  if (imuOK) {
    axf=ALPHA*imu.readFloatAccelX()+(1-ALPHA)*axf;
    ayf=ALPHA*imu.readFloatAccelY()+(1-ALPHA)*ayf;
    azf=ALPHA*imu.readFloatAccelZ()+(1-ALPHA)*azf;
    gxf=GALPHA*imu.readFloatGyroX()+(1-GALPHA)*gxf;
    gyf=GALPHA*imu.readFloatGyroY()+(1-GALPHA)*gyf;
    gzf=GALPHA*imu.readFloatGyroZ()+(1-GALPHA)*gzf;
    data += "AX:" + String(axf,3) + " ";
    data += "AY:" + String(ayf,3) + " ";
    data += "AZ:" + String(azf,3) + " ";
    data += "GX:" + String(gxf,2) + " ";
    data += "GY:" + String(gyf,2) + " ";
    data += "GZ:" + String(gzf,2);
  }
  return data;
}

void loop() {
  if (Serial) {
    // ── 串口模式 ──
    Serial.println(buildData());
    delay(20);
  } else {
    // ── BLE 模式 ──
    BLEDevice central = BLE.central();
    if (central) {
      digitalWrite(LED_BLUE, LOW); // 蓝灯常亮=已连接
      while (central.connected()) {
        txChar.setValue(buildData() + "\n");
        delay(20);
      }
      digitalWrite(LED_BLUE, HIGH);
      BLE.advertise();
    }
  }
}