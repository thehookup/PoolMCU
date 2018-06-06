#include <SimpleTimer.h>  //https://github.com/jfturcot/SimpleTimer
#include <ESP8266WiFi.h>
#include <PubSubClient.h>  //https://github.com/knolleary/pubsubclient
#include <ESP8266mDNS.h> 
#include <WiFiUdp.h>
#include <ArduinoOTA.h>  //https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA


//USER CONFIGURED SECTION START//
const char* ssid = "YOUR_WIRELESS_SSID";
const char* password = "YOUR_WIRELESS_SSID";
const char* mqtt_server = "YOUR_MQTT_SERVER_ADDRESS";
const int mqtt_port = 1883;
const char *mqtt_user = "YOUR_MQTT_USERNAME";
const char *mqtt_pass = "YOUR_MQTT_PASSWORD";
const char *mqtt_client_name = "PoolMCU"; // Client connections can't have the same connection name
//USER CONFIGURED SECTION END//

WiFiClient espClient;
PubSubClient client(espClient);
SimpleTimer timer;

// Pins
const int cleanerPin = 15; //marked as D8 on the board
const int spaLightPin = 13;  //marked as D7 on the board
const int poolLightPin = 12; //marked as D6 on the board
const int tempPin = A0;//marked as A0 on the board
const int modePin = 4;  //marked as D2 on the board
const int modeSensePin = 14; //marked as D5 on the board

// Variables
String currentStatus = "POOL";
String newStatus = "POOL";
int gateOldStatus = 1;
int oldTemp = 0;
String oldStatus = "0";
char temperature[50];
char poolMode[50];
bool boot = true;
bool checkDebounce = false;
int newGate = 1;
bool connectWifi();

//Functions
void setup_wifi() 
{
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() 
{
  int retries = 0;
  while (!client.connected()) {
    if(retries < 5)
    {
      Serial.print("Attempting MQTT connection...");
      if (client.connect(mqtt_client_name, mqtt_user, mqtt_pass)) 
      {
        Serial.println("connected");
        if(boot == false)
        {
          client.publish("checkIn/poolMCU", "Reconnected"); 
        }
        if(boot == true)
        {
          client.publish("checkIn/poolMCU", "Rebooted");
          boot = false;
        }
        client.subscribe("commands/pool");
      } 
      else 
      {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        retries++;
        delay(5000);
      }
    }
    if(retries > 5)
    {
    ESP.restart();
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  Serial.print("Message arrived [");
  String newTopic = topic;
  Serial.print(topic);
  Serial.print("] ");
  payload[length] = '\0';
  String newPayload = String((char *)payload);
  Serial.println(newPayload);
  Serial.println();
  if (newTopic == "commands/pool") 
    {
      if (currentStatus == "SPA" && newPayload == "POOL")
      {
        digitalWrite(modePin, HIGH);
        delay(800);
        digitalWrite(modePin, LOW);
        delay(2000);
        currentStatus = "POOL";
      }
      if (currentStatus == "POOL" && newPayload == "SPA")
      {
        digitalWrite(modePin, HIGH);
        delay(800);
        digitalWrite(modePin, LOW);
        delay(2000);
        currentStatus = "SPA";
      }
      if(newPayload == "Pool_Light")
      {
        digitalWrite(poolLightPin, HIGH);
        Serial.println("Sending Pool Light");
        delay(800);
        digitalWrite(poolLightPin, LOW);
      }

      if(newPayload == "Spa_Light")
      {
        digitalWrite(spaLightPin, HIGH);
        Serial.println("Sending Spa Light");
        delay(800);
        digitalWrite(spaLightPin, LOW);
      }
      if(newPayload == "Cleaner")
      {
        digitalWrite(cleanerPin, HIGH);
        Serial.println("Sending Cleaner");
        delay(800);
        digitalWrite(cleanerPin, LOW);
      }
    }
}

void getPoolMode()
{
  if (pulseIn(modeSensePin, HIGH, 3000000) > 100)
  {
    currentStatus = "SPA";
    if(currentStatus != oldStatus)
    {
      client.publish("pool/mode","SPA", true);
      oldStatus = currentStatus;
    }
  }
  else if(digitalRead(modeSensePin) == HIGH)
  {
    currentStatus = "SPA";
    if(currentStatus != oldStatus)
    {
      client.publish("pool/mode","SPA", true);
      oldStatus = currentStatus;
    }
  }
  else 
  {
    currentStatus = "POOL";
    if(currentStatus != oldStatus)
    {
      client.publish("pool/mode","POOL", true);
      oldStatus = currentStatus;
    }
  }
}


void checkIn()
{
  client.publish("checkIn/poolMCU","OK");
}


void getTemperature() {
  if(checkDebounce == false)
  {
    uint8_t i;
    float average;
    int samples[50];
    float numerator = 1;
    float denominator = 1;
   
    // take 50 samples in a row, with a slight delay
    for (i=0; i< 50; i++) {
     samples[i] = analogRead(tempPin);
     delay(10);
    }
   
    // average all the samples out
    average = 0;
    for (i=0; i< 50; i++) {
       average += samples[i];
    }
    average /= 50;
 
    // convert the value to resistance
    numerator = -31000 * average;
    denominator = (3.3 * average) -5155;  // might need to change -5155 to 5*max ADC value
    average = numerator / denominator;
    Serial.print("Thermistor resistance "); 
    Serial.println(average);
  
    // convert to temperature in F
    float steinhart;
    steinhart = average / 10000;     // (R/Ro)
    steinhart = log(steinhart);                  // ln(R/Ro)
    steinhart /= 3915;                   // 1/B * ln(R/Ro)
    steinhart += 1.0 / (25 + 273.15); // + (1/To)
    steinhart = 1.0 / steinhart;                 // Invert
    steinhart -= 273.15;                         // convert to C
    int tempcalc = steinhart * 9 / 5 + 32;           // convert to F
    String temp_str = String(tempcalc);
    temp_str.toCharArray(temperature, temp_str.length() + 1);
    client.publish("pool/temperature", temperature);  
  }
}


void setup() 
{
  Serial.begin(115200);
  // GPIO Pin Setup
  pinMode(tempPin, INPUT);
  pinMode(modeSensePin, INPUT);
  pinMode(modePin, OUTPUT);
  pinMode(spaLightPin, OUTPUT);
  pinMode(poolLightPin, OUTPUT);
  pinMode(cleanerPin, OUTPUT);
  digitalWrite(cleanerPin, LOW);
  digitalWrite(modePin, LOW);
  digitalWrite(spaLightPin, LOW);
  digitalWrite(poolLightPin, LOW);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  ArduinoOTA.setHostname("PoolMCU");
  ArduinoOTA.begin(); 
  timer.setInterval(30000, getTemperature);
  timer.setInterval(3000, getPoolMode);
  timer.setInterval(120000, checkIn);   
}

void loop() 
{
  if (!client.connected()) 
  {
    reconnect();
  }
  client.loop();
  ArduinoOTA.handle();
  timer.run();
}


