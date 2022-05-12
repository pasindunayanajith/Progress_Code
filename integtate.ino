#ifdef ENABLE_DEBUG
       #define DEBUG_ESP_PORT Serial
       #define NODEBUG_WEBSOCKETS
       #define NDEBUG
#endif 

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "SinricPro.h"
#include "SinricProSwitch.h"

#include <map>


// Update HOST URL here
#define HOST "zero24xplk3.000webhostapp.com" 

#define WIFI_SSID         "Dialog 4G 151"    
#define WIFI_PASSWORD     "21b2FDf3"
#define APP_KEY           "754cbb53-5ed7-4fa8-82ca-56e011051c08"      // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET        "ea9a493c-1077-4470-b941-3858fdcbe4a3-6acffc87-8ee2-47a3-bd67-2722cd75a69d"   // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"


#define Signal_Pin 13  //D7
#define Signal_Pin_Switch_1 15 //D8
#define Signal_Pin_Switch_2 16//D0
#define Signal_Pin_Switch_3 12 //D6
//#define Signal_Pin_Switch_4 13 //D7

//Enter the device IDs here
#define device_ID_1   "623416c2753dc5aab49305a5"
#define device_ID_2   "626ca665d0fd258c521db827"
#define device_ID_3   "626ca9c5d0fd258c521db8cb"
#define device_ID_4   "SWITCH_ID_NO_4_HERE"

// define the GPIO connected with Relays and switches
#define RelayPin1 5  //D1
#define RelayPin2 4  //D2
#define RelayPin3 14 //D5
#define RelayPin4 12 //D6

#define SwitchPin1 10  //SD3
#define SwitchPin2 0   //D3 
#define SwitchPin3 13  //D7
#define SwitchPin4 3   //RX


#define wifiLed   16   //D0

// Declare global variables which will be uploaded to server
String sendval, sendval2, postData,user,message,device,postDataA1,postDataA2,postDataA3,postDataA4;

#define BAUD_RATE   115200

#define DEBOUNCE_TIME 250

typedef struct {      // struct for the std::map below
  int relayPIN;
  int flipSwitchPIN;
} deviceConfig_t;


// this is the main configuration
// please put in your deviceId, the PIN for Relay and PIN for flipSwitch
// this can be up to N devices...depending on how much pin's available on your device ;)
// right now we have 4 devicesIds going to 4 relays and 4 flip switches to switch the relay manually
std::map<String, deviceConfig_t> devices = {
    //{deviceId, {relayPIN,  flipSwitchPIN}}
    {device_ID_1, {  RelayPin1, SwitchPin1 }},
    {device_ID_2, {  RelayPin2, SwitchPin2 }},
    {device_ID_3, {  RelayPin3, SwitchPin3 }},
    {device_ID_4, {  RelayPin4, SwitchPin4 }}     
};

typedef struct {      // struct for the std::map below
  String deviceId;
  bool lastFlipSwitchState;
  unsigned long lastFlipSwitchChange;
} flipSwitchConfig_t;

std::map<int, flipSwitchConfig_t> flipSwitches;    // this map is used to map flipSwitch PINs to deviceId and handling debounce and last flipSwitch state checks
                                                  // it will be setup in "setupFlipSwitches" function, using informations from devices map

void setupRelays() { 
  for (auto &device : devices) {           // for each device (relay, flipSwitch combination)
    int relayPIN = device.second.relayPIN; // get the relay pin
    pinMode(relayPIN, OUTPUT);             // set relay pin to OUTPUT
    digitalWrite(relayPIN, HIGH);
  }
}

void setupFlipSwitches() {
  for (auto &device : devices)  {                     // for each device (relay / flipSwitch combination)
    flipSwitchConfig_t flipSwitchConfig;              // create a new flipSwitch configuration

    flipSwitchConfig.deviceId = device.first;         // set the deviceId
    flipSwitchConfig.lastFlipSwitchChange = 0;        // set debounce time
    flipSwitchConfig.lastFlipSwitchState = true;     // set lastFlipSwitchState to false (LOW)--

    int flipSwitchPIN = device.second.flipSwitchPIN;  // get the flipSwitchPIN

    flipSwitches[flipSwitchPIN] = flipSwitchConfig;   // save the flipSwitch config to flipSwitches map
    pinMode(flipSwitchPIN, INPUT_PULLUP);                   // set the flipSwitch pin to INPUT
  }
}

bool onPowerState(String deviceId, bool &state)
{
  Serial.printf("%s: %s\r\n", deviceId.c_str(), state ? "on" : "off");
  int relayPIN = devices[deviceId].relayPIN; // get the relay pin for corresponding device
  digitalWrite(relayPIN, !state);             // set the new relay state
  return true;
}

