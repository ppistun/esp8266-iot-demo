#include <ESP8266WiFi.h>
#include <MQTT.h>
#include <EEPROM.h>

//servo
#include <Servo.h>

#define DEBUG

#define AP_SSID     "SSID"
#define AP_PASSWORD "password"

#define EIOTCLOUD_USERNAME "sdemo"
#define EIOTCLOUD_PASSWORD "sdemo12password"


// create MQTT object
#define EIOT_CLOUD_ADDRESS "cloud.iot-playground.com"
MQTT myMqtt("some_id_45", EIOT_CLOUD_ADDRESS, 1883);

#define CONFIG_START 0
#define CONFIG_VERSION "v01"

#define AP_CONNECT_TIME 30 //s

char ap_ssid[16];
char ap_pass[16];

#define SUB "/"

Servo servo;

WiFiServer server(80);

//GPIO
#define GPIO_LAMP 5
#define GPIO_CUS1 4


struct StoreStruct {
  // This is for mere detection if they are your settings
  char version[4];
  // The variables of your settings
  uint moduleId;  // module id
  bool state;     // state
  char ssid[20];
  char pwd[20];
} storage = {
  CONFIG_VERSION,
  // The default module 0
  3,
  0, // off
  AP_SSID,
  AP_PASSWORD
};

bool stepOk = false;
int buttonState;

boolean result;
String topic("");
String valueStr("");

boolean switchState;
const int buttonPin = 0;
const int outPin = 2;
int lastButtonState = LOW;
int cnt = 0;

void setup()
{
#ifdef DEBUG
  Serial.begin(115200);
#endif
  delay(1000);

  pinMode(buttonPin, INPUT);
  digitalWrite(buttonPin, HIGH);

  EEPROM.begin(512);

  switchState = false;

  loadConfig();

  servo.attach(14);
  pinMode(GPIO_LAMP, OUTPUT);
  pinMode(GPIO_CUS1, OUTPUT);

  digitalWrite(GPIO_LAMP, HIGH);
  digitalWrite(GPIO_CUS1, HIGH);

#ifndef DEBUG
  pinMode(outPin, OUTPUT);
  digitalWrite(outPin, switchState);
#endif


#ifdef DEBUG
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(AP_SSID);
#endif
  WiFi.begin(storage.ssid, storage.pwd);

  int i = 0;

  while (WiFi.status() != WL_CONNECTED && i++ < (AP_CONNECT_TIME*2) ) {
    delay(500);
#ifdef DEBUG
    Serial.print(".");
#endif
  }

  if (!(i < (AP_CONNECT_TIME*2)))
  {
    ESP.reset();
  }

#ifdef DEBUG
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Connecting to MQTT server");
#endif

#ifdef DEBUG
  Serial.print("MQTT client id:");
  Serial.println(clientName);
#endif
  // setup callbacks
  myMqtt.onConnected(myConnectedCb);
  myMqtt.onDisconnected(myDisconnectedCb);
  myMqtt.onPublished(myPublishedCb);
  myMqtt.onData(myDataCb);

  //////Serial.println("connect mqtt...");
  myMqtt.setUserPwd(EIOTCLOUD_USERNAME, EIOTCLOUD_PASSWORD);
  myMqtt.connect();

  delay(500);

#ifdef DEBUG
  Serial.print("ModuleId: ");
  Serial.println(storage.moduleId);
#endif

  storage.state = switchState;

  Serial.println("Suscribe: /"+String(storage.moduleId)+ SUB);
  myMqtt.subscribe("/"+String(storage.moduleId)+ SUB);
}

unsigned int previousMillis = 0;
const int interval = 500;
int als_data_prev = 0;

void loop() {
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
#ifdef DEBUG
    Serial.print(".");
#endif
  }


  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    if (reading == LOW)
    {
      switchState = !switchState;
#ifdef DEBUG
      Serial.println("button pressed");
#endif
    }

#ifdef DEBUG
      Serial.println("set state: ");
      Serial.println(valueStr);
#endif

    lastButtonState = reading;
  }

   unsigned long currentMillis = millis();
  if(currentMillis - previousMillis >= interval) {

    auto vcc = ESP.getVcc();
    int als_data = analogRead(A0);

    if (als_data - als_data_prev > 10 || als_data_prev - als_data > 10)
    {
      //sendmessage(als_data);
      valueStr = String(als_data);
      topic  = "/"+String(storage.moduleId)+ "/light_sensor";
      result = myMqtt.publish(topic, valueStr, 0, 1024);

      als_data_prev = als_data;

#ifdef DEBUG
      Serial.print("Publish ");
      Serial.print(topic);
      Serial.print(" ");
      Serial.print(valueStr);
      Serial.print(" result:");
      Serial.println(result);
#endif

    }
    previousMillis = currentMillis;
  }

#ifndef DEBUG
  digitalWrite(outPin, switchState);
