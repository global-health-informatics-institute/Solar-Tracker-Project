#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <String.h>
#include <Stream.h>
#include "DS3231.h"
#include <Arduino.h>
#include <Ticker.h>
#include <StopWatch.h>
#define uS_TO_S_FACTOR 1000000 //Coversion factor for micro seconds to seconds
#define TIME_TO_SLEEP 1800 //in seconds
#define LED_PIN 2 //Define LED on pin 2
#define MSG_BUFFER_SIZE  (100)

char msg[MSG_BUFFER_SIZE];
const char* host = "esp32";
const char* ssid = "Solar-Tracker AP";
const char* password = "watchout";
const char* mqtt_server = "192.168.4.2";

int newAngle = 0;
int angle = 47;
unsigned long resetDelay = 0;
long lastReconnectAttempt = 0;

Ticker blinker;
int blinkerPace = 1;  // in seconds, determines how fast the LED should blink

RTClib RTC; // Real time clock library object
StopWatch stopwatch;

RTC_DATA_ATTR int overrideT = 0;//Tracker Operation control variables
RTC_DATA_ATTR int bootCount = 0;// Keeps track of the number of reboots
      
int eastRelay = 12; // relay pinout
int westRelay = 4;

WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
   WiFi.disconnect();
  delay(3000);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String Message = "";
 
  for (int i = 0; i < length; i++) {
        Message += (char)payload[i]; // puts the payload in a string variable
      }
  int msgPayload = Message.toInt();
  
  if(strcmp(topic,"stopTrackerr")==0){//deals with this topic
     Serial.print("Message arrived [");
     Serial.print(topic);
     Serial.print("] ");
     if(msgPayload == 1){
     overrideT = 2;
     digitalWrite(eastRelay,LOW);
     digitalWrite(westRelay,LOW);
     }else if(msgPayload == 0){
      overrideT = 0;
      }
    }
  
  if(strcmp(topic, "control tracker")==0){//deals with that topic
      Serial.print("Message arrived [");
      Serial.print(topic);
      Serial.print("] ");
      // deal with instruction
      newAngle = msgPayload;
      
      if(newAngle < 47){newAngle = 47;} //makes sure that the value isnt less than allowed
      if(newAngle > 133){newAngle = 133;} //makes sure that the value isnt greater than allowed
      
      int newPos = 0;
      int delay_is = 0;
      newPos = newAngle - angle;
      
      if(newPos < 0){ //if the new angle is less than the current angle, turn east and take the new angle position
          
          delay_is = abs(newPos)*734;

          digitalWrite(eastRelay,HIGH);
          delay(delay_is);
          digitalWrite(eastRelay,LOW);
          
          angle = newAngle;
        
      }else if(newPos > 0){ // if the new angle is greater than the current angle, turn west and take the new angle position
          delay_is = abs(newPos)*734;
          Serial.println(delay_is);
          
          digitalWrite(westRelay,HIGH);
          delay(delay_is);
          digitalWrite(westRelay,LOW);
          
          angle = newAngle;
      }
}
}

//Non-blocking reconnection function for mqtt broker
boolean reconnect() {
  if (client.connect("arduinoClient")) {
    Serial.println("MQTT Broker connected");
    // Once connected, publish an announcement...
    client.publish("outTopic","Connection established");
    // ... and resubscribe
    client.subscribe("control tracker");
    client.subscribe("stopTrackerr");
  }
  return client.connected();
}

//Check if header is present and correct
bool is_authentified() {
  Serial.println("Enter is_authentified");
  if (server.hasHeader("Cookie")) {
    Serial.print("Found cookie: ");
    String cookie = server.header("Cookie");
    Serial.println(cookie);
    if (cookie.indexOf("ESPSESSIONID=1") != -1) {
      Serial.println("Authentification Successful");
      return true;
    }
  }
  Serial.println("Authentification Failed");
  return false;
}

