#include <WiFi.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include <ezButton.h>
#include <SPI.h>
#include <MFRC522.h>
#include <esp_adc_cal.h>

#define EEPROM_SIZE 2048
#define LED_RFID_STATUS 26
ezButton limitSwitch(15);  // create ezButton object that attach to pin 7;

const char* ssid = "RFID_Box";
const char* password = "teksol-rfid";
const String address = "http://192.168.4.1";

const String BOX_ID = "1001";
const String BOX_Version = "0.1";

const uint8_t LED_BUILTIN = 2;
unsigned long timer = 0;

// define RFC522 SDA Pin
const uint8_t SS1_PIN = 12;
const uint8_t SS2_PIN = 13;
const uint8_t SS3_PIN = 14;
const uint8_t SS4_PIN = 27;
const uint8_t RST_PIN = 5;
char rfid_ID_details[4][9] = {"A160B71D","91D4D51D","630A2234","63523334"};
const uint8_t NUMBER_OF_READERS = 4;
const byte ssPins[] = {SS1_PIN, SS2_PIN, SS3_PIN, SS4_PIN};
int rfid_pass_status[] = {2,2,2,2};
int led_success=1;

const int inputPin = 4;
volatile bool buttonPressed = false;
int currentButtonState = 0;

unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

int counter = 0;
int buttonState = 0;
int lastButtonState = 0;

MFRC522 mfrc522[NUMBER_OF_READERS];

// const int CHRG_PIN = 35;
const int LED_PIN  = 34;

unsigned long wait_period = 1000; 
unsigned long wait_last_time = 0;
unsigned long ON_INTERVAL = 500;
unsigned long OFF_INTERVAL_HIGH = 5000;
unsigned long OFF_INTERVAL_LOW = 1000;
unsigned long previousMillis = 0;

bool ledState = false;
bool is_plugged_in = false;
bool percentage = 0;

typedef struct dataPayload {
  unsigned long timestamp;
  uint16_t tagId1;
  uint16_t tagId2;
  uint16_t tagId3;
  uint16_t tagId4;
  float battery;
} Payload;

