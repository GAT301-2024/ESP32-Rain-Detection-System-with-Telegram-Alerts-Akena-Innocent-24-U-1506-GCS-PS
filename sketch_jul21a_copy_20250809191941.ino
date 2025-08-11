#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

String ssid = "";
String password = "";

const char* telegramBotToken = "7687639557:AAHFJm14pSsASu_iV2GNt6ZaIqJ5GJK_KVA";
const char* chatId = "7341682268";

const int rainSensorPin = 34;
const int buzzerPin = 4;

bool alertEnabled = true;
bool isRaining = false;
bool messageSent = false;
int lastUpdateId = 0;
unsigned long lastMessageTime = 0;
int rainThreshold = 500;

void setup() {
  Serial.begin(115200);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  Serial.println("\nAvailable WiFi Networks:");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    Serial.printf("%d: %s (%d dBm)\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
  }

  while (true) {
    Serial.println("\nEnter your WiFi SSID:");
    while (ssid == "") {
      if (Serial.available()) {
        ssid = Serial.readStringUntil('\n');
        ssid.trim();
      }
    }

    Serial.println("Enter your WiFi Password:");
    while (password == "") {
      if (Serial.available()) {
        password = Serial.readStringUntil('\n');
        password.trim();
      }
    }

    Serial.printf("\nConnecting to %s...\n", ssid.c_str());
    WiFi.begin(ssid.c_str(), password.c_str());
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected to WiFi!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      break;
    } else {
      Serial.println("\nFailed to connect to WiFi");
      ssid = "";
      password = "";
      Serial.println("Do you want to skip internet setup and continue offline? (yes/no)");
      while (true) {
        if (Serial.available()) {
          String answer = Serial.readStringUntil('\n');
          answer.trim();
          answer.toLowerCase();
          if (answer == "yes") {
            Serial.println("Continuing without internet.");
            break;
          } else if (answer == "no") {
            Serial.println("Retrying WiFi setup.");
            break;
          } else {
            Serial.println("Invalid input. Type 'yes' or 'no':");
          }
        }
      }
      if (WiFi.status() != WL_CONNECTED && ssid != "") {
        break;
      }
    }
  }

  // Self-Test
  Serial.println("\nRunning Hardware Self-Test...");
  delay(3000);
  Serial.print("Rain sensor value: ");
  Serial.println(analogRead(rainSensorPin));
  Serial.println("Buzzing buzzer...");
  digitalWrite(buzzerPin, HIGH);
  delay(1000);
  digitalWrite(buzzerPin, LOW);
  Serial.println("Hardware test complete!\n");
}

void loop() {
  checkTelegramCommands();

  int sensorValue = analogRead(rainSensorPin);
  bool currentlyRaining = sensorValue < rainThreshold;

  if (currentlyRaining && alertEnabled) {
    if (!isRaining) {
      isRaining = true;
      messageSent = false;
      Serial.println("Rain detected!");
    }

    // Buzzing and Messaging
    digitalWrite(buzzerPin, HIGH);

    if (!messageSent || millis() - lastMessageTime >= 2000) {
      sendTelegramAlert("It just started raining!");
      lastMessageTime = millis();
      messageSent = true;
    }

  } else if (!currentlyRaining && isRaining) {
    isRaining = false;
    messageSent = false;
    digitalWrite(buzzerPin, LOW);
    sendTelegramAlert("The rain has stopped.");
    Serial.println("Rain stopped.");
  }

  if (!alertEnabled) {
    digitalWrite(buzzerPin, LOW);
  }

  delay(500); // smoother polling
}

void checkTelegramCommands() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(telegramBotToken) + "/getUpdates?offset=" + String(lastUpdateId + 1);
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getString());

    if (!error && doc["ok"]) {
      JsonArray result = doc["result"].as<JsonArray>();

      for (JsonObject messageObj : result) {
        lastUpdateId = messageObj["update_id"].as<int>();
        String text = messageObj["message"]["text"].as<String>();

        Serial.print("Received command: ");
        Serial.println(text);

        if (text == "/stop") {
          alertEnabled = false;
          digitalWrite(buzzerPin, LOW);
          sendTelegramAlert("Alerts & buzzer disabled.");
        } else if (text == "/start") {
          alertEnabled = true;
          messageSent = false;
          sendTelegramAlert("Alerts resumed.");
        } else if (text == "/status") {
          String statusMsg = "Status:\n";
          statusMsg += "Rain Sensor: " + String(analogRead(rainSensorPin)) + "\n";
          statusMsg += "Rain Detected: " + String(isRaining ? "Yes" : "No") + "\n";
          statusMsg += "Alerts: " + String(alertEnabled ? "Enabled" : "Disabled");
          sendTelegramAlert(statusMsg);
        }
      }
    }
  }
  http.end();
}

void sendTelegramAlert(String msg) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Skipped Telegram alert: No internet.");
    return;
  }

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(telegramBotToken) + "/sendMessage";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  JsonDocument doc;
  doc["chat_id"] = chatId;
  doc["text"] = msg;

  String requestBody;
  serializeJson(doc, requestBody);

  int httpCode = http.POST(requestBody);
  if (httpCode > 0) {
    Serial.println("Message sent to Telegram.");
  } else {
    Serial.println("Failed to send Telegram message.");
  }
  http.end();
}