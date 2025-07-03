#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <MPU6050.h>
#include "WiFi.h"
#include <TFT_eSPI.h>
#include <SPI.h>

#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"

// Create instances of sensors and TFT display
MPU6050 mpu;
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);
TFT_eSPI tft = TFT_eSPI();

const int buttonPin = 15;
bool buttonPressed = false;
unsigned long pressStart = 0;
bool selectionMade = false;

enum ScreenState { HI_THERE, SELECT_PLACE, SELECT_TASK, SHOW_STEPS };
ScreenState currentScreen = HI_THERE;


String places[] = {"Kitchen", "Bathroom", "Hall"};
int totalPlaces = 3;
int currentPlace = 0;

String kitchenQuestions[] = {
  "Make some coffee?",
  "Boil some milk?",
  "Toast some bread?"
};

String bathroomQuestions[] = {
  "Brush your teeth?",
  "Wash your face?",
  "Take a shower?"
};

String hallQuestions[] = {
  "Watch TV?",
  "Listen to music?",
  "See old photos?"
};

int totalQuestions = 3;
int currentQuestion = 0;

void connectAWS() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("h.");
  }

  // Configure WiFiClientSecure with AWS certs
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  client.setServer(AWS_IOT_ENDPOINT, 8883);
  client.setCallback(messageHandler);

  Serial.println("\nConnecting to AWS IoT");
  while (!client.connect(THINGNAME)) {
    Serial.print("cl.");
    delay(100);
  }

  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }

  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
  Serial.println("AWS IoT Connected!");
}

void publishMessage(float ax, float ay, float az, float gx, float gy, float gz) {
  StaticJsonDocument<256> doc;
  doc["accel_x"] = ax;
  doc["accel_y"] = ay;
  doc["accel_z"] = az;
  doc["gyro_x"] = gx;
  doc["gyro_y"] = gy;
  doc["gyro_z"] = gz;

  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

bool messageDisplayed = false; // Show abnormal message only once

void messageHandler(char* topic, byte* payload, unsigned int length) {
  Serial.print("Incoming message on topic: ");
  Serial.println(topic);

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  const char* message = doc["message"];
  Serial.print("Message: ");
  Serial.println(message);

  // Show abnormal message only once
  if (strstr(message, "Abnormal") != NULL && !messageDisplayed) {
    // Display on TFT
    tft.fillRect(10, 10, 300, 100, TFT_BLACK); // Clear area
    tft.setCursor(10, 10);
    tft.setTextColor(TFT_CYAN);
    tft.setTextSize(2);
    tft.println("AWS Says:");
    tft.setCursor(10, 40);
    tft.setTextColor(TFT_YELLOW);
    tft.println(message);

    messageDisplayed = true; // Prevent repeating
    delay(3000); // Let user read the message

    // Switch to Hi There screen
    currentScreen = HI_THERE;
    showHiThere();
  }
}


void setup() {
  Serial.begin(115200);
  Wire.begin();
  mpu.initialize();

  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed");
    while (1);
  }

  Serial.println("MPU6050 connected");
  connectAWS();

  pinMode(buttonPin, INPUT_PULLUP);
  tft.init();
  tft.setRotation(1);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  showHiThere();
}

void loop() {
  int16_t ax, ay, az;
  int16_t gx, gy, gz;

  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  publishMessage(ax, ay, az, gx, gy, gz);

  if (digitalRead(buttonPin) == LOW) {
    if (!buttonPressed) {
      buttonPressed = true;
      pressStart = millis();
    } else {
      if (millis() - pressStart > 1000 && !selectionMade) {
        selectionMade = true;
        switch (currentScreen) {
          case HI_THERE:
            currentScreen = SELECT_PLACE;
            currentPlace = 0;
            showPlace(currentPlace);
            break;
          case SELECT_PLACE:
            currentScreen = SELECT_TASK;
            currentQuestion = 0;
            showQuestion(currentPlace, currentQuestion);
            break;
          case SELECT_TASK:
            currentScreen = SHOW_STEPS;
            showSteps(currentPlace, currentQuestion);
            break;
          case SHOW_STEPS:
            currentScreen = HI_THERE;
            showHiThere();
            break;
        }
      }
    }
  } else {
    if (buttonPressed && !selectionMade) {
      switch (currentScreen) {
        case SELECT_PLACE:
          currentPlace = (currentPlace + 1) % totalPlaces;
          showPlace(currentPlace);
          break;
        case SELECT_TASK:
          currentQuestion = (currentQuestion + 1) % totalQuestions;
          showQuestion(currentPlace, currentQuestion);
          break;
        case SHOW_STEPS:
          currentScreen = HI_THERE;
          showHiThere();
          break;
        case HI_THERE:
          currentScreen = SELECT_PLACE;
          currentPlace = 0;
          showPlace(currentPlace);
          break;
      }
    }
    buttonPressed = false;
    selectionMade = false;
  }

  client.loop();
  delay(1000);
}

