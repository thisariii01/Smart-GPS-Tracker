#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

WiFiClient espClient;
PubSubClient mqttClient(espClient);

//Initially defined SOS phone number with country code
String PHONE = "+94703760969";

#define rxGSM 12
#define txGSM 13
SoftwareSerial sim800(rxGSM,txGSM);

#define rxGPS 5
#define txGPS 4
SoftwareSerial neogps(rxGPS,txGPS);
TinyGPSPlus gps;

String smsStatus,senderNumber,receivedDate,msg;

#define SOS 14
int SOS_Time = 5;
int c = 0;
float latitude;
float longitude;
float velocity;
float height;
char latArr[10];
char longArr[10];
char veloArr[10];
char altiArr[10];
char phoneArr[12];

void setup() {
  
  Serial.begin(115200);
  Serial.println("Arduino serial initialize");
  
  sim800.begin(9600);
  Serial.println("SIM800L serial initialize");

  neogps.begin(9600);
  Serial.println("NodeMCU serial initialize");
 
  smsStatus = "";
  senderNumber="";
  receivedDate="";
  msg="";

  sim800.print("AT+CMGF=1\r"); //SMS text mode
  delay(1000);

  sim800.print("AT+CMGD=1,4");

  WiFiManager wifiManager;
  //wifiManager.resetSettings();
  wifiManager.autoConnect("GPSTrackerAP");
  Serial.println("Connected.......");
  sim800.print("AT+CMGF=1\r");
  delay(1000);
  sim800.print("AT+CMGS=\""+PHONE+"\"\r");
  delay(1000);
  sim800.print("Connected to WiFi");
  delay(100);
  sim800.write(0x1A); //ascii code for ctrl-26 
  delay(1000);
  setupMqtt();
}

void loop() {
  if (digitalRead(SOS) == LOW){
      Serial.print("Calling In.."); // Waiting for 5 sec
      for (c = 0; c < SOS_Time; c++)
      {
        Serial.println((SOS_Time - c));
        delay(1000);
        if (digitalRead(SOS) == HIGH)
          break;
      }

      if (c == 5)
      {
        Serial.println("Sending location");
        sendLocation();
        delay(10000);
        sim800.println("ATD"+PHONE+";");
        delay(10000);
      }
  }

  if (!mqttClient.connected()) {
        connectToBroker();
  }

  mqttClient.loop();
	getLocation();
  dtostrf(latitude, 0, 6, latArr);
  dtostrf(longitude, 0, 6, longArr);
  dtostrf(velocity, 0, 6, veloArr);
  dtostrf(height, 0, 6, altiArr);
  PHONE.toCharArray(phoneArr,13);
  mqttClient.publish("200023C_LONGITUDE", longArr);
	mqttClient.publish("200023C_LATITUDE", latArr);
  mqttClient.publish("200023C_VELOCITY", veloArr);
  mqttClient.publish("200023C_ALTITUDE", altiArr);
  mqttClient.publish("200023C_SOSNUM", phoneArr);
  delay(1000);
}

void sendLocation()
{
  boolean newData = false;
  
  for (unsigned long start = millis(); millis() - start < 2000;)
  {
    while (neogps.available())
    {
      if (gps.encode(neogps.read()))
        {newData = true;}
    }
  }

  //If newData is true
  if(newData)
  {
    yield();
    Serial.print("Latitude= "); 
    Serial.print(gps.location.lat(), 6);
    Serial.print(" Longitude= "); 
    Serial.println(gps.location.lng(), 6);
    newData = false;

    String latitudeS = String(gps.location.lat(), 6);
    String longitudeS = String(gps.location.lng(), 6);
    
    String text = "Latitude= " + latitudeS;
    text += "\n\r";
    text += "Longitude= " + longitudeS;
    text += "\n\r";
    text += "Speed= " + String(gps.speed.kmph()) + " km/h";
    text += "\n\r";
    text += "Altitude= " + String(gps.altitude.meters()) + " meters";
    text += "\n\r";
    text += "http://maps.google.com/maps?q=" + latitudeS + "+" + longitudeS;

    delay(300);
    yield();
    sim800.print("AT+CMGF=1\r");
    delay(1000);
    sim800.print("AT+CMGS=\""+PHONE+"\"\r");
    delay(1000);
    sim800.print(text);
    delay(100);
    sim800.write(0x1A); //ascii code for ctrl-26 
    delay(1000);
    Serial.println("GPS Location SMS Sent Successfully.");
  }
  else {Serial.println("Invalid GPS data");}
}

void getLocation() {
  boolean newData = false;
  
  for (unsigned long start = millis(); millis() - start < 2000;)
  {
    while (neogps.available())
    {
      if (gps.encode(neogps.read()))
        {newData = true;}
    }
  }

  //If newData is true
  if(newData)
  {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
    velocity = gps.speed.kmph();
    height = gps.altitude.meters();
    newData = false;
  }
  else {Serial.println("Invalid GPS data");}
}

void setupMqtt() {
    mqttClient.setServer("test.mosquitto.org", 1883);
    mqttClient.setCallback(receiveCallback);
}

void receiveCallback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    char payloadCharAr[length];
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
        payloadCharAr[i] = (char)payload[i];
    }
    Serial.println();
    Serial.println(PHONE);
    Serial.println(payloadCharAr);
		if(strcmp(topic,"200023C_SOS_SET") == 0) {
      for (int i = 0; i < 12; i++) {
        PHONE[i] = payloadCharAr[i];
      }
      Serial.println(PHONE);
    }  
}

void connectToBroker() {
    while (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection... ");
        if (mqttClient.connect("ESP8266-200023C")) {
              sim800.print("AT+CMGF=1\r");
              delay(1000);
              sim800.print("AT+CMGS=\""+PHONE+"\"\r");
              delay(1000);
              sim800.print("Connected to Server");
              delay(100);
              sim800.write(0x1A); //ascii code for ctrl-26 
              delay(1000);
            Serial.println("connected");
						//Subscribing To Topics
						mqttClient.subscribe("200023C_SOS_SET");
        }
        else {
            Serial.print("failed");
            Serial.println(mqttClient.state());
            delay(5000);
        }
    }
}
