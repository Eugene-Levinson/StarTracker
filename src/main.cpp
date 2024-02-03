#include <Arduino.h>
#include <AccelStepper.h>
#include <WiFi.h>
#include "esp_task_wdt.h"

// Stepper motor definitions
#define STEPS_PER_REVOLUTION 2048
#define IN1 15
#define IN2 2
#define IN3 4
#define IN4 5

#define MotorInterfaceType 4

// Initialize stepper
AccelStepper myStepper(MotorInterfaceType, IN1, IN3, IN2, IN4);

// State
enum State {
  IDLE,
  RotatingCCW,
};

State state;

// Task for core 2 to run stepper motor timing
TaskHandle_t Task1;
TaskHandle_t Task2;

// Wifi configuration
const char* ssid = "Eugene's";
const char* password = "12345678";

IPAddress ip(192, 168, 1, 69);
IPAddress dns(192, 168, 0, 1);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

WiFiServer espServer(80);
bool ServerOn = false;
String request;

// Core Task Function to handle motor control
void HandleMotorControl( void * pvParameters ){
  Serial.print("Motor Control Running on Core:");
  Serial.println(xPortGetCoreID());

  for(;;){

      // State machine
      switch (state) {
        
        case RotatingCCW:
          myStepper.setSpeed(128);
          myStepper.runSpeed();
          break;

        default:
          myStepper.setSpeed(0);
          myStepper.runSpeed();
          break;
      }

  } 
}


void connectedToWifiHandler(WiFiEvent_t event){
  Serial.println("Connected to WiFi");
  Serial.println("Getting IP Address... ");
  
  delay(1000);

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.println("HTTP server started");
  Serial.println("Starting ESP32 Web Server...");
  espServer.begin(); /* Start the HTTP web Server */
  ServerOn = true;
  Serial.println("ESP32 Web Server Started");
  Serial.print("\n");
  Serial.print("The URL of ESP32 Web Server is: ");
  Serial.print("http://");
  Serial.println(WiFi.localIP());
  Serial.print("\n");
  Serial.println("Use the above URL in your Browser to access ESP32 Web Server\n");

  state = RotatingCCW;
}

void disconnectedFromWifiHandler(WiFiEvent_t event){
  Serial.println("Disconnected from WiFi");

  if (ServerOn){
    espServer.stop();
    Serial.println("HTTP server stopped");
    ServerOn = false;
  }

  state = IDLE;
}

String SendHTML(){
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>Hello World</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr +="p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr +="<h1>Hello World</h1>\n";
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}

// Core Task Function to handle HTTP Server
void HandleHTTPServer( void * pvParameters ){
  Serial.print("HTTP Server Running on Core:");
  Serial.println(xPortGetCoreID());

  for(;;){
      WiFiClient client = espServer.available(); /* Check if a client is available */

    if (client) {
      Serial.println("New Connection!");
      boolean currentLineIsBlank = true;

      while (client.connected()) {
        if (client.available()){
          char c = client.read();
          request += c;
          Serial.write(c);
          
          // If request is complete - end of the line and line is blank
          if (c == '\n' && currentLineIsBlank){
            
            /* HTTP Response in the form of HTML Web Page */
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println(); //  IMPORTANT

            client.println("<!DOCTYPE HTML>");
            client.println("<html>");
            
            client.println("<head>");
            client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            
            client.println("<style>");
            
            client.println("html { font-family: Courier New; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button {border: none; color: white; padding: 10px 20px; text-align: center;");
            client.println("text-decoration: none; font-size: 25px; margin: 2px; cursor: pointer;}");
            client.println(".button1 {background-color: #FF0000;}");
            client.println(".button2 {background-color: #00FF00;}");
            
            client.println("</style>");
            client.println("</head>");
            client.println("<body>");
            client.println("<h2>ESP32 Web Server</h2>");
            client.println("</body>");
            client.println("</html>");
            
            break; 
          }

          if(c == '\n'){
            currentLineIsBlank = true;
          } else if(c != '\r'){
            currentLineIsBlank = false;
          }
        }
      }
    
      delay(1);
      request = "";
      //client.flush();
      client.stop();
      Serial.println("Client disconnected");
      Serial.print("\n");

    }
  } 
}


// Setup
void setup() {
  Serial.begin(9600);
  
  // Configure wifi
  //WiFi.config(ip, gateway, subnet, dns);
  WiFi.mode(WIFI_STA);

  WiFi.onEvent(connectedToWifiHandler, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(disconnectedFromWifiHandler, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.begin(ssid, password);
  
  // Configure stepper
  myStepper.setMaxSpeed(1000);
  myStepper.setAcceleration(500);

  // Set initial state
  state = IDLE;

    //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
                    HandleMotorControl,   /* Task function. */
                    "MotorControl",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    tskIDLE_PRIORITY,           /* priority of the task  tskIDLE_PRIORITY*/
                    &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(500); 

  //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
                    HandleHTTPServer,   /* Task function. */
                    "HTTP Server",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task2,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */
    delay(500); 

}


void loop() {
}