#endif
  if (switchState != storage.state)
  {
    storage.state = switchState;
    // save button state
    saveConfig();

    valueStr = String(switchState);
    topic  = "/"+String(storage.moduleId)+ "/light_sensor";
    result = myMqtt.publish(topic, valueStr, 0, 1);
#ifdef DEBUG
      Serial.print("Publish ");
      Serial.print(topic);
      Serial.print(" ");
      Serial.println(valueStr);
#endif
    delay(200);
  }
}

void waitOk()
{
  while(!stepOk)
    delay(100);

  stepOk = false;
}

String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}


/*
 *
 */
void myConnectedCb() {
#ifdef DEBUG
  Serial.println("connected to MQTT server");
#endif
  if (storage.moduleId != 0)
    myMqtt.subscribe("/" + String(storage.moduleId) + SUB);
}

void myDisconnectedCb() {
#ifdef DEBUG
  Serial.println("disconnected. try to reconnect...");
#endif
  delay(500);
  myMqtt.connect();
}

void myPublishedCb() {
#ifdef DEBUG
  Serial.println("published.");
#endif
}

void myDataCb(String& topic, String& data) {
#ifdef DEBUG
  Serial.print("Received topic:");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(data);
#endif
  if (topic ==  String("/NewModule"))
  {
    storage.moduleId = data.toInt();
    stepOk = true;
  }
  else if (topic == String("/"+String(storage.moduleId)+ "/Sensor.Parameter1/NewParameter"))
  {
    stepOk = true;
  }
  else if (topic == String("/"+String(storage.moduleId)+ "/lamp"))
  {
    bool bOn = data.toInt() > 0? true : false;
#ifdef DEBUG
    Serial.print("lamp received: ");
    Serial.println(bOn);
#endif
    digitalWrite(GPIO_LAMP, !bOn?HIGH:LOW);
  }
  else if (topic == String("/"+String(storage.moduleId)+ "/custom1"))
  {
    bool bOn = data.toInt() > 0? true : false;
#ifdef DEBUG
    Serial.print("custom1 received: ");
    Serial.println(bOn);
#endif
    digitalWrite(GPIO_CUS1, !bOn?HIGH:LOW);
  }
  else if (topic == String("/"+String(storage.moduleId)+ "/Sensor.Parameter1") || topic == String("/"+String(storage.moduleId)+ "/servo"))
  {
    int angle = data.toInt();
#ifdef DEBUG
    Serial.print("servo received: ");
    Serial.println(angle);
#endif
    servo.write(angle);
  }
}

void loadConfig() {
}

void saveConfig() {
}



//void AP_Setup(void){
//  Serial.println("setting mode");
//  WiFi.mode(WIFI_AP);
//
//  String clientName;
//  clientName += "Thing-";
//  uint8_t mac[6];
//  WiFi.macAddress(mac);
//  clientName += macToStr(mac);
//
//  Serial.println("starting ap");
//  WiFi.softAP((char*) clientName.c_str(), "");
//  Serial.println("running server");
//  server.begin();
//}
//
//void AP_Loop(void){
//
//  bool  inf_loop = true;
//  int  val = 0;
//  WiFiClient client;
//
//  Serial.println("AP loop");
//
//  while(inf_loop){
//    while (!client){
//      Serial.print(".");
//      delay(100);
//      client = server.available();
//    }
//    String ssid;
//    String passwd;
//    // Read the first line of the request
//    String req = client.readStringUntil('\r');
//    client.flush();
//
//    // Prepare the response. Start with the common header:
//    String s = "HTTP/1.1 200 OK\r\n";
//    s += "Content-Type: text/html\r\n\r\n";
//    s += "<!DOCTYPE HTML>\r\n<html>\r\n";
//
//    if (req.indexOf("&") != -1){
//      int ptr1 = req.indexOf("ssid=", 0);
//      int ptr2 = req.indexOf("&", ptr1);
//      int ptr3 = req.indexOf(" HTTP/",ptr2);
//      ssid = req.substring(ptr1+5, ptr2);
//      passwd = req.substring(ptr2+10, ptr3);
//      val = -1;
//    }
//
//    if (val == -1){
//      strcpy(storage.ssid, ssid.c_str());
//      strcpy(storage.pwd, passwd.c_str());
//
//      saveConfig();
//      //storeAPinfo(ssid, passwd);
//      s += "Setting OK";
//      s += "<br>"; // Go to the next line.
//      s += "Continue / reboot";
//      inf_loop = false;
//    }
//
//    else{
//      String content="";
//      // output the value of each analog input pin
//      content += "<form method=get>";
//      content += "<label>SSID</label><br>";
//      content += "<input  type='text' name='ssid' maxlength='19' size='15' value='"+ String(storage.ssid) +"'><br>";
//      content += "<label>Password</label><br>";
//      content += "<input  type='password' name='password' maxlength='19' size='15' value='"+ String(storage.pwd) +"'><br><br>";
//      content += "<input  type='submit' value='Submit' >";
//      content += "</form>";
//      s += content;
//    }
//
//    s += "</html>\n";
//    // Send the response to the client
//    client.print(s);
//    delay(1);
//    client.stop();
//  }
//}
