#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

// Network credentials
const char* ssid = "lovewifi";
const char* password = "lovesoni..";

// RFID pins
#define RST_PIN D3
#define SS_PIN D4

// LED and buzzer pins
#define GREEN_LED_PIN D1
#define RED_LED_PIN D2
#define BUZZER_PIN D8

// Google Script ID - FIXED FORMAT
const char* scriptId = "AKfycbwqce1EOTo5MZfCO3tEYxzK4VYzASCsWLWzifirGav9KSBS7iPko3P4W7PFsW5EqhtC";

// Authorized UIDs and corresponding names
struct AuthorizedUser {
  byte uid[4];
  const char* name;
};

AuthorizedUser authorizedUsers[] = {
  {{0x63, 0x6F, 0xBB, 0x2D}, "Love Soni"},
  // Add more users here
};

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;
WiFiClientSecure client;

void setup() {
  Serial.begin(115200);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  SPI.begin();
  rfid.PCD_Init();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  client.setInsecure();
  Serial.println("RFID Access Control System Ready");
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  Serial.print("UID tag: ");
  String content = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    content += String(rfid.uid.uidByte[i], HEX) + " ";
  }
  content.toUpperCase();
  Serial.println(content);

  bool isAuthorized = false;
  const char* userName = "Unauthorized User";

  for (int i = 0; i < sizeof(authorizedUsers) / sizeof(authorizedUsers[0]); i++) {
    if (compareUID(rfid.uid.uidByte, authorizedUsers[i].uid)) {
      isAuthorized = true;
      userName = authorizedUsers[i].name;
      break;
    }
  }

  if (isAuthorized) {
    Serial.println("Access granted");
    grantAccess();
    logAccess("Access Granted", content, userName);
  } else {
    Serial.println("Access denied");
    denyAccess();
    logAccess("Access Denied", content, userName);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(1000);
}

bool compareUID(byte readUID[], byte storedUID[]) {
  for (int i = 0; i < 4; i++) {
    if (readUID[i] != storedUID[i]) return false;
  }
  return true;
}

void grantAccess() {
  digitalWrite(GREEN_LED_PIN, HIGH);
  delay(2000);
  digitalWrite(GREEN_LED_PIN, LOW);
}

void denyAccess() {
  digitalWrite(RED_LED_PIN, HIGH);
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
  delay(1000);
  digitalWrite(RED_LED_PIN, LOW);
}

void logAccess(String status, String uid, String name) {
  if (WiFi.status() == WL_CONNECTED) {
    String url = "https://script.google.com/macros/s/" + String(scriptId) + "/exec";
    url += "?status=" + urlEncode(status);
    url += "&uid=" + urlEncode(uid);
    url += "&name=" + urlEncode(name);
    url += "&timestamp=" + String(millis());

    Serial.println("Posting to Google Sheets...");
    Serial.println(url);

    HTTPClient http;
    http.begin(client, url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      Serial.println("HTTP Response: " + http.getString());
    } else {
      Serial.println("Error on HTTP request");
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

String urlEncode(String str) {
  String encodedString = "";
  char c, code0, code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += '+';
    } else if (isAlphaNumeric(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}