void initWiFi(){
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi..");
  timer = millis();
  while (WiFi.status() != WL_CONNECTED && (millis()-timer < 10000)) {
    Serial.print(".");
    digitalWrite(LED_BUILTIN, HIGH);
    delay(300);
    digitalWrite(LED_BUILTIN, LOW);
    delay(300);
  }
  timer = 0;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connection Failed.");
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    Serial.println("Connected.");
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void initRFID(){
  Serial.println("Initial RFID...");
  for (uint8_t reader = 0 ; reader < NUMBER_OF_READERS ; reader++) {
    mfrc522[reader].PCD_Init(ssPins[reader], RST_PIN);
    Serial.printf("Reader %d: ",reader);
    mfrc522[reader].PCD_DumpVersionToSerial();
    delay(200);
  }
}

String dump_byte_array(byte *buffer, byte bufferSize) {
  byte Data[bufferSize]; // Declare an array to collect the byte values
  
  for (byte i = 0; i < bufferSize; i++) {
    Data[i] = buffer[i]; // Copy the byte value into the Data array
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
  Serial.println();

  // Convert the Data array to a character array
  char DataChar[bufferSize * 2 + 1]; // Each byte takes two characters in the string
  for (byte i = 0; i < bufferSize; i++) {
    sprintf(&DataChar[i * 2], "%02X", Data[i]); // Convert each byte to a two-digit hexadecimal string
  }
  DataChar[bufferSize * 2] = '\0'; // Add null terminator to end of string

  String RFID_ID = String(DataChar); // Convert the character array to a String variable

  Serial.print("Data as string: ");
  Serial.println(RFID_ID);
  return RFID_ID;
}

// TODO: Receive Payload and Send with Payload value
void sendPostData(){
  bool sendFlag = 1;
  String data = sendFlag ? "1" : "0";

  HTTPClient http;
  http.begin(address + "/post");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String payload = "id=" + BOX_ID + "&version=" + BOX_Version + "&data=" + data;
  int httpCode = http.POST(payload);
  if (httpCode > 0) {
    String response = http.getString();
    Serial.println(httpCode);
    Serial.println(response);
  } else {
    Serial.println("Error on HTTP request");
  }
  http.end();
}

void getRFID(){
      for (uint8_t reader = 0 ; reader < NUMBER_OF_READERS ; reader++ ) {
        mfrc522[reader].PICC_IsNewCardPresent();
        mfrc522[reader].PICC_ReadCardSerial();
        Serial.print("Reader");
        Serial.print("[");
        Serial.print(reader);
        Serial.print("]");
        Serial.println(dump_byte_array(mfrc522[reader].uid.uidByte, mfrc522[reader].uid.size));
        mfrc522[reader].PICC_HaltA();
        mfrc522[reader].PCD_StopCrypto1();  
        delay(1000);
  }
}

// int RFID_reader(){
//   int slot;
//       for (uint8_t reader = 0 ; reader < NUMBER_OF_READERS ; reader++ ) {
//       if (mfrc522[reader].PICC_IsNewCardPresent() && mfrc522[reader].PICC_ReadCardSerial()) {
//       if(reader==0){
//         slot = 0;
//       }
//       if(reader==1){
//         slot = 1;
//       }
//     }
//   }
//   return slot;
// }

// void BatteryManager()
// {
//   // Read ADC value
//   uint16_t raw_value = adc1_get_raw(ADC1_CHANNEL_0);
//   float voltage = raw_value * (3.3 / (float)(4095));    // max 3.3 or 3.6

//   int percentage = map(voltage * 100, 2.0 * 100, 3.3 * 100, 0, 100);
  
//   wait_last_time = millis(); 
  
//   bool is_plugged_in = digitalRead(CHRG_PIN);
//   Serial.print(is_plugged_in);

//   // Blink LED Condition
//   if (is_plugged_in == 1) {
//     unsigned long currentMillis = millis();
//     if (percentage <= 10) {
//       if (currentMillis - previousMillis >= OFF_INTERVAL_LOW && ledState == false) {
//         previousMillis = currentMillis;
//         ledState = true;
//         digitalWrite(LED_PIN, ledState);
//       }
//       else if (currentMillis - previousMillis >= ON_INTERVAL && ledState == true) {
//         previousMillis = currentMillis;
//         ledState = false;
//         digitalWrite(LED_PIN, ledState);
//       }
//     }
//     else if(percentage > 10) {
//       if ((currentMillis - previousMillis >= OFF_INTERVAL_HIGH) && (ledState == false)) {
//         previousMillis = currentMillis;
//         ledState = true;
//         digitalWrite(LED_PIN, ledState);
//       }
//       else if ((currentMillis - previousMillis >= ON_INTERVAL) && (ledState == true)) {
//         previousMillis = currentMillis;
//         ledState = false;
//         digitalWrite(LED_PIN, ledState);
//       }
//     }
    
//   }
// }

//--------------------------------------------------------------------------//
int writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);

  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }

  return addrOffset + 1 + len;
  EEPROM.commit();
}

