#include <Arduino.h>
#include <M5StickC.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "BLE2902.h"
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include "settings.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include "AXP192.h"

void displayBatt(void);
void menuHandler(void);
void yicameraHandler(void);
void irHanlder(void);
void btHandler(void);

void transmitIR(void);
void searchCamera();
void connectToCamera();
void RecordOFF(String token);
String requestToken();
void TakePhoto(String token);
void RecordON(String token);
void RecordOFF(String token);

void taskServer(void*);

#define ir_send_pin G9

/*
0- menu
1- Yi
2- BT
3- IR
*/
uint8_t currFunc = 0;
uint8_t menuFunc = 1;
//redraw screen for first
bool redraw = true;
bool firstRun = true;

WiFiClient client;
String YI_SSID;
bool RecON = false;

BLEHIDDevice* hid;
BLECharacteristic* input;
BLECharacteristic* output;

bool connected = false;

TFT_eSprite battSprite = TFT_eSprite(&M5.Lcd); 
TFT_eSprite screenSprite = TFT_eSprite(&M5.Lcd); 

void setup() {
  //Debug serial port
  M5.begin();
  M5.IMU.Init();
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Axp.ScreenBreath(9);
  
  battSprite.createSprite(160, 10);
  battSprite.setRotation(3);
  
  screenSprite.createSprite(160, 70);
  screenSprite.setRotation(3);

  M5.Axp.EnableCoulombcounter();
  
  pinMode(ir_send_pin, OUTPUT);
  digitalWrite(ir_send_pin, HIGH);
}

void displayBatt(void) {
  static unsigned long lastRefreshTime;
  if(millis() - lastRefreshTime >= 2000 || lastRefreshTime == 0) {
    lastRefreshTime = millis();
    battSprite.fillSprite(BLACK);
    battSprite.setCursor(0, 0, 1);
    battSprite.printf("V:%.3fv I:%.3fma", M5.Axp.GetBatVoltage(), M5.Axp.GetBatCurrent());
    battSprite.pushSprite(0, 0);
  }
}

void menuHandler(void) {
  if (M5.BtnB.pressedFor(20)) {
    if(menuFunc<3)
      menuFunc++;
    else
      menuFunc=1;

    redraw=true;      
  } else if (M5.BtnA.pressedFor(20)) {
    currFunc = menuFunc;
    firstRun = true;
  }

  if(redraw) {
    redraw = false;
    screenSprite.fillSprite(BLACK);
    screenSprite.setCursor(0, 20);
    screenSprite.setTextColor(TFT_DARKCYAN);
    screenSprite.setTextSize(2);
    screenSprite.setTextFont(1);  
    switch(menuFunc) {
      case 1: screenSprite.print("Yi Camera"); break;
      case 2: screenSprite.print("BT Remote"); break;
      case 3: screenSprite.print("IR Remote"); break;
      default:break;
    }    
    screenSprite.pushSprite(0, 10);
    delay(200);
  }
}

void yicameraHandler(void) {
  if(firstRun) {      
    screenSprite.fillSprite(BLACK);
    screenSprite.setCursor(0, 20);
    screenSprite.setTextColor(TFT_DARKCYAN);
    screenSprite.setTextSize(1);
    screenSprite.setTextFont(1);  
    screenSprite.println("Yi Camera");
    screenSprite.pushSprite(0, 10);
    searchCamera();
    connectToCamera();
    firstRun = false;
    delay(1000);
  }

  if (M5.BtnA.pressedFor(20)) {
    String token = requestToken();
    if (token.length() != 0) {
      TakePhoto(token);
    }
  }
  
  if (M5.BtnB.pressedFor(20)) {
    String token = requestToken();    
    if (token.length() != 0) {
      if (RecON) {
        RecordOFF(token);
        screenSprite.fillCircle(110, 50, 10, BLACK);        
        screenSprite.pushSprite(0, 10);
        RecON = false;
      } else {
        RecordON(token);        
        screenSprite.fillCircle(110, 50, 10, RED);
        screenSprite.pushSprite(0, 10);
        RecON = true;
      }
    }
  }
}

void irHandler(void) {
  if(firstRun) {      
    screenSprite.fillSprite(BLACK);
    screenSprite.setCursor(0, 20);
    screenSprite.setTextColor(TFT_DARKCYAN);
    screenSprite.setTextSize(1);
    screenSprite.setTextFont(1);  
    screenSprite.println("IR Remote");
    screenSprite.pushSprite(0, 10);
    firstRun = false;
    delay(1000);
  }

  if (M5.BtnA.pressedFor(20)) {
    transmitIR();
    delay(100);
  }
}

