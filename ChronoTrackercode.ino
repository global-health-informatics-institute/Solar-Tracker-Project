#include <WiFi.h>

unsigned long dayTime = 0;          // day-time timer variable
unsigned long nightTime = 0;          // night-time timer variable

unsigned long theNightTime = 32400000 ; // The amount of time needed to wait till the next sunset initially

unsigned long hourMark = 600000;            // variable hold an arbitrary number of milliseconds
unsigned long offTime;                      // Determines the duration that tracker waits      

int eastRelay = 4; // relay pinout
int westRelay = 2;

int overrideT;                              //Tracker Operation control variables

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
  
  Serial.println("Server access granted");
  
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
   digitalWrite(eastRelay, LOW);
   digitalWrite(westRelay, LOW);
  }
  if (ClientRequest == "TurnEast") { // Makes tracker turn east
    digitalWrite(2, HIGH);
    delay(5000);
    digitalWrite(2, LOW);
    delay(5000);
  }

  if (ClientRequest == "TurnWest") { // Makes tracker turn west
    digitalWrite(4, HIGH);
    delay(5000);
    digitalWrite(4, LOW);
    delay(5000);
  }

  if (ClientRequest == "AUTOMATE") {  // exits override mode
    overrideT = 0;
  }
  if (ClientRequest == "Op Time/Duration + 1hr") { //increase the tracker operation time by an hour
       theNightTime += 3600000;
       if(theNightTime > 86400000){
       offTime+=3600000;}
  }
  if (ClientRequest == "Op Time - 1hr") { //decrease the tracker operation time by an hour
       theNightTime -= 3600000;
       offTime-=3600000;
  }
  if (ClientRequest == "Op duration - 1hr") {// decrease the tracker duration time by an hour
       theNightTime -= 3600000;
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
  
  overrideT = 0;                     //initially set the Override value to 0
  
  dayTime = millis();
  nightTime = millis();
  
  pinMode(eastRelay, OUTPUT);
  pinMode(westRelay, OUTPUT);

  Serial.println("START");
  WiFi.softAP("Solar Tracker2", "watchout");   //ESP32 Access Point
  Serial.println("Access Point IP is");
  Serial.println((WiFi.softAPIP()));
  server.begin();                          //server starts

}


void loop(){

callServer();                               // function call that gives user access to server
if(overrideT == 0){                         // When the override variable is 0
     
 if(millis() - dayTime >= hourMark){       //Moves panel by a value of 15 degrees every hour
 
 digitalWrite(westRelay,HIGH);
 delay(15000);
 digitalWrite(westRelay,LOW);
 delay(1);

  dayTime= millis();
  hourMark = 3600000;
}

}else if(overrideT == 1){                     //When the override variable is 1 the tracker is idle and waits.

for(offTime = 0; offTime >= 1512000; offTime++){   // 28 cycles approximately make a second, the tracker waits until count up is finished.
callServer();                                    //enables server contact even during the wait cycle
}
overrideT = 0;                                   // Override variable set back to 0 to resume tracking
}


if(millis() - nightTime >= theNightTime){   // When the sunset timer is set off
overrideT = 1;                              // the override variable is set to 1

digitalWrite(eastRelay,HIGH);               // The panel is then reset to face the eastern side
delay(105000);
digitalWrite(eastRelay,LOW);
delay(1);
nightTime = millis();

theNightTime =86400000;                    //The sunset timer will be set off after 24 hours                    
}
}