int readStringFromEEPROM(int addrOffset, String *strToRead)
{
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen + 1];

  for (int i = 0; i < newStrLen; i++)
  {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0'; 

  *strToRead = String(data);
  return addrOffset + 1 + newStrLen;
}
//--------------------------------------------------------------------------//

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_RFID_STATUS, OUTPUT);
  limitSwitch.setDebounceTime(50); // set debounce time to 50 milliseconds

  // pinMode(CHRG_PIN, INPUT);
  // pinMode(LED_PIN , OUTPUT);

  // Initialize ADC
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);

  // initWiFi();
  SPI.begin();
  initRFID();

}
void CheckRFID(){
currentButtonState = digitalRead(inputPin);

if (currentButtonState != lastButtonState) {
  lastDebounceTime = millis();
}

if ((millis() - lastDebounceTime) > debounceDelay) {
  if (currentButtonState != buttonState) {
    buttonState = currentButtonState;
    if (buttonState == LOW) {
      EEPROM.begin(EEPROM_SIZE);

      String RFID_ID[NUMBER_OF_READERS];
      int eepromOffset = 0;

      for (uint8_t reader = 0 ; reader < NUMBER_OF_READERS ; reader++ ) {
        if (mfrc522[reader].PICC_IsNewCardPresent() && mfrc522[reader].PICC_ReadCardSerial()) {
          int rfidAddrOffset[reader] = {0};
          int newrfidAddrOffset[reader] = {0};
          Serial.print("reader");
          Serial.println(reader);
          RFID_ID[reader] = dump_byte_array(mfrc522[reader].uid.uidByte, mfrc522[reader].uid.size);
          String New_RFID_ID;
          if(reader == 0){
            rfidAddrOffset[reader] = writeStringToEEPROM(eepromOffset, RFID_ID[reader]);
            newrfidAddrOffset[reader] = readStringFromEEPROM(eepromOffset, &New_RFID_ID);
          }
          else{
            rfidAddrOffset[reader] = writeStringToEEPROM(rfidAddrOffset[reader-1], RFID_ID[reader]);  
            newrfidAddrOffset[reader] = readStringFromEEPROM(newrfidAddrOffset[reader-1], &New_RFID_ID);            
          }
          Serial.print("Saved RFID_ID_");
          Serial.print(reader + 1);
          Serial.print(": ");
          Serial.println(New_RFID_ID);
        }
      }

      Serial.println();
      Serial.println("|----Save Status----|");
      Serial.print("RFID_ID_1: ");     
      Serial.println(RFID_ID[0]);
      Serial.print("RFID_ID_2: ");
      Serial.println(RFID_ID[1]);
      Serial.print("RFID_ID_3: ");
      Serial.println(RFID_ID[2]);
      Serial.print("RFID_ID_4: ");
      Serial.println(RFID_ID[3]);
      EEPROM.end();
      for(int reader = 0 ; reader < NUMBER_OF_READERS ; reader++ ){
        Serial.print("Check[");
        Serial.print(reader);
        Serial.print("] = ");
        Serial.println(rfid_pass_status[reader]);
        if(RFID_ID[reader] == rfid_ID_details[reader]){
            rfid_pass_status[reader] = 1;    // Example --> 1st slot to drone
        }
        else if(RFID_ID[reader] == ""){
            rfid_pass_status[reader] = 0;
        }
      }

      for(int reader = 0 ; reader < NUMBER_OF_READERS ; reader++ ){ 
        if(rfid_pass_status[reader] == 0){
            led_success = 0;
            break;
        }
        else{
            led_success = 1;
         }
        } 

        if(led_success == 1){
            digitalWrite(LED_RFID_STATUS, HIGH);
        }
    }
  }
}
lastButtonState = currentButtonState;
}

void loop() {
  // sendPostData();
  // Serial.println("Reading RFID..");
  limitSwitch.loop(); // MUST call the loop() function first

  // if(limitSwitch.isPressed())
    // Serial.println("The limit switch: UNTOUCHED -> TOUCHED");

  // if(limitSwitch.isReleased()){
  //   // Serial.println("The limit switch: TOUCHED -> UNTOUCHED");
  //   SPI.begin();
  //   initRFID();
  // }

  int state = limitSwitch.getState();
  if(state == HIGH){
    // Serial.println("The limit switch: UNTOUCHED");
    getRFID();
  }
  else{
    // Serial.println("The limit switch: TOUCHED");
  }
  // delay(3000);
  // BatteryManager();
  CheckRFID();

//--------------------------------------------------------------------//
}
//--------------------------------------------------------------------//