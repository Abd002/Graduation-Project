#include <WiFi.h>
#include <PubSubClient.h>
#include <Firebase_ESP_Client.h>
#include <LittleFS.h>
#include <addons/SDHelper.h>
#include <addons/TokenHelper.h>
#include <EEPROM.h>

#define MAX_DATA_LENGTH 256

typedef struct {
  uint8_t sid;
  uint8_t SubFn;
  uint16_t DID;
  uint8_t data[MAX_DATA_LENGTH];
} UDS_SendRequestTypeDef;
UDS_SendRequestTypeDef message, response;

void UDS_SendResponse(uint8_t* response, uint16_t len) {
  Serial.println("[UDS] Sending Response...");
  for (int i = 0; i < len; i++) {
    while (!Serial1.available());
    Serial1.read();
    Serial1.write(response[i]);
  }
  Serial.println("[UDS] Response Sent.");
}

UDS_SendRequestTypeDef UDS_SendRequest(UDS_SendRequestTypeDef* message) {
  Serial.println("[UDS] Sending Request...");
  uint8_t data[MAX_DATA_LENGTH + 4] = {0};

  data[0] = message->sid;
  data[1] = message->SubFn;
  *((uint16_t*)&data[2]) = message->DID;
  for (int i = 4; i < MAX_DATA_LENGTH + 4; i++) {
    data[i] = message->data[i - 4];
  }

  UDS_SendResponse(data, message->DID + 4);

  uint8_t RxData[100] = {};
  Serial.println("[UDS] Waiting for response...");
  while (!Serial1.available());
  RxData[0] = Serial1.read();
  if (RxData[0] == message->sid + 0x40) {
    for (int i = 1; i < 4 + *((uint16_t*)&RxData[2]); i++) {
      while (!Serial1.available());
      RxData[i] = Serial1.read();
    }
  }

  UDS_SendRequestTypeDef ret = {RxData[0], RxData[1], *((uint16_t*)&RxData[2])};
  for (int i = 4; i < 12; i++) {
    ret.data[i - 4] = RxData[i];
  }
  Serial.println("[UDS] Request Completed.");
  return ret;
}

int readFile() {
  Serial.println("[File] Reading file from LittleFS...");
  File file = LittleFS.open("/8008000.bin", "r");
  if (!file) {
    Serial.println("[File] Failed to open file.");
    return 0;
  }

  while (file.available()) {
    message.sid = 0x36;
    message.SubFn = 0x00;
    message.DID = MAX_DATA_LENGTH;
    for (int i = 0; i < MAX_DATA_LENGTH; i++) {
      if (!file.available()) {
        break;
      }
      byte data = file.read();
      message.data[i] = data;
    }
    response = UDS_SendRequest(&message);
    if (response.sid != 0x76) {
      Serial.println("[File] Error in UDS response.");
      file.close();
      return 0;
    }
  }

  file.close();
  Serial.println("[File] File processed successfully.");
  return 1;
}

// Remaining code sections omitted for brevity, but similar logging can be added.
#define EEPROM_SIZE 4
#define EEPROM_VERSION_ADDRESS 0

#define NUM_BYTES 8
#define SYNC_CODE 0xAA
#define ACK_CODE  0x55

//WIFI section
const char* ssid = "Abd0002";
const char* password = "";
void  connectToWIFI() {
  Serial.println();
  Serial.print("[WiFi] Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int tryDelay = 500;
  int numberOfTries = 20;

  while (true) {
    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL: Serial.println("[WiFi] SSID not found"); break;
      case WL_CONNECT_FAILED:
        Serial.print("[WiFi] Failed - WiFi not connected! Reason: ");
        return;
        break;
      case WL_CONNECTION_LOST: Serial.println("[WiFi] Connection was lost"); break;
      case WL_SCAN_COMPLETED:  Serial.println("[WiFi] Scan is completed"); break;
      case WL_DISCONNECTED:    Serial.println("[WiFi] WiFi is disconnected"); break;
      case WL_CONNECTED:
        Serial.println("[WiFi] WiFi is connected!");
        Serial.print("[WiFi] IP address: ");
        Serial.println(WiFi.localIP());
        return;
        break;
      default:
        Serial.print("[WiFi] WiFi Status: ");
        Serial.println(WiFi.status());
        break;
    }
    delay(tryDelay);

    if (numberOfTries <= 0) {
      Serial.print("[WiFi] Failed to connect to WiFi!");
      // Use disconnect function to force stop trying to connect
      WiFi.disconnect();
      return;
    }
    else {
      numberOfTries--;
    }
  }
}

int SERVER_Version = -1;

