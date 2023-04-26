#include <WiFi.h>
#include <BluetoothSerial.h>
#include <Arduino.h>
#include <FirebaseESP32.h>
// Provide the token generation process info.
#include <addons/TokenHelper.h>
// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>
// For the following credentials, see examples/Authentications/SignInAsUser/EmailPassword/EmailPassword.ino
#include <Preferences.h>
// Declare Preferences object
Preferences preferences;
//Preferences prefs;
/* 2. Define the API Key */
#define API_KEY "AIzaSyAKEou6JOvFfcgPt_G-W3cGXnGn-g8W82w"
/* 3. Define the RTDB URL */
#define DATABASE_URL "https://pyronnoia-280b1-default-rtdb.firebaseio.com/"  //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app
/* 4. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL "cHlyb25ub2lhZXNwMzJ0dGdvdGNhbGw@email.com"
#define USER_PASSWORD "a!PQc;*?6rnCdHRE"
#define WIFI_SSID "PLDTHOMEFIBR9K2X5"
#define WIFI_PASSWORD "PLDTWIFIYBJrQ"


#define SSID_MAX_LEN 32  // Maximum length of SSID (including null terminator)
#define PASS_MAX_LEN 64  // Maximum length of password (including null terminator)

// Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
WiFiServer server(80);
WiFiClient client;

// NOTE: For firebase, best to use millis instead of delay()
unsigned long sendDataPrevMillis = 0;
unsigned long count = 0;
bool isBluetoothInitialized = false;
unsigned long startTime = millis();
bool startconn = true;
// Define the analog pins for the sensors
#define MQ2_PIN 34
#define MQ135_PIN 14
// Define the pin for the IR flame sensor
#define FLAME_PIN 32
BluetoothSerial SerialBT;
const char* prevSavedSsid = "";
const char* prevSavedPassword = "";
const char* ssid = "";
const char* password = "";
const char* savedssid = "";
const char* savedpassword = "";

void firebaseSetup() {
  bool isConnectedtoFB = false;  // flag to keep track of connection status
  WiFi.begin(ssid, password);
  Serial.print("Firebase to Wi-Fi");

  startTime = millis();  // set the start time

  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startTime >= 10000) {  // connection timeout after 10 seconds
      Serial.println("Connection timed out.");
      return;  // exit the function if connection times out
    }
    delay(1000);
    Serial.print("!");
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback;  // see addons/TokenHelper.h
  Firebase.begin(&config, &auth);

  // Comment or pass false value when WiFi reconnection will control by your code or third party library
  Firebase.reconnectWiFi(true);
  Firebase.setDoubleDigits(5);
}
// Read the analog value from the MQ-2 sensor and convert it to PPM
float readMQ2() {
  int sensorValue = analogRead(MQ2_PIN);
  float voltage = sensorValue / 4095.0 * 3.3;
  float ppm = voltage / 0.1;
  return ppm;
}

// Read the analog value from the MQ-135 sensor and convert it to PPM
float readMQ135() {
  int sensorValue = analogRead(MQ135_PIN);
  float voltage = sensorValue / 4095.0 * 3.3;
  float ppm = 3.027 * pow(voltage, 3) - 21.714 * pow(voltage, 2) + 55.805 * voltage - 39.253;
  return ppm;
}

// Read the digital value from the IR flame sensor and return true if a flame is detected
bool readFlame() {
  bool flame = digitalRead(FLAME_PIN);
  if (flame == HIGH) {
    return 0;
  } else {
    return 1;
  }
}

void setup() {
  // Open the preferences file with a namespace of "wifi" and read-write access
  preferences.begin("wifi", true);

  // Load the saved SSID and password from preferences, or use default values
  ssid = preferences.getString("ssid").c_str();
  password = preferences.getString("password").c_str();
  preferences.end();
  Serial.begin(115200);
  //SerialBT.begin("PYRONNOIA");
  WiFi.mode(WIFI_STA);
  // Set the flame sensor's digital output pin as an input
  pinMode(FLAME_PIN, INPUT);
  pinMode(MQ2_PIN, INPUT);
  pinMode(MQ135_PIN, INPUT);
}

void loop() {

  if (WiFi.status() != WL_CONNECTED) {
    //loadWifiCredentials();
    if (startconn) {
      SerialBT.end();
      bool checkconnect = connectToWifi();
      if (checkconnect) {
        startconn = false;
      } else {
        startconn = false;
        Serial.println(" failed Connecting to Wi-Fi network");
      }

    } else {

      if (!isBluetoothInitialized) {
        SerialBT.begin("PYRONNOIA");
        server.stop();
        isBluetoothInitialized = true;
        if (!isBluetoothInitialized) {
          Serial.println("PYRONNOIA OFF");
        } else {
          Serial.println("PYRONNOIA ON");
        }
      }
      //else if (!SerialBT.connected()) {
      //    Serial.println("Waiting for Bluetooth connection...");
      //    delay(10000);

      //}
      else {
        bluetoothconnect();  // check for incoming messages
      }
    }



  } else {
    // Load saved WiFi credentials
    startconn = true;
    //isBluetoothInitialized = false;
    //Serial.println(ssid);
    //Serial.println(password);
    SerialBT.end();

    if (!Firebase.ready()) {

      firebaseSetup();

    } else {
      server.begin();
      client = server.available();
      if (client) {
        Serial.println("Client connected");
        while (client.connected()) {
          if (client.available()) {
            Serial.println("Sent response: Hello, client!");

            String message = client.readStringUntil('\r');
            Serial.println("entered wifi creds");
            int separatorIndex = message.indexOf(':');
            if (separatorIndex != -1) {
              String wifiName = message.substring(0, separatorIndex);
              String wifiPassword = message.substring(separatorIndex + 1);
              wifiName.trim();
              wifiPassword.trim();
              if (wifiName != "" && wifiPassword != "") {
                Serial.println("Received Wi-Fi credentials:");
                Serial.println("SSID: " + wifiName);
                Serial.println("Password: " + wifiPassword);
                ssid = wifiName.c_str();
                password = wifiPassword.c_str();
                bool connected = connectToWifi();
                if (!connected) {
                  Serial.println("Invalid Wi-Fi credentials received");
                } else {
                  Serial.println("VALID Wi-Fi credentials received");
                }
              } else {
                Serial.println("Invalid Wi-Fi credentials received");
              }
            } else {
              Serial.println("Invalid message received");
            }
          }
        }
        client.stop();
        Serial.println("Client disconnected");
      } else {
      }


      if (Firebase.ready() && (millis() - sendDataPrevMillis > 1000 || sendDataPrevMillis == 0)) {
        sendDataPrevMillis = millis();

        // Read the sensor values
        float mq2 = readMQ2();
        float mq135 = readMQ135();
        bool flame = readFlame();

        // Display the sensor values in the serial monitor
        Serial.print("MQ-2 Sensor PPM: ");
        Serial.println(mq2);
        Serial.print("MQ-135 Sensor PPM: ");
        Serial.println(mq135);
        Serial.print("Flame Detected: ");
        Serial.println(flame);

        // Upload to firebase
        Serial.printf("Set MQ-2 Sensor PPM... %s\n", Firebase.setFloat(fbdo, "/Sensor Data/" + std::string(auth.token.uid.c_str()) + "/mq2", mq2) ? "ok" : fbdo.errorReason().c_str());
        Serial.printf("Set MQ-135 Sensor PPM... %s\n", Firebase.setFloat(fbdo, "/Sensor Data/" + std::string(auth.token.uid.c_str()) + "/mq135", mq135) ? "ok" : fbdo.errorReason().c_str());
        Serial.printf("Set Flame Detected... %s\n", Firebase.setBool(fbdo, "/Sensor Data/" + std::string(auth.token.uid.c_str()) + "/flame", flame) ? "ok" : fbdo.errorReason().c_str());
        count++;
      } else {
        //Serial.println("Firebasenot ready");
      }
    }
  }
}

bool connectToWifi() {
  WiFi.disconnect();
  delay(1000);
  if (!startconn) {
    WiFi.begin(ssid, password);
  } else {
    loadWifiCredentials();
    WiFi.begin(ssid, password);
    Serial.println("USING SAVED CREDS");
    Serial.println(ssid);
    Serial.println(password);
  }
  Serial.println("Connecting to Wi-Fi network");
  startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startTime >= 10000) {  // connection timeout after 10 seconds
      return false;
    }
    delay(1000);
    Serial.print("!");
  }
  saveWiFiCredentials();
  return true;
}



void bluetoothconnect() {
  if (SerialBT.connected() && WiFi.status() != WL_CONNECTED) {
    Serial.println("Bluetooth is connected into serial.");
    delay(1000);
  }

  if (SerialBT.available() && SerialBT.connected() && WiFi.status() != WL_CONNECTED) {
    String message = SerialBT.readStringUntil('\n');
    Serial.println("entered wifi creds");
    int separatorIndex = message.indexOf(':');
    if (separatorIndex != -1) {
      String wifiName = message.substring(0, separatorIndex);
      String wifiPassword = message.substring(separatorIndex + 1);
      wifiName.trim();
      wifiPassword.trim();
      if (wifiName != "" && wifiPassword != "") {
        Serial.println("Received Wi-Fi credentials:");
        Serial.println("SSID: " + wifiName);
        Serial.println("Password: " + wifiPassword);
        ssid = wifiName.c_str();
        password = wifiPassword.c_str();
        bool connected = connectToWifi();
        if (!connected) {
          Serial.println("Invalid Wi-Fi credentials received");
        } else {
          Serial.println("VALID Wi-Fi credentials received");
        }
      } else {
        Serial.println("Invalid Wi-Fi credentials received");
      }
    } else {
      Serial.println("Invalid message received");
    }
  }
}



// Store last known WiFi credentials

void saveWiFiCredentials() {
  // Get the editor to modify preferences
  preferences.begin("wifi", false);
  Serial.println("const char creds before saving");
  Serial.println(ssid);
  Serial.println(password);
  // Store the SSID and password in preferences
  preferences.putString("ssid", String(ssid));
  preferences.putString("password", String(password));

  // Commit the changes
  preferences.end();
}




void loadWifiCredentials() {
  // Get the preferences for "wifi"
  preferences.begin("wifi", true);

  // Load the SSID and password from preferences
  ssid = preferences.getString("ssid").c_str();
  password = preferences.getString("password").c_str();
  Serial.println("const char creds loaded: ");
  Serial.println(ssid);
  Serial.println(password);
  // Serial.println(ssid);
  // Serial.println(password);
  // Close the preferences
  preferences.end();
}


/*void loadWiFiCredentials() {
  // Open the preferences file with a namespace of "wifi" and read-only access
  preferences.begin("wifi", true);

  // Load the saved SSID and password from preferences, or use default values
  savedssid = preferences.getString("ssid").c_str();
  savedpassword = preferences.getString("password").c_str();

  // Close the preferences file
  preferences.end();
  
}*/
