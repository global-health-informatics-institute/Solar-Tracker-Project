#include <WiFi.h>
#include<WebServer.h>
#include<WiFiClient.h>
#define uS_TO_S_FACTOR 1000000 //Coversion factor for micro seconds to seconds
#define TIME_TO_SLEEP 1800 //in seconds

unsigned long dayTime = 0;          // day-time timer variable
unsigned long nightTime = 0;          // night-time timer variable

RTC_DATA_ATTR unsigned long retract = 135000;
RTC_DATA_ATTR unsigned long theNightTime = 24900000;           // The amount of time needed to wait till the next sunset initially

RTC_DATA_ATTR unsigned long hourMark = 3300000;            // variable hold an arbitrary number of milliseconds
      
int eastRelay = 4; // relay pinout
int westRelay = 2;

RTC_DATA_ATTR int overrideT = 0;                              //Tracker Operation control variables
RTC_DATA_ATTR int bootCount = 0;                              // Keeps track of the number of reboots

String  ClientRequest;
WiFiServer server(80);

WiFiClient client;

String myresultat;

String ReadIncomingRequest() {                      // Reads client requests as string
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
    overrideT = 1;
    Serial.println("Override is on....");
   digitalWrite(eastRelay, LOW);
   digitalWrite(westRelay, LOW);
  }
  if (ClientRequest == "TurnEast") { // Makes tracker turn east
    Serial.println("Turning east now....");
    digitalWrite(4, HIGH);
    delay(5000);
    digitalWrite(4, LOW);
    delay(1000);
    Serial.println("Finished turning east....");
  }

  if (ClientRequest == "TurnWest") { // Makes tracker turn west
    Serial.println("Turning west now....");
    digitalWrite(2, HIGH);
    delay(5000);
    digitalWrite(2, LOW);
    delay(5000);
    Serial.println("Finished turning west....");
  }

  if (ClientRequest == "AUTOMATE") {  // exits override mode
    overrideT = 0;
    Serial.println("Override is off....");
  }
  if (ClientRequest == "Op Time/Duration + 1hr") { //increase the tracker operation time by an hour
       if(theNightTime < 32400000){
       theNightTime += 3600000;
       }
       if(theNightTime >= 32400000){
       }
  }
  if (ClientRequest == "Op Time - 1hr") { //decrease the tracker operation time by an hour
       unsigned long x = theNightTime - 3600000;
       if( x > 0){
       theNightTime -= 3600000;
              }
  }
  if (ClientRequest == "Operate for 9hrs") { //reset to default duration value
       theNightTime = 32400000;
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

void setup() {
  
  Serial.begin(9600); 
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);     //ESP32 stays in deep sleep for an arbitrary number of milliseconds
  Serial.println(TIME_TO_SLEEP * 1000000UL);
  
  
 if(overrideT == 1){                          //The ESP32 keeps going back into deepsleep if the number of required reboots is not completed
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
  
  overrideT = 0;                     //initially set the Override value to 0
  
  dayTime = millis();
  nightTime = millis();
  pinMode(eastRelay, OUTPUT);
  pinMode(westRelay, OUTPUT);
 
 WiFi.mode(WIFI_AP_STA);  //Both hotspot and client are enabled
  
  Serial.println("START");
  WiFi.softAP("Solar TrackerAP", "watchout");   //ESP32 Access Point
  Serial.println("Access Point IP is");
  Serial.println((WiFi.softAPIP()));
  server.begin();                          //server starts

}


void loop(){

callServer();                               // function call that gives user access to server
if(overrideT == 0){                         // When the override variable is 0
     
 if(millis() - dayTime >= hourMark){       //Moves panel by a value of 11.5 degrees every hour

Serial.println("Pass");
 digitalWrite(westRelay,HIGH);
 delay(11500);
 digitalWrite(westRelay,LOW);
 delay(1);

  dayTime= millis();
  hourMark = 3600000;
}
}



if(millis() - nightTime >= theNightTime){   // When the sunset timer is set off
overrideT = 1; // the override variable is set to 1
digitalWrite(eastRelay,HIGH);               // The panel is then reset to face the eastern side
delay(retract);
digitalWrite(eastRelay,LOW);
delay(1);
nightTime = millis();

theNightTime =34200000;                    //The sunset timer will be set off after 9 hours                    
Serial.println("Going into deep sleep now....");
esp_deep_sleep_start();                       // ESP32 goes into deep sleep mode
}
}