void btHandler(void) {
  static bool lastConn = false;
  if(firstRun) {
    xTaskCreate(taskServer, "server", 20000, NULL, 5, NULL);    
    screenSprite.fillSprite(BLACK);
    screenSprite.setCursor(0, 20);
    screenSprite.setTextColor(TFT_DARKCYAN);
    screenSprite.setTextSize(1);
    screenSprite.setTextFont(1);  
    screenSprite.println("BT Remote");
    screenSprite.pushSprite(0, 10);
    firstRun=false;
    delay(1000);
  }

  if (lastConn != connected) {
    lastConn = connected;
    screenSprite.println("Connected!");
    screenSprite.pushSprite(0, 10);
  }

  if (connected & M5.BtnA.pressedFor(20)) {
    //Key press
    uint8_t msg[] = {0x0, 0x0, __SEND_KEY, 0x0, 0x0, 0x0, 0x0, 0x0};
    input->setValue(msg, sizeof(msg));
    input->notify();

    //Key release
    uint8_t msg1[] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
    input->setValue(msg1, sizeof(msg1));
    input->notify();

    delay(1000);
  }
}

void transmitIR(void) {
  uint8_t i;
  for (i = 0; i < 76; i++) {
  digitalWrite(ir_send_pin, LOW);
  delayMicroseconds(7);
  digitalWrite(ir_send_pin, HIGH);
  delayMicroseconds(7);
  }
  delay(27);
  delayMicroseconds(810);
  for (i = 0; i < 16; i++) {
  digitalWrite(ir_send_pin, LOW);
  delayMicroseconds(7);
  digitalWrite(ir_send_pin, HIGH);
  delayMicroseconds(7);
  }
  delayMicroseconds(1540);
  for (i = 0; i < 16; i++) {
  digitalWrite(ir_send_pin, LOW);
  delayMicroseconds(7);
  digitalWrite(ir_send_pin, HIGH);
  delayMicroseconds(7);
  }
  delayMicroseconds(3545);
  for (i = 0; i < 16; i++) {
  digitalWrite(ir_send_pin, LOW);
  delayMicroseconds(7);
  digitalWrite(ir_send_pin, HIGH);
  delayMicroseconds(7);
  }
  
  digitalWrite(ir_send_pin, HIGH);
}

void searchCamera() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(10);
  int cnt = WiFi.scanNetworks();
  Serial.print("Networks: ");
  if (cnt > 0) {
    for (int i = 0; i < cnt; ++i) {
      Serial.print(WiFi.SSID(i) + ",");
      if (WiFi.SSID(i).startsWith("YDXJ_")) {
        YI_SSID = WiFi.SSID(i);
        break;
      }
    }
  }
  Serial.println();  
  
  screenSprite.fillSprite(BLACK);
  screenSprite.setCursor(0, 20);
  screenSprite.setTextColor(TFT_DARKCYAN);
  screenSprite.setTextSize(1);
  screenSprite.setTextFont(1);  
  screenSprite.println(YI_SSID);
  screenSprite.pushSprite(0, 10);
  //screenSprite.pushSprite(0, 10);
}

void connectToCamera() {
  bool result = true;
  short retry = 30;
  const int jsonPort = 7878;
  char password[11] = "1234567890";
  char ssid[30];
  Serial.print("Con: ");
  YI_SSID.toCharArray(ssid, YI_SSID.length() + 1);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    if (retry == 0) {
      result = false;
      break;
    }
    delay(500);
    retry--;
  }
  Serial.print(" -> wifi con:");
  if (result == true) Serial.print("OK "); else Serial.print("XX ");

  if (!client.connect("192.168.42.1", jsonPort)) result = false;
  Serial.print(" IP con:");
  if (result == true) 
  {
    Serial.print("OK.");    
    screenSprite.println("Connected!");
    screenSprite.pushSprite(0, 10);
  } 
  else
  {
     Serial.print("XX.");
  }
  Serial.println();
}

String requestToken() {
  String token;
  // This will send the request token msg to the server
  client.print("{\"msg_id\":257,\"token\":0}\n\r");
  //delay(1000);
  yield(); delay(250); yield(); delay(250); yield(); delay(250); yield(); delay(250);
  // Read all the lines of the reply from server and print them to Serial
  String response;
  while (client.available()) {
    char character = client.read();
    response.concat(character);
  }
  // Search token in to the stream
  int offset = response.lastIndexOf("\"param\":");
  if (offset != -1) {
    for (int i = offset + 8; i < response.length(); ++i) {
      if ((response.charAt(i) != ',')) {
        token.concat(response.charAt(i));
      } else
      {
        break;
      }      
    }
  }
  Serial.println(response);
  Serial.println(token);
  return token;
}

void TakePhoto(String token) {
  if (RecON) {
    RecordOFF(token);
    //RecON = false;
    String token = requestToken();
  }
  client.print("{\"msg_id\":769,\"token\":");
  client.print(token);
  client.print("}\n\r");
  Serial.print("{\"msg_id\":769,\"token\":");
  Serial.print(token);
  Serial.print("}\n\r");
  Serial.print("Photo - Response: ");
  yield(); delay(250); yield(); delay(250); yield(); delay(250); yield(); delay(250);
  String response;
  while (client.available()) {
    char character = client.read();
    response.concat(character);
  }
  Serial.println(response);
  if (RecON) {
    String token = requestToken();
    RecordON(token);
  }
}