void showHiThere() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(60, 80);
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(2);
  tft.println("Hi There!");
}

void showPlace(int index) {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(20, 40);
  tft.setTextColor(TFT_WHITE);
  tft.println("Select a place:");
  tft.setCursor(40, 80);
  tft.setTextColor(TFT_YELLOW);
  tft.println(places[index]);
}

void showQuestion(int placeIndex, int qIndex) {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(20, 30);
  tft.setTextColor(TFT_GREEN);
  tft.println("What do you want?");
  tft.setCursor(30, 80);
  tft.setTextColor(TFT_YELLOW);

  if (placeIndex == 0) tft.println(kitchenQuestions[qIndex]);
  else if (placeIndex == 1) tft.println(bathroomQuestions[qIndex]);
  else if (placeIndex == 2) tft.println(hallQuestions[qIndex]);
}

void showSteps(int placeIndex, int taskIndex) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);

  if (placeIndex == 0) { // Kitchen
    if (taskIndex == 0) {
      tft.setCursor(10, 20);   tft.println("1. Take a cup & add coffee");
      tft.setCursor(10, 60);   tft.println("2. Add sugar");
      tft.setCursor(10, 100);  tft.println("3. Pour water/milk");
      tft.setCursor(10, 140);  tft.println("4. Stir slowly");
      tft.setCursor(10, 180);  tft.println("5. Drink carefully");
    } else if (taskIndex == 1) {
      tft.setCursor(10, 20);   tft.println("1. Take milk packet");
      tft.setCursor(10, 60);   tft.println("2. Pour in pan");
      tft.setCursor(10, 100);  tft.println("3. Put pan on stove");
      tft.setCursor(10, 140);  tft.println("4. Turn stove low");
      tft.setCursor(10, 180);  tft.println("5. Wait till it boils");
    } else if (taskIndex == 2) {
      tft.setCursor(10, 20);   tft.println("1. Take 2 slices");
      tft.setCursor(10, 60);   tft.println("2. Put in toaster");
      tft.setCursor(10, 100);  tft.println("3. Press lever");
      tft.setCursor(10, 140);  tft.println("4. Wait to pop");
      tft.setCursor(10, 180);  tft.println("5. Take a plate");
    }
  } else if (placeIndex == 1) { // Bathroom
    if (taskIndex == 0) {
      tft.setCursor(10, 20);   tft.println("1. Take toothpaste");
      tft.setCursor(10, 60);   tft.println("2. Apply onto brush");
      tft.setCursor(10, 100);  tft.println("3. Brush teeth");
      tft.setCursor(10, 140);  tft.println("4. Rinse mouth");
      tft.setCursor(10, 180);  tft.println("5. Spit & wash");
    } else if (taskIndex == 1) {
      tft.setCursor(10, 20);   tft.println("1. Splash water on face");
      tft.setCursor(10, 60);   tft.println("2. Apply facewash");
      tft.setCursor(10, 100);  tft.println("3. Massage gently");
      tft.setCursor(10, 140);  tft.println("4. Rinse & pat dry");
      tft.setCursor(10, 180);  tft.println("5. Apply moisturizer");
    } else if (taskIndex == 2) {
      tft.setCursor(10, 20);   tft.println("1. Adjust shower temperature");
      tft.setCursor(10, 60);   tft.println("2. Step into shower");
      tft.setCursor(10, 100);  tft.println("3. Use soap/shampoo");
      tft.setCursor(10, 140);  tft.println("4. Rinse thoroughly");
      tft.setCursor(10, 180);  tft.println("5. Step out and dry off");
    }
  } else if (placeIndex == 2) { // Hall
    if (taskIndex == 0) {
      tft.setCursor(10, 20);   tft.println("1. Sit on the couch");
      tft.setCursor(10, 60);   tft.println("2. Pick a channel");
      tft.setCursor(10, 100);  tft.println("3. Turn on TV");
      tft.setCursor(10, 140);  tft.println("4. Adjust volume");
      tft.setCursor(10, 180);  tft.println("5. Watch your show");
    } else if (taskIndex == 1) {
      tft.setCursor(10, 20);   tft.println("1. Find your playlist");
      tft.setCursor(10, 60);   tft.println("2. Press play");
      tft.setCursor(10, 100);  tft.println("3. Adjust volume");
      tft.setCursor(10, 140);  tft.println("4. Relax & enjoy music");
      tft.setCursor(10, 180);  tft.println("5. Take a break");
    } else if (taskIndex == 2) {
      tft.setCursor(10, 20);   tft.println("1. Find old album");
      tft.setCursor(10, 60);   tft.println("2. Browse photos");
      tft.setCursor(10, 100);  tft.println("3. Pick memories");
      tft.setCursor(10, 140);  tft.println("4. Relive moments");
      tft.setCursor(10, 180);  tft.println("5. Share with family");
    }
  }
}