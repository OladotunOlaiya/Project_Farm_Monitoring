#include "arduino_secrets.h"
#include <ArduinoMqttClient.h>
#include <ArduinoHttpClient.h>
#include <WiFi101.h>
#include <TimeLib.h>
#include <DHT.h>
#include <WiFiUdp.h>
#define DHTPIN A0     
#define DHTTYPE DHT11   

DHT dht(DHTPIN, DHT11);

// NTP Servers:
static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = -5;  // Eastern Standard Time (USA)

///////you may enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

char serverAddress[] = "dangerous-hound-60.loca.lt";  // server address
int http_port = 80;

WiFiClient wifi;
HttpClient client = HttpClient(wifi, serverAddress, http_port);
int status = WL_IDLE_STATUS;

WiFiUDP Udp;
unsigned int localPort = 2390;  // local port to listen for UDP packets

const char broker[] = "demo.thingsboard.io";
int        port     = 1883;
const char topic[]  = "v1/devices/me/telemetry";

String username = "w3iSBMqeAhuLEP95NgzX"; //authentication token here
//String username = "ipQV5UEV8UBItKpmRJVC"; //authentication token here
String password = "";

const long interval = 1000;
unsigned long previousMillis = 0;
const int RELAY_PIN = 7; // the Arduino pin, which connects to the IN pin of relay
int buttonPin = 1;
int trigPin = 1;
int echoPin=0;
int sensorPin = A1; // moisture sensor analogue pin
int sensorValue; 
int limit = 300;  
int pingTravelTime; 
float pingTraveldistance;
float distanceTotarget;
unsigned long getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

void setup() {
  //pin setup
  pinMode(buttonPin, INPUT);
  pinMode(trigPin,OUTPUT);
  pinMode(echoPin,INPUT);
  pinMode(13, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);// initialize digital pin as an output.
  
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  Serial.println("DHT11 test!");

  dht.begin();
  
  //while (!Serial) {
    //; // wait for serial port to connect. Needed for native USB port only
  //}

  // attempt to connect to Wifi network:
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    // failed, retry
    Serial.print(".");
    delay(5000);
  }

  Serial.println("You're connected to the network");
  Serial.println();

  // You can provide a unique client ID, if not set the library uses Arduino-millis()
  // Each client must have a unique client ID
  // mqttClient.setId("clientId");

  // You can provide a username and password for authentication
  // mqttClient.setUsernamePassword("username", "password");
  
  mqttClient.setUsernamePassword(username, password);

  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);

  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());

    while (1);
  }

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();
  Udp.begin(localPort);
}

//
String msgPayload = "";
String msgPayloadHttp = "";

void loop() {
  String contentType = "application/json";
  //*************************************************************************************************
  digitalWrite(trigPin,LOW);
  delayMicroseconds(25);
  digitalWrite(trigPin,HIGH);
  delayMicroseconds(25);
  digitalWrite(trigPin,LOW);
  pingTravelTime = pulseIn(echoPin,HIGH);
  delay(500);
  //distance = (duration/2)*0.034;
  pingTraveldistance=(pingTravelTime*765.*5280.*12.)/(3600.0*1000000.);
  distanceTotarget = (pingTraveldistance/2.0)*2.54;
  //************************************************************************************************
  //getting readings from the moisture sensor
  sensorValue = analogRead(sensorPin); 
  
  //***********************************************************************************************
  
  float h = dht.readHumidity();
  float t = dht.readTemperature();
   Serial.print("distance: ");
  Serial.print( distanceTotarget);
  Serial.print(" \t");
  Serial.print("Analog Value : ");
  Serial.print(sensorValue);
  Serial.print(" \t");
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" *C ");
  Serial.print(" \t");
  //***********************************************************************************************
  //// start the pump if the moisture content is below the threshold(limit)
  
  limit = 700;
  //600 > 700 +> In the soil
  //1023 > 700 => outside the soil
  
  //700 < 600 => in soil
  //700 < 1023 => outside the soil
  while (sensorValue > limit){ 
    //Serial.println("limit:");
    //Serial.print(limit);
    //Serial.println("Sensor Value: ");
    //Serial.print(sensorValue);
    
     digitalWrite(RELAY_PIN, HIGH);
     delay(500);
     digitalWrite(RELAY_PIN, LOW);
     delay(5000);
     
     sensorValue = analogRead(sensorPin);
    }
    digitalWrite(RELAY_PIN, LOW);
    delay(500);
    /*
   else{
      digitalWrite(RELAY_PIN, LOW);
      delay(500);
      
      }
      */
  //***********************************************************************************************

  msgPayload = "{\"temperature\":"+String(t)+", \"humidity\":"+String(h)+",\"moisture\":"+String(sensorValue)+",\"waterlevel\":"+String(distanceTotarget)+" }";
  // call poll() regularly to allow the library to send MQTT keep alives which
  // avoids being disconnected by the broker
  mqttClient.poll();

  // send message, the Print interface can be used to set the message contents
  mqttClient.beginMessage(topic);
  mqttClient.print(msgPayload);
  mqttClient.endMessage();
  

  // send message, the Print interface can be used to set the message contents
  Serial.print("Sending message to topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(msgPayload);
  Serial.println();

  msgPayloadHttp = "{\"temperature\":" + String(t) + ", \"humidity\":" + String(h) + ",\"moisture\":"+String(sensorValue)+",\"waterlevel\":"+String(distanceTotarget)+",\"@timestamp\":" + getNtpTime() + "000}";
  client.post("/my_index/_doc", contentType, msgPayloadHttp);
  // read the status code and body of the response
  int statusCode = client.responseStatusCode();
  String response = client.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);
  
  delay(5000);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

unsigned long getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];

      unsigned long epoch = secsSince1900 - 2208988800UL;
      Serial.println(epoch);


      //return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
      return epoch;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
