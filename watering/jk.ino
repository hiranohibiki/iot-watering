#include <ESP8266WiFi.h>
#include <SPI.h>
#include <PubSubClient.h>
#include "BME280_SPI.h"

#define BME_CS 15
#define HUMI 4
#define SENSE_PIN A0

const char* ssid = "Buffalo-G-1E06";
const char* password = "r8sdcyr6xs4aw";

// MQTT設定
const char* mqttDeviceIdPrefix = "";
const char* topicPrefix = "watering/";
const char* mqttServer = "test.mosquitto.org"; // より安定したブローカー
// 他の選択肢:
// const char* mqttServer = "broker.emqx.io"; // EMQX (高速)
// const char* mqttServer = "mqtt.eclipseprojects.io"; // Eclipse (安定)
// const char* mqttServer = "broker.hivemq.com"; // HiveMQ (元の設定)

const unsigned long readInterval = 5000;
unsigned long lastReadTime = 0;

float soil_value = 0;
int soil_threshold = 1000; // 土壌水分閾値
unsigned long pumpStartTime = 0; // ポンプ開始時刻
bool pumpRunning = false; // ポンプ稼働中フラグ
const unsigned long pumpDuration = 3000; // ポンプ稼働時間（3秒）

BME280 bme280;
WiFiClient espClient;
PubSubClient client(espClient);

String mqttDeviceId;
String stateTopic;
String tempTopic;
String humidTopic;
String pressureTopic;
String soilTopic;
String pumpTopic;
String thresholdTopic;
String requestThresholdTopic;
String currentThresholdTopic;
String pumpStateTopic;

void setup() {
  Serial.begin(115200);

  pinMode(HUMI, OUTPUT);
  digitalWrite(HUMI, HIGH);

  // WiFi接続
  setupWifi();

  // ESP8266のチップIDを取得
  String chipId = String(ESP.getChipId(), HEX);
  mqttDeviceId = String(mqttDeviceIdPrefix) + chipId;
  Serial.print("mqttDeviceId: ");
  Serial.println(mqttDeviceId);

  // MQTTトピック名を設定
  stateTopic = topicPrefix + chipId + "/state";
  tempTopic = topicPrefix + chipId + "/temperature";
  humidTopic = topicPrefix + chipId + "/humidity";
  pressureTopic = topicPrefix + chipId + "/pressure";
  soilTopic = topicPrefix + chipId + "/soil";
  pumpTopic = topicPrefix + chipId + "/pump";
  thresholdTopic = topicPrefix + chipId + "/threshold";
  requestThresholdTopic = topicPrefix + chipId + "/request_threshold";
  currentThresholdTopic = topicPrefix + chipId + "/current_threshold";
  pumpStateTopic = topicPrefix + chipId + "/pump_state";

  Serial.print("Temperature Topic: ");
  Serial.println(tempTopic);
  Serial.print("Humidity Topic: ");
  Serial.println(humidTopic);
  Serial.print("Pressure Topic: ");
  Serial.println(pressureTopic);
  Serial.print("Soil Topic: ");
  Serial.println(soilTopic);
  Serial.print("Pump Topic: ");
  Serial.println(pumpTopic);
  Serial.print("Threshold Topic: ");
  Serial.println(thresholdTopic);
  Serial.print("Request Threshold Topic: ");
  Serial.println(requestThresholdTopic);
  Serial.print("Current Threshold Topic: ");
  Serial.println(currentThresholdTopic);
  Serial.print("Pump State Topic: ");
  Serial.println(pumpStateTopic);

  bme280.begin(BME_CS);
  Serial.println("BME280 initialized.");

  // MQTTクライアント設定
  client.setServer(mqttServer, 1883);
  client.setCallback(callback);
  
  // 閾値の初期設定
  soil_threshold = 1000; // デフォルト閾値

  Serial.println("Setup Complete");
}

void setupWifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // ポンプ制御用のコールバック
  if (strcmp(topic, pumpTopic.c_str()) == 0) {
    if ((char)payload[0] == '1') {
      digitalWrite(HUMI, LOW);
      Serial.println("Pump ON via MQTT");
      pumpRunning = true;
      pumpStartTime = millis();
      client.publish(pumpStateTopic.c_str(), "ON");
    } else {
      digitalWrite(HUMI, HIGH);
      Serial.println("Pump OFF via MQTT");
      pumpRunning = false;
      client.publish(pumpStateTopic.c_str(), "OFF");
    }
  }

  // 閾値更新の処理
  if (strcmp(topic, thresholdTopic.c_str()) == 0) {
    char buf[16];
    int len = min((int)length, 15);
    memcpy(buf, payload, len);
    buf[len] = '\0';
    int newThreshold = atoi(buf);
    if (newThreshold > 0 && newThreshold < 2000) {
      soil_threshold = newThreshold;
      Serial.print("Threshold updated: ");
      Serial.println(soil_threshold);
      // 閾値更新後に即時publish
      char thresholdStr[10];
      sprintf(thresholdStr, "%d", soil_threshold);
      client.publish(currentThresholdTopic.c_str(), thresholdStr);
      Serial.print("Current threshold sent: ");
      Serial.println(soil_threshold);
    }
  }

  // 閾値取得要求の処理
  if (strcmp(topic, requestThresholdTopic.c_str()) == 0) {
    char thresholdStr[10];
    sprintf(thresholdStr, "%d", soil_threshold);
    client.publish(currentThresholdTopic.c_str(), thresholdStr);
    Serial.print("Current threshold sent: ");
    Serial.println(soil_threshold);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqttDeviceId.c_str())) {
      Serial.println("connected");
      String mqttPayload = mqttDeviceId + " connected";
      client.publish(stateTopic.c_str(), mqttPayload.c_str());
      client.subscribe(pumpTopic.c_str());
      client.subscribe(thresholdTopic.c_str());
      client.subscribe(requestThresholdTopic.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long currentTime = millis();
  if (currentTime - lastReadTime >= readInterval) {
    lastReadTime = currentTime;

    float temp = bme280.readTemperature();
    float humid = bme280.readHumidity();
    float pressure = bme280.readPressure() / 100.0;

    soil_value = analogRead(SENSE_PIN);

    Serial.print("Temp: "); Serial.print(temp);
    Serial.print(" C, Humid: "); Serial.print(humid);
    Serial.print(" %, Pressure: "); Serial.print(pressure);
    Serial.print(" hPa, Soil Moisture: "); Serial.println(soil_value);

    // 3秒間給水ポンプ制御
    unsigned long currentTime = millis();
    
    // ポンプ稼働時間のチェック
    if (pumpRunning && (currentTime - pumpStartTime >= pumpDuration)) {
      // 3秒経過したらポンプ停止
      Serial.println("Pump OFF (3 seconds elapsed)");
      digitalWrite(HUMI, HIGH);
      pumpRunning = false;
      client.publish(pumpStateTopic.c_str(), "OFF");
    }
    
    // 土壌が乾燥していて、ポンプが停止中の場合にポンプ開始
    if (!pumpRunning && soil_value > soil_threshold) {
      Serial.print("Soil is dry (threshold: ");
      Serial.print(soil_threshold);
      Serial.println("). Pump ON for 3 seconds");
      digitalWrite(HUMI, LOW);
      pumpRunning = true;
      pumpStartTime = currentTime;
      client.publish(pumpStateTopic.c_str(), "ON");
    }
    Serial.println("--------");

    // MQTTでセンサーデータを送信
    char tempStr[10];
    char humidStr[10];
    char pressureStr[10];
    char soilStr[10];
    
    dtostrf(temp, 1, 2, tempStr);
    dtostrf(humid, 1, 2, humidStr);
    dtostrf(pressure, 1, 2, pressureStr);
    dtostrf(soil_value, 1, 0, soilStr);

    client.publish(tempTopic.c_str(), tempStr);
    client.publish(humidTopic.c_str(), humidStr);
    client.publish(pressureTopic.c_str(), pressureStr);
    client.publish(soilTopic.c_str(), soilStr);

    Serial.println("Sensor data published to MQTT");
  }
}