// emqx MQTT settings
const char* mqttServer = "broker.emqx.io";
const int mqttPort = 1883;
WiFiClient espClient;
PubSubClient client(espClient);
void callback(char* topic, byte* message, unsigned int length) {
  // Check if the message is from the specific topic you want
  if (strcmp(topic, "okay/abd002") == 0) {

    char messageBuffer[length + 1];
    for (unsigned int i = 0; i < length; i++) {
      messageBuffer[i] = (char)message[i];
    }
    messageBuffer[length] = '\0';
    SERVER_Version = atoi(messageBuffer);

    Serial.print("Message received on your specific topic: ");
    for (int i = 0; i < length; i++) {
      Serial.print((char)message[i]);
    }
    Serial.println();
  }
  else {
    // Handle messages from other topics, if needed
    Serial.print("Message received on topic: ");
    Serial.print(topic);
    Serial.print(". Message: ");
    for (int i = 0; i < length; i++) {
      Serial.print((char)message[i]);
    }
    Serial.println();
  }
}
void connectToMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");

    if (client.connect("Abd002ESP32")) { // Client ID can be any unique name
      Serial.println("connected");
      client.subscribe("okay/abd002"); // Subscribe to the topic after connecting
    }
    else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
}
void getCodeVersion() {
  SERVER_Version = -1;
  while (SERVER_Version == -1) {
    if (!client.connected()) {
      connectToMQTT();
    }
    client.loop();
  }
}


#define API_KEY ""
#define USER_EMAIL "abdelrahman.5alifa@gmail.com"
#define USER_PASSWORD ""
#define STORAGE_BUCKET_ID "learning-fota.appspot.com"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void connectToFirebase() {
  // Firebase configuration
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;
  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
  config.fcs.download_buffer_size = 2048;
  Firebase.begin(&config, &auth);

  while (!Firebase.ready()) {
    Serial.println("Waiting for Firebase connection...");
    delay(1000);  // Retry every 1 second
  }
}
void downloadFileFromFirebase(const char* remoteFilePath) {
  String localFilePath = "/8008000.bin";
  if (Firebase.Storage.download(&fbdo, STORAGE_BUCKET_ID, remoteFilePath, localFilePath, mem_storage_type_flash, fcsDownloadCallback)) {
    Serial.println("File downloaded successfully");

    EEPROM.put(EEPROM_VERSION_ADDRESS, SERVER_Version);
    EEPROM.commit();


  }
  else {
    Serial.println("Download failed");
    Serial.println(fbdo.errorReason());
  }
}
void fcsDownloadCallback(FCS_DownloadStatusInfo info) {
  if (info.status == firebase_fcs_download_status_download) {
    Serial.printf("Downloaded %d%%, Elapsed time %d ms\n", (int)info.progress, info.elapsedTime);
  }
  else if (info.status == firebase_fcs_download_status_complete) {
    Serial.println("Download completed\n");
  }
  else if (info.status == firebase_fcs_download_status_error) {
    Serial.printf("Download failed: %s\n", info.errorMsg.c_str());
  }
}
void downloadFiles() {
  downloadFileFromFirebase("blink_exe.bin");
}

int old_version = 5;

void setup() {
  Serial.begin(115200);
  delay(10);

  connectToWIFI();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback); // Set callback function to handle incoming messages
  
  if (!LittleFS.begin(true)) {
    return;
  }
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("failed to initialize EEPROM");
    delay(1000000);
  }
  connectToFirebase();

  
  Serial1.begin(115200, SERIAL_8N1, 16, 17);
  Serial1.write(0xFF);
}

// Example logging for `loop` function:
void loop() {
  //if (Serial1.available() > 0) {
    //uint8_t receivedByte = Serial1.read();
    //if (receivedByte == 0xAA) {
      Serial.println("[Loop] Sync byte received, starting firmware update...");

      getCodeVersion();
      int readValue;
      EEPROM.get(EEPROM_VERSION_ADDRESS, readValue);
      if (readValue != SERVER_Version) {
        Serial.println("[Loop] EEPROM version mismatch. Downloading new files...");
        downloadFiles();
      } else {
        Serial.println("[Loop] EEPROM version matches server version.");
      }

      EEPROM.get(EEPROM_VERSION_ADDRESS, readValue);
      int stmVersion = -1;

      message.sid = 0x22;
      message.SubFn = 0x00;
      message.DID = 0;

      response = UDS_SendRequest(&message);

      if (response.sid == 0x62) {
        stmVersion = *((int*)&response.data[0]);
        Serial.printf("[Loop] STM32 version: %d\n", stmVersion);

        if (stmVersion != readValue) {
          Serial.println("[Loop] Updating STM32 firmware...");
          message.sid = 0x34;
          message.SubFn = 0x00;
          message.DID = 6;
          *((int*)&message.data[0]) = 0x08008000;
          *((uint16_t*)&message.data[4]) = readValue;

          response = UDS_SendRequest(&message);

          if (response.sid == 0x74) {
            if (readFile()) {
              Serial.println("[Loop] File read successful. Finalizing update...");
              message.sid = 0x37;
              message.SubFn = 0x00;
              message.DID = 0;

              response = UDS_SendRequest(&message);

              if (response.sid == 0x77) {
                Serial.println("[Loop] Update completed. Resetting device...");
                message.sid = 0x11;
                message.SubFn = 0x01;
                message.DID = 0;
                response = UDS_SendRequest(&message);
              }
            } else {
              Serial.println("[Loop] File read failed.");
            }
          } else {
            Serial.println("[Loop] Flash memory write failed.");
          }
        } else {
          Serial.println("[Loop] STM32 firmware is up-to-date.");
          Serial.println("Resetting device...");
          message.sid = 0x11;
          message.SubFn = 0x01;
          message.DID = 0;
          response = UDS_SendRequest(&message);
        }
      } else {
        Serial.println("[Loop] Failed to read STM32 version.");
      }
    //}
  //}
}