void handleFlipSwitches() {
  unsigned long actualMillis = millis();                                          // get actual millis
  for (auto &flipSwitch : flipSwitches) {                                         // for each flipSwitch in flipSwitches map
    unsigned long lastFlipSwitchChange = flipSwitch.second.lastFlipSwitchChange;  // get the timestamp when flipSwitch was pressed last time (used to debounce / limit events)

    if (actualMillis - lastFlipSwitchChange > DEBOUNCE_TIME) {                    // if time is > debounce time...

      int flipSwitchPIN = flipSwitch.first;                                       // get the flipSwitch pin from configuration
      bool lastFlipSwitchState = flipSwitch.second.lastFlipSwitchState;           // get the lastFlipSwitchState
      bool flipSwitchState = digitalRead(flipSwitchPIN);                          // read the current flipSwitch state
      if (flipSwitchState != lastFlipSwitchState) {                               // if the flipSwitchState has changed...
#ifdef TACTILE_BUTTON
        if (flipSwitchState) {                                                    // if the tactile button is pressed 
#endif      
          flipSwitch.second.lastFlipSwitchChange = actualMillis;                  // update lastFlipSwitchChange time
          String deviceId = flipSwitch.second.deviceId;                           // get the deviceId from config
          int relayPIN = devices[deviceId].relayPIN;                              // get the relayPIN from config
          bool newRelayState = !digitalRead(relayPIN);                            // set the new relay State
          digitalWrite(relayPIN, newRelayState);                                  // set the trelay to the new state

          SinricProSwitch &mySwitch = SinricPro[deviceId];                        // get Switch device from SinricPro
          mySwitch.sendPowerStateEvent(!newRelayState);                            // send the event
#ifdef TACTILE_BUTTON
        }
#endif      
        flipSwitch.second.lastFlipSwitchState = flipSwitchState;                  // update lastFlipSwitchState
      }
    }
  }
}

void setupWiFi()
{
  Serial.printf("\r\n[Wifi]: Connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.printf(".");
    delay(250);
  }
  //digitalWrite(wifiLed, LOW);
  Serial.printf("connected!\r\n[WiFi]: IP-Address is %s\r\n", WiFi.localIP().toString().c_str());
}

void setupSinricPro()
{
  for (auto &device : devices)
  {
    const char *deviceId = device.first.c_str();
    SinricProSwitch &mySwitch = SinricPro[deviceId];
    mySwitch.onPowerState(onPowerState);
  }

  SinricPro.begin(APP_KEY, APP_SECRET);
  SinricPro.restoreDeviceStates(true);


  
}

void setup()
{

    
  Serial.begin(BAUD_RATE);

  
  //indicator detection
   pinMode(Signal_Pin , INPUT_PULLUP);
   pinMode(Signal_Pin_Switch_1 , INPUT_PULLUP);
   pinMode(Signal_Pin_Switch_2 , INPUT_PULLUP);
   pinMode(Signal_Pin_Switch_3 , INPUT_PULLUP);
   //pinMode(Signal_Pin_Switch_4 , INPUT_PULLUP);
  pinMode(wifiLed, OUTPUT);  
  digitalWrite(wifiLed, HIGH);

  setupRelays();
 setupFlipSwitches();
  setupWiFi();
  setupSinricPro();
  
   Serial.begin(115200); 
  Serial.println("Communication Started \n\n");  
  //delay(1000);
    
 

 
  
 


  
}

void loop()
{
//  SinricPro.handle();
  //handleFlipSwitches();

  detector();

  
}


