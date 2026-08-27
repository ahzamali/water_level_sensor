/*
  This is distance measure and publish. Uses deep sleep functionality to save power,
  All initialization happens in the setup program
  Version two, tried to use BME280 temperature and Humidity sensor not completely implemented. 
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "time.h"
#include <DHT.h>
#include <Adafruit_BME280.h>   // include Adafruit BME280 sensor library



// WIFI related settings
const char* ssid = "YOUR_WIFI_SSID"; // Enter your WiFi name
const char* password =  "YOUR_WIFI_PASSWORD"; // Enter WiFi password

const char* mqttServer1 = "YOUR_MQTT_BROKER_IP";
const char* mqttServer2 = "YOUR_MQTT_BROKER_HOSTNAME";

const int mqttPort = 1883;
const char* mqttUser = "";
const char* mqttPassword = "";
//NTP related
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;


WiFiClient espClient;
PubSubClient client(espClient);


//const int LED_PORT = D5;  // Test Led port

// distance calculations
const int trigger = D7;  // the GPIO that is connected to the Trigger Pin of the Ultrasound Sensor
const int echo = D6;     // the GPIO that is connected to the Echo Pin of the Ultrasound Sensor
const int DHTPIN = D5;   // Temperature Sensor Port

char speed_of_sound[6] = "342";          // default, m/s
long distance_to_water = 0;
long duration = 0;
#define waterlevel_topic "roof/tank_water/level"
#define waterlevel_raw "roof/tank_water/distance"
#define debug "roof/log"
long max_tank_depth = 98;
long usable_lenth = 75;

// define device I2C address: 0x76 or 0x77 (0x77 is library default address)
#define BME280_I2C_ADDRESS  0x76
// initialize Adafruit BME280 library
Adafruit_BME280  bme280;

char data[256];
// DHT
#define humidity_topic "roof/sensor/humidity"
#define temperature_topic "roof/sensor/temperature"
DHT dht (DHTPIN, DHT11);

// This function sends Arduino’s up time every second to Virtual Pin (5).
// In the app, Widget’s reading frequency should be set to PUSH. This means
// that you define how often to send data to Blynk App.
void sendSensor()
{
  Serial.println("Sensing Temperature");

  // read temperature, humidity and pressure from the BME280 sensor
  float temp = bme280.readTemperature();    // get temperature in degree Celsius
  float hum = bme280.readHumidity();       // get humidity in rH%
  float pres = bme280.readPressure();       // get pressure in Pa
  
  if (isnan(hum) || isnan(temp)) {
    Serial.println("Failed to read from DHT sensor!");
    client.publish(debug, "Failed to read from DHT");
    return;
  }
  // send to mqtt server
  client.publish(temperature_topic, String(temp).c_str(), true);
  client.publish(humidity_topic, String(hum).c_str(), true);
  Serial.println("Temperature is " + String(temp) + " humidity " + String(hum));
  sprintf(data, "Temperature and humidity is %f and %f" , temp, hum);
  client.publish(debug, data);
}
// the setup function runs once when you press reset or power the board
void setup() {


    // initialize the BME280 sensor
  if( bme280.begin(BME280_I2C_ADDRESS) == 0 )
  {  // connection error or device address wrong!
    Serial.println("Connection Error");
    while(1)  // stay here
      delay(1000);
  }

  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(trigger, OUTPUT);
  pinMode(echo, INPUT);
  //  pinMode(LED_PORT, OUTPUT);
  // Enable serial port
  Serial.begin(115200);
  Serial.println("Setup Complete");
  // Enable Wifi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");
  Serial.println("IP address is " + WiFi.localIP());

  //client.setCallback(callback);

  int retryCount = 0;
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    // Connect to MQTT server
    client.setServer(mqttServer1, mqttPort);
    if (client.connect("roof_sensor", mqttUser, mqttPassword )) {
      Serial.println("MQTT connected... ");
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      Serial.print("Trying Second Server");
      client.setServer(mqttServer2, mqttPort);
      if (client.connect("roof_sensor", mqttUser, mqttPassword )) {
        Serial.println("MQTT connected... ");
      } else {      
        Serial.print("failed to connect to second with state ");
        Serial.print(client.state());
        delay(2000);
        retryCount++;
      }
    }
    if (retryCount == 3) {
      // if not reachable sleep and try latter 
      ESP.deepSleep(300e6); // 10 min sleep
    }
  }


  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  //temp sensor
  dht.begin();
  sendSensor();
  // water level
  sendWaterLevel();
  Serial.println("Deep Sleep");
  ESP.deepSleep(300e6); // 10 min sleep
  //ESP.deepSleep(300000);  // test short sleep
}

// the loop function runs over and over again forever
void sendWaterLevel() {
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  float dist = 0;
  float dist1 = calculateDistance();
  float dist2 = calculateDistance();
  float dist3 = calculateDistance();
  dist = (dist1+dist2+dist3)/3;
  sprintf(data, "Time: , %0.2f cm", dist);
  // calculate percentage of water remaining
  float percent = (max_tank_depth - dist) / usable_lenth * 100;
  client.publish(waterlevel_topic, String(percent).c_str(), true);
  Serial.println("Absolute water level is " + String(percent));
  // send absolute value as well for debug
  client.publish(waterlevel_raw, data, true);
  client.publish(debug, data);
  delay(100); // give time to publish
}

// Function That calcualates the distance on
float calculateDistance () {
  float distance = 0;
  // write to trigger
  digitalWrite(trigger, LOW);
  delayMicroseconds(2);
  digitalWrite(trigger, HIGH);
  // delay(2000);
  delayMicroseconds(10);
  digitalWrite(trigger, LOW);
  // Read the Echo
  duration = pulseIn(echo, HIGH, 60000);
  // Serial.println("Pulse Duration read is "  + String(duration));
  distance = (duration / 2) * (String(speed_of_sound)).toFloat() / 10000.0 ;
  Serial.println("Distance to Water  "  + String(distance));
  return distance;
}

/*
   loop doesn't work in the sleep mode
*/
void loop() {
  // sleeploop();
}
