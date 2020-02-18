#include <WiFi.h>
#include<WebServer.h>
#include<WiFiClient.h>
#include <Wire.h>
#include "DS3231.h"
#define uS_TO_S_FACTOR 1000000 //Coversion factor for micro seconds to seconds
#define TIME_TO_SLEEP 1800 //in seconds

RTClib RTC; // Real time clock library object

RTC_DATA_ATTR int overrideT = 0;//Tracker Operation control variables
RTC_DATA_ATTR int bootCount = 0;// Keeps track of the number of reboots
      
int eastRelay = 32; // relay pinout
int westRelay = 13;

String  ClientRequest;

WiFiServer server(80);

WiFiClient client;

String myresultat;
String ReadIncomingRequest() {// Reads client requests as string
  while (client.available()) {
    ClientRequest = (client.readStringUntil('\r'));
    if ((ClientRequest.indexOf("HTTP/1.1") > 0) && (ClientRequest.indexOf("/favicon.ico") < 0)) {
      myresultat = ClientRequest;
    }
  }
  return myresultat;
  }



//Gives server access (client request handler)
void callServer() {
  
  client = server.available();
  
  if (!client) {
    return;
  }
  while (!client.available()) {
    delay(1);
  }
  
  ClientRequest = (ReadIncomingRequest());
  ClientRequest.remove(0, 5);
  ClientRequest.remove(ClientRequest.length() - 9, 9);

  if (ClientRequest == "OVERRIDE") { // Overrides the tracker and makes it stop
    overrideT = 2;
    digitalWrite(eastRelay, LOW);
    digitalWrite(westRelay, LOW);
  }
  
  if (ClientRequest == "TurnEast") { // Makes tracker turn east
    digitalWrite(12, HIGH);
    delay(6250);
    digitalWrite(12, LOW);
  }
  
  if (ClientRequest == "TurnWest") { // Makes tracker turn west
    digitalWrite(4, HIGH);
    delay(6250);
    digitalWrite(4, LOW);
      }
  
  if (ClientRequest == "AUTOMATE") {  // exits override mode
    overrideT = 0;
    
  }
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("");
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.println("OK");
  client.println("</html>");
  client.stop();
  delay(1);
  client.flush();
}

//Everything in this function is done once
void setup() {
  
  Serial.begin(9600); 
  Wire.begin();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);//ESP32 stays in deep sleep for an arbitrary number of milliseconds
  
  if(overrideT == 1){//The ESP32 keeps going back into deepsleep if the number of required reboots is not completed
      bootCount++;
      if(bootCount <= 28){
          Serial.println("This is reboot number: ");
          Serial.println(bootCount);
          esp_deep_sleep_start();
      }else if(bootCount == 29){
          overrideT = 0;
          bootCount = 0;
   } 
  }

  overrideT = 0;//initially set the Override value to 0
 
  pinMode(eastRelay, OUTPUT);
  pinMode(westRelay, OUTPUT);
  pinMode(2,OUTPUT);

  WiFi.mode(WIFI_AP_STA);//Both hotspot and client are enabled
  
  Serial.println("START");
  WiFi.softAP("Solar TrackerAP", "watchout");//ESP32 Access Point
  Serial.println("Access Point IP is");
  Serial.println((WiFi.softAPIP()));
  server.begin(); //server starts
}

//The main loop function
void loop(){
  
  DateTime now = RTC.now(); // Real time clock date and time synchronised with the ESP32 date and time
  callServer(); // function call that gives user access to server
  
  digitalWrite(2,HIGH); /*HeartBeat of ESP32*/
  delay(1000);
  digitalWrite(2,LOW);
  delay(1000);

  if(now.hour() == 17 && now.minute() == 0){// When the sunset timer is set off
     overrideT = 1; // the override variable is set to 1
  
     digitalWrite(eastRelay,HIGH); // The panel is then reset to face the eastern side
     delay(140000L);
     digitalWrite(eastRelay,LOW);
    
     esp_deep_sleep_start();  // ESP32 goes into deep sleep mode
     }
  
  if(overrideT == 0){    // When the override variable is 0
     if(now.hour() > 8 && now.minute() == 0 && now.second() < 5){ //Moves panel by a value of 15 degrees every hour
          digitalWrite(westRelay,HIGH);
          delay(12500);
          digitalWrite(westRelay,LOW);
     }
   }
}