void RecordON(String token) {
  client.print("{\"msg_id\":513,\"token\":");
  client.print(token);
  client.print("}\n\r");
  Serial.print("RecON - Response: ");
  yield(); delay(250); yield(); delay(250); yield(); delay(250); yield(); delay(250);
  String response;
  while (client.available()) {
    char character = client.read();
    response.concat(character);
  }
  Serial.println(response);
}

void RecordOFF(String token) {
  client.print("{\"msg_id\":514,\"token\":");
  client.print(token);
  client.print("}\n\r");
  Serial.print("RecOFF - Response: ");
  yield(); delay(250); yield(); delay(250); yield(); delay(250); yield(); delay(250);
  String response;
  while (client.available()) {
    char character = client.read();
    response.concat(character);
  }
  Serial.println(response);
}

class MyCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      connected = true;
      BLE2902* desc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
      desc->setNotifications(true);
    }

    void onDisconnect(BLEServer* pServer) {
      connected = false;
      BLE2902* desc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
      desc->setNotifications(false);
    }
};

/*
   This callback is connect with output report. In keyboard output report report special keys changes, like CAPSLOCK, NUMLOCK
   We can add digital pins with LED to show status
   bit 0 - NUM LOCK
   bit 1 - CAPS LOCK
   bit 2 - SCROLL LOCK
*/
class MyOutputCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* me) {
      uint8_t* value = (uint8_t*)(me->getValue().c_str());
      //ESP_LOGI(LOG_TAG, "special keys: %d", *value);
    }
};

void taskServer(void*) {


  BLEDevice::init(__BT_NAME);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyCallbacks());

  hid = new BLEHIDDevice(pServer);
  input = hid->inputReport(1); // <-- input REPORTID from report map
  output = hid->outputReport(1); // <-- output REPORTID from report map

  output->setCallbacks(new MyOutputCallbacks());

  std::string name = __MANUFACTURER;
  hid->manufacturer()->setValue(name);

  hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  hid->hidInfo(0x00, 0x02);

  BLESecurity *pSecurity = new BLESecurity();
  //  pSecurity->setKeySize();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);

  const uint8_t report[] = {
    USAGE_PAGE(1),      0x01,       // Generic Desktop Ctrls
    USAGE(1),           0x06,       // Keyboard
    COLLECTION(1),      0x01,       // Application
    REPORT_ID(1),       0x01,        //   Report ID (1)
    USAGE_PAGE(1),      0x07,       //   Kbrd/Keypad
    USAGE_MINIMUM(1),   0xE0,
    USAGE_MAXIMUM(1),   0xE7,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x01,
    REPORT_SIZE(1),     0x01,       //   1 byte (Modifier)
    REPORT_COUNT(1),    0x08,
    HIDINPUT(1),           0x02,       //   Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position
    REPORT_COUNT(1),    0x01,       //   1 byte (Reserved)
    REPORT_SIZE(1),     0x08,
    HIDINPUT(1),           0x01,       //   Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position
    REPORT_COUNT(1),    0x06,       //   6 bytes (Keys)
    REPORT_SIZE(1),     0x08,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x65,       //   101 keys
    USAGE_MINIMUM(1),   0x00,
    USAGE_MAXIMUM(1),   0x65,
    HIDINPUT(1),           0x00,       //   Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position
    REPORT_COUNT(1),    0x05,       //   5 bits (Num lock, Caps lock, Scroll lock, Compose, Kana)
    REPORT_SIZE(1),     0x01,
    USAGE_PAGE(1),      0x08,       //   LEDs
    USAGE_MINIMUM(1),   0x01,       //   Num Lock
    USAGE_MAXIMUM(1),   0x05,       //   Kana
    HIDOUTPUT(1),          0x02,       //   Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile
    REPORT_COUNT(1),    0x01,       //   3 bits (Padding)
    REPORT_SIZE(1),     0x03,
    HIDOUTPUT(1),          0x01,       //   Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile
    END_COLLECTION(0)
  };

  hid->reportMap((uint8_t*)report, sizeof(report));
  hid->startServices();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setAppearance(HID_KEYBOARD);
  pAdvertising->addServiceUUID(hid->hidService()->getUUID());
  pAdvertising->start();
  hid->setBatteryLevel(7);
  delay(portMAX_DELAY);
};

void loop() {
  displayBatt();
  switch(currFunc) {
    case 0: menuHandler(); break;
    case 1: yicameraHandler();break;
    case 2: btHandler(); break;
    case 3: irHandler(); break;
    default: break;
  }
  if(M5.Axp.GetBtnPress() == 0x02) 
  {
        esp_restart();
  }
  M5.update();
  delay(1);
}