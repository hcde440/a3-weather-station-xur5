// Melody Xu
// HCDE 440 
// 4/23/19
// A3 Weather Station

// This sketch collects temperature , pressure, and humidity data from the dht and mpl
// sensors and displays them on the OLED display
// It also reports sensor data at regular intervals, and the button state data as it occurs (aperiodically) to the MQTT server. 
// All messages are in the JSON format. 

//Including libraries for esp, dht and mpl sensors, OLED, JSON and MQTT
#include <DHT.h>
#include <DHT_U.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PubSubClient.h>   
#include <ArduinoJson.h>    
#include <ESP8266WiFi.h> 
#include <Adafruit_MPL115A2.h>
#include <Adafruit_Sensor.h>
#include "config.h"

#define mqtt_server "------"  //this is its address, unique to the server
#define mqtt_user "------"               //this is its server login, unique to the server
#define mqtt_password "-----"           //this is it server password, unique to the server

#define wifi_ssid "Fake Asians"   // Wifi Stuff
#define wifi_password "samesame" //

WiFiClient espClient;             // espClient
PubSubClient mqtt(espClient);     // tie PubSub (mqtt) client to WiFi client

char mac[6]; //A MAC address as the unique user ID!

char message[201]; //201, as last character in the array is the NULL character, denoting the end of the array

unsigned long currentMillis, previousMillis; // hold the values of timers

char espUUID[8] = "ESP8602"; // Name of the microcontroller

const int buttonPin = 2; 
int buttonState = 1;       // Keep track of button state; 1 is pressed, 0 is released
boolean buttonFlag = false; // Keep track if button state is changed

// pin connected to DH22 data line
#define DATA_PIN 12


DHT_Unified dht(DATA_PIN, DHT22); // initialize DHT sensor
Adafruit_MPL115A2 mpl; // initialize mpl pressure sensor

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // Initialize oled 

// Initialize variables for temperature, humidity, and pressure
float tem;
float hum;
float pre;

void setup() {
  // Start the serial connection
  Serial.begin(115200);

  // System status
  while(! Serial);
  Serial.print("This board is running: ");
  Serial.println(F(__FILE__));
  Serial.print("Compiled: ");
  Serial.println(F(__DATE__ " " __TIME__));

  pinMode(buttonPin, INPUT_PULLUP); // Initializing button pin

  Serial.println("initializing dht");
  dht.begin(); // Begin dht (non i2c) sensor

  Serial.println("initializing mpl");
  mpl.begin(); // Begin mpl (i2c) sensor


  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  display.display(); // Initiate the display
  delay(2000); // Pause for 2 seconds
  display.clearDisplay();
  
  mqtt.setServer(mqtt_server, 1883); // Start the mqtt
  mqtt.setCallback(callback); //Register the callback function
  
  // Initiate sensor values 
  pre = 0;
  hum = 0;
  tem = 0;
}

void loop() {
  currentMillis = millis(); // Get curremt time
  if (!mqtt.connected()) {  // Try connecting again
    reconnect();
  }

  mqtt.loop(); //this keeps the mqtt connection 'active'
  
  buttonState = digitalRead(buttonPin); // Read button state

  // If button is pressed
  if (buttonState == HIGH && buttonFlag == false) {
    Serial.println("Motion detected");
    char message[20];
    sprintf(message, "{\"uuid\": \"%s\", \"button\": \"%s\" }", espUUID, "1");
    mqtt.publish("fromMel/button", message);
    buttonFlag = true;
  }

  // If button is released
  if (buttonState == LOW && buttonFlag == true) {
    Serial.println("Area is clear.");
    char message[20];
    sprintf(message, "{\"uuid\": \"%s\", \"button\": \"%s\" }", espUUID, "0");
    mqtt.publish("fromMel/button", message);
    buttonFlag = false;
  }
  // A periodic report for every 5 seconds
  if (currentMillis - previousMillis > 5000) { 
     getSensorData();     
  }
  displayText();          // displays text on OLED
}


//function to reconnect if we become disconnected from the server
void reconnect() {
  // Loop until we're reconnected
  while (!espClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    // Attempt to connect
    if (mqtt.connect(espUUID, mqtt_user, mqtt_password)) { //the connction
      Serial.println("connected");
      // Once connected, publish an announcement...
      char announce[40];
      strcat(announce, espUUID);
      strcat(announce, "is connecting. <<<<<<<<<<<");
      mqtt.publish(espUUID, announce);
      // ... and resubscribe
      //      client.subscribe("");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic); //'topic' refers to the incoming topic name, the 1st argument of the callback function
  Serial.println("] ");

  DynamicJsonBuffer  jsonBuffer; //blah blah blah a DJB
  JsonObject& root = jsonBuffer.parseObject(payload); //parse it!

  if (!root.success()) { //well?
    Serial.println("parseObject() failed, are you sure this message is JSON formatted.");
    return;
  }
}

// Reads dht and mpl sensor data, converts to JSON and reports in MQTT
void getSensorData() {
  sensors_event_t event;       // prep a instance for grabbing data 
  dht.temperature().getEvent(&event);  // grabbing data
  tem = event.temperature;    // Storing temperature data

  Serial.print("Temperature: ");
  Serial.print(tem);
  Serial.println("C");

  dht.humidity().getEvent(&event);  // grabbing data
  hum = event.relative_humidity;  // Storing humidity data

  Serial.print("Humidity: ");
  Serial.print(hum);
  Serial.println("%");
  
  // gets pressure from mpl sensor
  pre = mpl.getPressure();      // Storing pressure data
  Serial.print("Pressure: "); 
  Serial.print(pre, 4); 
  Serial.println(" kPa");

  // Creates type char storage spaces for different sensor values
  char str_tem[7];
  char str_hum[7];
  char str_pre[6];
  char message[60];

  // Converts sensor data to string
  dtostrf(tem, 4, 2, str_tem);
  dtostrf(hum, 4, 2, str_hum);
  dtostrf(pre, 4, 2, str_pre);
  sprintf(message, "{\"temp\": \"%s\", \"humidity\": \"%s\", \"pressure\": \"%s\" }", str_tem, str_hum, str_pre);    
  mqtt.publish("fromMel/weather", message);   // publishing data to MQTT
  Serial.println("publishing");
  previousMillis = currentMillis;   // resets timer
}

// displays text on OLED
void displayText(void) { 
  // converts to string
  String sPre = String(pre);
  String sTem = String(tem); 
  String sHum = String(hum); 
  
  display.clearDisplay(); 
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(10, 0);
  display.println(sTem + "C "); // print temp and pressure
  display.setCursor(10, 13);
  display.println(sPre + "kPa "); // print temp and pressure in the next line
  display.setCursor(10, 26);
  display.println(sHum + "% "); // print humidity in the next line
  display.display();      
  delay(100);
}