void detector(){
    SinricPro.handle();

  if ( digitalRead(Signal_Pin) == 0 ){
       Serial.println ("Main Line Power Activate");
       sendval="Main Line Power Activate";
       sendval2="Main Line Power Activate";
     
       HTTPClient http;    // http object of clas HTTPClient   
       postData = "sendval=" + sendval + "&sendval2=" + sendval2;
  
       http.begin("http://zero24xplk3.000webhostapp.com/dbwrite.php");              // Connect to host where MySQL databse is hosted
       http.addHeader("Content-Type", "application/x-www-form-urlencoded");            //Specify content-type header

       int httpCode = http.POST(postData);   // Send POST request to php file and store server response code in variable named httpCode
       Serial.println("Values are, sendval = " + sendval + " and sendval2 = "+sendval2 );
  
       // if connection eatablished then do this
       if (httpCode == 200) { Serial.println("Values uploaded successfully."); Serial.println(httpCode); 
        String webpage = http.getString();    // Get html webpage output and store it in a string
        Serial.println(webpage + "\n"); 
      }
      // if failed to connect then return and restart
      else { 
      Serial.println(httpCode); 
      Serial.println("Failed to upload values. \n"); 
      http.end(); 
      return; }
   
  }else{

       Serial.println ("Main Line Power Dectivate");
       sendval="Power Cut Detected";
       sendval2="Power Cut Detected";
     
       HTTPClient http;    // http object of clas HTTPClient   
       postData = "sendval=" + sendval + "&sendval2=" + sendval2;
  
       http.begin("http://zero24xplk3.000webhostapp.com/dbwrite.php");              // Connect to host where MySQL databse is hosted
       http.addHeader("Content-Type", "application/x-www-form-urlencoded");            //Specify content-type header

       int httpCode = http.POST(postData);   // Send POST request to php file and store server response code in variable named httpCode
       Serial.println("Values are, sendval = " + sendval + " and sendval2 = "+sendval2 );
  
       // if connection eatablished then do this
       if (httpCode == 200) { Serial.println("Values uploaded successfully."); Serial.println(httpCode); 
        String webpage = http.getString();    // Get html webpage output and store it in a string
        Serial.println(webpage + "\n"); 
      }
      // if failed to connect then return and restart
      else { 
      Serial.println(httpCode); 
      Serial.println("Failed to upload values. \n"); 
      http.end(); 
      return; }
    
  }
  //************************************Indicator Detection Finish**********************************************************************************
    SinricPro.handle();

  //************************Relay Switch-01********************************************************************************************************
  if ( digitalRead(Signal_Pin_Switch_1) == 0 ){

       Serial.println ("Relay01 Power Activate");
       message="Relay01 Power Activate";
       user="199827002100";
       device="01";
     
       HTTPClient http1;    // http object of clas HTTPClient   
       postDataA1 = "message=" + message + "&user=" + user + "&device=" + device;

       http1.begin("http://zero24xplk3.000webhostapp.com/dbwrite.php");              // Connect to host where MySQL databse is hosted
       http1.addHeader("Content-Type", "application/x-www-form-urlencoded");            //Specify content-type header

       int httpCode1 = http1.POST(postDataA1);   // Send POST request to php file and store server response code in variable named httpCode
       Serial.println("Values are, message=" + message + "and user=" + user + "and device" + device );
  
       // if connection eatablished then do this
       if (httpCode1 == 200) { Serial.println("Values uploaded successfully."); Serial.println(httpCode1); 
        String webpage1 = http1.getString();    // Get html webpage output and store it in a string
        Serial.println(webpage1 + "\n"); 
      }
      // if failed to connect then return and restart
      else { 
      Serial.println(httpCode1); 
      Serial.println("Failed to upload values. \n"); 
      http1.end(); 
      return; }


    
  }else{
       Serial.println ("Relay01 Power Dectivate");
       message="Relay01 Power Deactivate";
       user="199827002100";
       device="01";
     
       HTTPClient http1;    // http object of clas HTTPClient   
       postDataA1 = "message=" + message + "&user=" + user + "&device=" + device;

       http1.begin("http://zero24xplk3.000webhostapp.com/offdbwrite.php");              // Connect to host where MySQL databse is hosted
       http1.addHeader("Content-Type", "application/x-www-form-urlencoded");            //Specify content-type header

       int httpCode1 = http1.POST(postDataA1);   // Send POST request to php file and store server response code in variable named httpCode
       Serial.println("Values are, message=" + message + "and user=" + user + "and device" + device );
  
       // if connection eatablished then do this
       if (httpCode1 == 200) { Serial.println("Values uploaded successfully."); Serial.println(httpCode1); 
        String webpage1 = http1.getString();    // Get html webpage output and store it in a string
        Serial.println(webpage1 + "\n"); 
      }
      // if failed to connect then return and restart
      else { 
      Serial.println(httpCode1); 
      Serial.println("Failed to upload values. \n"); 
      http1.end(); 
      return; }
     
  }

    //************************Relay Switch-02********************************************************************************************************
    SinricPro.handle();

  if ( digitalRead(Signal_Pin_Switch_2) == 0 ){

     Serial.println ("Relay02 Power Activate");
       message="Relay02 Power Activate";
       user="199827002100";
       device="02";
     
       HTTPClient http2;    // http object of clas HTTPClient   
       postDataA2 = "message=" + message + "&user=" + user + "&device=" + device;

       http2.begin("http://zero24xplk3.000webhostapp.com/dbwrite.php");              // Connect to host where MySQL databse is hosted
       http2.addHeader("Content-Type", "application/x-www-form-urlencoded");            //Specify content-type header

       int httpCode2 = http2.POST(postDataA2);   // Send POST request to php file and store server response code in variable named httpCode
       Serial.println("Values are, message=" + message + "and user=" + user + "and device" + device );
  
       // if connection eatablished then do this
       if (httpCode2 == 200) { Serial.println("Values uploaded successfully."); Serial.println(httpCode2); 
        String webpage2 = http2.getString();    // Get html webpage output and store it in a string
        Serial.println(webpage2 + "\n"); 
      }
      // if failed to connect then return and restart
      else { 
      Serial.println(httpCode2); 
      Serial.println("Failed to upload values. \n"); 
      http2.end(); 
      return; }


  }else{

      Serial.println ("Relay02 Power Deactivate");
       message="Relay02 Power Deactivate";
       user="199827002100";
       device="02";
     
       HTTPClient http2;    // http object of clas HTTPClient   
       postDataA2 = "message=" + message + "&user=" + user + "&device=" + device;

       http2.begin("http://zero24xplk3.000webhostapp.com/offdbwrite.php");              // Connect to host where MySQL databse is hosted
       http2.addHeader("Content-Type", "application/x-www-form-urlencoded");            //Specify content-type header

       int httpCode2 = http2.POST(postDataA2);   // Send POST request to php file and store server response code in variable named httpCode
       Serial.println("Values are, message=" + message + "and user=" + user + "and device" + device );
  
       // if connection eatablished then do this
       if (httpCode2 == 200) { Serial.println("Values uploaded successfully."); Serial.println(httpCode2); 
        String webpage2 = http2.getString();    // Get html webpage output and store it in a string
        Serial.println(webpage2 + "\n"); 
      }
      // if failed to connect then return and restart
      else { 
      Serial.println(httpCode2); 
      Serial.println("Failed to upload values. \n"); 
      http2.end(); 
      return; }

    
  }
  //************************Relay Switch-03********************************************************************************************************
    SinricPro.handle();

  if ( digitalRead(Signal_Pin_Switch_3) == 0 ){
     
     
   Serial.println ("Relay03 Power Activate");
       message="Relay03 Power Activate";
       user="199827002100";
       device="03";
     
       HTTPClient http3;    // http object of clas HTTPClient   
       postDataA3 = "message=" + message + "&user=" + user + "&device=" + device;

       http3.begin("http://zero24xplk3.000webhostapp.com/dbwrite.php");              // Connect to host where MySQL databse is hosted
       http3.addHeader("Content-Type", "application/x-www-form-urlencoded");            //Specify content-type header

       int httpCode3 = http3.POST(postDataA3);   // Send POST request to php file and store server response code in variable named httpCode
       Serial.println("Values are, message=" + message + "and user=" + user + "and device" + device );
  
       // if connection eatablished then do this
       if (httpCode3 == 200) { Serial.println("Values uploaded successfully."); Serial.println(httpCode3); 
        String webpage3 = http3.getString();    // Get html webpage output and store it in a string
        Serial.println(webpage3 + "\n"); 
      }
      // if failed to connect then return and restart
      else { 
      Serial.println(httpCode3); 
      Serial.println("Failed to upload values. \n"); 
      http3.end(); 
      return; }


    
  }else{

   Serial.println ("Relay03 Power Dectivate");
       message="Relay03 Power Deactivate";
       user="199827002100";
       device="03";
     
       HTTPClient http3;    // http object of clas HTTPClient   
       postDataA3 = "message=" + message + "&user=" + user + "&device=" + device;

       http3.begin("http://zero24xplk3.000webhostapp.com/offdbwrite.php");              // Connect to host where MySQL databse is hosted
       http3.addHeader("Content-Type", "application/x-www-form-urlencoded");            //Specify content-type header

       int httpCode3 = http3.POST(postDataA3);   // Send POST request to php file and store server response code in variable named httpCode
       Serial.println("Values are, message=" + message + "and user=" + user + "and device" + device );
  
       // if connection eatablished then do this
       if (httpCode3 == 200) { Serial.println("Values uploaded successfully."); Serial.println(httpCode3); 
        String webpage3 = http3.getString();    // Get html webpage output and store it in a string
        Serial.println(webpage3 + "\n"); 
      }
      // if failed to connect then return and restart
      else { 
      Serial.println(httpCode3); 
      Serial.println("Failed to upload values. \n"); 
      http3.end(); 
      return; }


    
  }
 
  
  }
