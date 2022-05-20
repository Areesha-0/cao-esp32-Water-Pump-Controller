#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include<Ultrasonic.h>
#include "SPIFFS.h"
#include <Wire.h>
#include <string.h>
#include <Preferences.h>

//network credentials
const char* ssid = "muhammadasif7";
const char* password = "asif1972";

// Stores relay state
String relay_state;
const char* PARAM_INPUT_1 = "input1"; //getting tank depth
const char* PARAM_INPUT_2 = "input2"; //getting level until which the tank should be filled

const int trig_pin = 33;
const int echo_pin = 32;
const int relay_pin = 13;

float tank_depth = 0;
float water_level = 0; //calculated using the ultrasonic sensor
float desired_waterlevel = 0; //given by user

bool motor_state; //saves to nvs

Preferences preferences;
Ultrasonic ultrasonic_sensor(trig_pin, echo_pin);

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

// Replaces placeholder with relay's state value
String processor(const String& var){
  Serial.println(var);
  if(var == "STATE"){
    if(digitalRead(relay_pin)){
      relay_state = "ON";
    }
    else{
      relay_state = "OFF";
    }
    Serial.print(relay_state);
    return relay_state;
  }
  else if(var == "WATERLEVEL"){
    return display_gauge();
  }
    else if(var == "TANKDEPTH"){
    return display_depth();
  }
  return String();
}

String get_waterlevel(){
  //read distance
  float distance_waterToTankTop = ultrasonic_sensor.read();
  water_level = tank_depth - distance_waterToTankTop ;
  water_level = (water_level / tank_depth) * 100;

  Serial.println("depth: ");
  Serial.println(tank_depth);
  
  if(isnan(distance_waterToTankTop)){
     Serial.println("Did not get any reading from the ultrasonic sensor");
     return "";   
  }
  else{ 
    if(water_level >= desired_waterlevel){ //water is about to reach the limit set
      digitalWrite(relay_pin, LOW);
      Serial.println("motor turned off at desired water level");
      preferences.putBool("state", 0);
      Serial.printf("State saved: 0");
    }
    
    if(water_level >= 90){ //water is about to overflow then turn off the motor
      digitalWrite(relay_pin, LOW);
      Serial.println("motor was about to overflow");
      preferences.putBool("state", 0);
      Serial.printf("State saved: 0");
    }

     Serial.println("desired water level (cm): ");
     Serial.println(desired_waterlevel); //for the chart
     Serial.println("water level (cm): ");
     Serial.println(distance_waterToTankTop); //for the chart
     Serial.println("water level percentage: ");
     Serial.println(water_level); //for the chart
     
     return String(distance_waterToTankTop);
  }
}

String display_gauge(){
  if(isnan(water_level)){
      Serial.println("water level not detected!");
      return "";
  }
  else {
      return String(water_level);
  } 
}

String display_depth(){
  if(isnan(tank_depth)){
      Serial.println("tank depth not detected!");
      return "";
  }
  else {
      return String(tank_depth);
  } 
}

void print_details(){
  
}
void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);

  preferences.begin("tank_depth", false);
  tank_depth = preferences.getFloat("depth",0); //returns default value 0 if it does not have depth 
  
 //Create a namespace called "motor_state"
  preferences.begin("motor_state", false); //read write mode
  motor_state = preferences.getBool("state", false);   // read the last motor state from flash memory

  //relay is in NC configuration
  pinMode(relay_pin, OUTPUT);
  digitalWrite(relay_pin, motor_state);


  // Initialize SPIFFS
  if (! SPIFFS.begin ()) {
    Serial.println ("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

   server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html");
  });

  server.on ("/distance", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request-> send_P(200, "text / plain", get_waterlevel(). c_str ());
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  // Route to set relay to HIGH
  server.on("/motor_on", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(relay_pin, HIGH); 
       
    // save motor's state in flash memory
    preferences.putBool("state", 1);
    Serial.printf("State saved: 1");
    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  // Route to set GPIO to LOW
  server.on("/motor_off", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(relay_pin, LOW);    
    // save the LED state in flash memory
    preferences.putBool("state", 0);
    Serial.printf("State saved: 0");
    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  server.on("/water_level", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", display_gauge().c_str());
  });
  
  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      
      Serial.println("depth of the tank is: ");
      Serial.println(inputMessage);
      
      tank_depth = inputMessage.toFloat(); //setting tank depth for program
      
      preferences.putFloat("depth", tank_depth); //saving depth given by the user
    }

    else if (request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_2)->value();
      inputParam = PARAM_INPUT_2;
      
      Serial.println("Turn of the motor at water level (%): ");
      Serial.println(inputMessage);
      
      desired_waterlevel = inputMessage.toFloat(); //saving water level
    }
    
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    request->send(200, "text/html", "HTTP GET request sent to your ESP on input field (" 
                                     + inputParam + ") with value: " + inputMessage +
                                     "<br><a href=\"/\">Return to Home Page</a>");
  });
  // Start server
  server.onNotFound(notFound);
  server.begin();
}
  
void loop() {
}