//setup function
void setup() {
  Serial.begin(115200);
  
  Wire.begin();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);//ESP32 stays in deep sleep for an arbitrary number of milliseconds
  
  if(overrideT == 1){//The ESP32 keeps going back into deepsleep if the number of required reboots is not completed
      bootCount++;
      if(bootCount <= 30){
          Serial.println("This is reboot number: ");
          Serial.println(bootCount);
          esp_deep_sleep_start();
      }else if(bootCount == 31){
          overrideT = 0;
          bootCount = 0;
   } 
  }
  overrideT = 0;//initially set the Override value to 0
 
  pinMode(eastRelay, OUTPUT);
  pinMode(westRelay, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  blinker.attach(blinkerPace, blink); //ESP32 Heart Beat
  
  int resetter = 86*734;
  digitalWrite(eastRelay,HIGH);
  delay(resetter);
  digitalWrite(eastRelay,LOW);
  
  stopwatch.start();
  //setup_wifi();
  
  //AP is created
   WiFi.softAP(ssid, password);
   IPAddress myIP = WiFi.softAPIP();
   Serial.print("AP IP address: ");
   Serial.println(myIP);
   client.setServer(mqtt_server, 1220);
   client.setCallback(callback); // sets the call back function as the go to fuction to deal with incoming messages
   lastReconnectAttempt = 0;
  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  
  server.on("/OVERRIDE", handle_override);
  server.on("/AUTOMATE", handle_automate);
  server.on("/TurnEast", handle_east);
  server.on("/TurnWest", handle_west);
  server.onNotFound(handle_NotFound);
  server.on("/", handleRoot);
  server.on("/login", handleLogin);
 
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  //here the list of headers to be recorded
  const char * headerkeys[] = {"User-Agent", "Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);
  
  //ask server to track these headers
  server.collectHeaders(headerkeys, headerkeyssize);
  server.begin();
}


//MAIN LOOP FUNCTION
void loop(void) {
  
  //Continuously triest to connect to mqtt server 
  if (!client.connected()) {
    Serial.println("Attempting mqtt connection");
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
    client.loop();
  }
  
  server.handleClient();
  delay(1);
  
  DateTime now = RTC.now(); // Real time clock date and time synchronised with the ESP32 date and time

  if(now.hour() == 17 && now.minute() == 5){// When the sunset timer is set off
     overrideT = 1; // the override variable is set to 1
     resetDelay = 86*734; // 86 is the span of rotation of the tracker. it rotates a total of 86 degrees
     
     digitalWrite(eastRelay,HIGH); // The panel is then reset to face the eastern side
     delay(resetDelay);
     digitalWrite(eastRelay,LOW);
     esp_deep_sleep_start();  // ESP32 goes into deep sleep mode
  }

  
  if(overrideT == 0){
       if(angle < 133){
        int nextAngle = 0;
        nextAngle = ((((now.hour() - 9)*60)+now.minute())/4)+47;
        Serial.println("The angle now is: ");
        Serial.println(nextAngle);
        int newDelay = abs(nextAngle - angle)*734;
           
           if (nextAngle > angle){
                  digitalWrite(westRelay,HIGH);
                  delay(newDelay);
                  digitalWrite(westRelay,LOW);
                  angle = nextAngle;
           }else if(nextAngle < angle){
                  digitalWrite(eastRelay,HIGH);
                  delay(newDelay);
                  digitalWrite(eastRelay,LOW);
                  angle = nextAngle;
            }
            //publishes the solar tracker position as it travels from east to west
             snprintf (msg, MSG_BUFFER_SIZE,"%ld", angle);
             Serial.print("Publish message: ");
             Serial.println(msg);
             client.publish("sun-position",msg);
         }
        if(angle == 133){
                  snprintf (msg, MSG_BUFFER_SIZE,"%ld", angle);
                  Serial.print("Publish message: ");
                  Serial.println(msg);
                  client.publish("sun-position",msg);
            }
  }else{
       snprintf (msg, MSG_BUFFER_SIZE,"%ld", angle);
       Serial.print("Publish message: ");
       Serial.println(msg);
       client.publish("sun-position",msg);
   }
   // Current time on RTC is published.
   snprintf (msg, MSG_BUFFER_SIZE,"%ld", now.unixtime());
   Serial.print("Publish message: ");
   Serial.println(msg);
   client.publish("time",msg);
 }



//Makes LED Blink
void blink() {
  digitalWrite(LED_PIN, !digitalRead(LED_PIN));}

//Handles Login page
 void handleLogin() {
  String msg;
  if (server.hasHeader("Cookie")) {
    Serial.print("Found cookie: ");
    String cookie = server.header("Cookie");
    Serial.println(cookie);
  }
  if (server.hasArg("DISCONNECT")) {
    Serial.println("Disconnection");
    server.sendHeader("Location", "/login");
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("Set-Cookie", "ESPSESSIONID=0");
    server.send(301);
    return;
  }
  if (server.hasArg("userid") && server.hasArg("pwd")) {
    if (server.arg("userid") == "Chato" &&  server.arg("pwd") == "Solartracker") {
      server.sendHeader("Location", "/");
      server.sendHeader("Cache-Control", "no-cache");
      server.sendHeader("Set-Cookie", "ESPSESSIONID=1");
      server.send(301);
      Serial.println("Log in Successful");
      return;
    }
    msg = "Wrong Username or Password!";
    Serial.println("Log in Failed");
  }
 
 String style =
"<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
"input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
"#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
"#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
"form{background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
".btn{background:#3498db;color:#fff;cursor:pointer}</style>";

 String loginIndex = 
"<form name=loginForm>"
"<h1>Solar Tracker OTA Login</h1>"
"<input name=userid placeholder='User ID'> "
"<input name=pwd placeholder=Password type=Password> "
"<input type=submit onclick=check(this.form) class=btn value=Login></form>"
"<script>"
"function check(form) {"
"if(form.userid.value!='Chato' || form.pwd.value!='Solartracker')"
"{alert('Error Password or Username')}"
"}"
"</script>" + style;
  server.send(200, "text/html", loginIndex);
}
//Handles the upload page
void handleRoot() {
  Serial.println("Enter handleRoot");
  String header;
  if (!is_authentified()) {
    server.sendHeader("Location", "/login");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(301);
    return;
  }
 String style =
"<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
"input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
"#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
"#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
"form{background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
".btn{background:#3498db;color:#fff;cursor:pointer}</style>";

 String serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
"<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
"<label id='file-input' for='file'>   Choose file...</label>"
"<input type='submit' class=btn value='Update'>"
"<br><br>"
"<div id='prg'></div>"
"<br><div id='prgbar'><div id='bar'></div></div><br></form>"
"<script>"
"function sub(obj){"
"var fileName = obj.value.split('\\\\');"
"document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];"
"};"
"$('form').submit(function(e){"
"e.preventDefault();"
"var form = $('#upload_form')[0];"
"var data = new FormData(form);"
"$.ajax({"
"url: '/update',"
"type: 'POST',"
"data: data,"
"contentType: false,"
"processData:false,"
"xhr: function() {"
"var xhr = new window.XMLHttpRequest();"
"xhr.upload.addEventListener('progress', function(evt) {"
"if (evt.lengthComputable) {"
"var per = evt.loaded / evt.total;"
"$('#prg').html('progress: ' + Math.round(per*100) + '%');"
"$('#bar').css('width',Math.round(per*100) + '%');"
"}"
"}, false);"
"return xhr;"
"},"
"success:function(d, s) {"
"console.log('success!') "
"},"
"error: function (a, b, c) {"
"}"
"});"
"});"
"</script>" + style;
serverIndex += "<a href=\"/login?DISCONNECT=YES\">Logout</a></body></html>" + style;
  server.send(200, "text/html", serverIndex);
}
//Overrides the tracker
void handle_override(){
    overrideT = 2;
    digitalWrite(eastRelay, LOW);
    digitalWrite(westRelay, LOW);
  }
//Tracker exits override
void handle_automate(){
    overrideT = 0;
  }
//Turns the tracker east
void handle_east(){
    digitalWrite(12, HIGH);
    delay(6250);
    digitalWrite(12, LOW);
  }
//Turns the tracker west
void handle_west(){
    digitalWrite(4, HIGH);
    delay(6250);
    digitalWrite(4, LOW);
  }
//URL not found
void handle_NotFound(){
    server.send(404, "text/plain", "Not found");
}
