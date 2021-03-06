/***************************************************************************
  This is an example of program for connected the Adafruit Huzzah ESP32 Feather  
  to the Medium One Prototyping Sandbox.  Visit www.medium.one for more information.
  Author: Medium One
  Last Revision Date: June 26, 2018

  The program includes a library and portions of sample code from Adafruit
  with their description below:
  
  This is a library for SparkFun RFID Starter Kit (ID-12LA RFID Scanner)

  Adafruit invests time and resources providing this open source code,
  please support Adafruit andopen-source hardware by purchasing products
  from Adafruit!

  Written by Limor Fried & Kevin Townsend for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ***************************************************************************/

/*
 *  Revisions:
 *  6/19 (v11) : the first release version 
 */



/*-------------------------------------------------------------------------*
 *          Includes: 
 *-------------------------------------------------------------------------*/
 
#include <PubSubClient.h>
#include <WiFi.h>        // new ESP32 WiFi library header's name
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <SPI.h>
#include "freertos/event_groups.h"  // Use the event group feature
/*-------------------------------------------------------------------------*
 * Constants:
 *-------------------------------------------------------------------------*/
/* define RTOS event bits (32 bits max)*/
#define LED_REUEST_EVENT_BIT        ( 1 << 0 ) // Bit 0 LED event
#define SEND_RFID_KEY_REQUEST_EVENT_BIT   ( 1 << 1 ) // Bit 1 New Card Key upload event


#define LEDTOTAL   2   // Total number of LED 
#define LEDRED     1   // On board Red LED (LED_BUILTIN)
#define LEDGRN     2   // Add on Green LED (GPIO


///////////////
// RFID task //
///////////////
// RFID task : UART Communication
#define IDSTRLEN       10  // 10 characters long (ID only)
#define IDCHKSTRLEN    12  // 12 characters long  (ID + checksum)

#define STARTCHAR 0x02
#define CRCHAR    0x0D
#define LFCHAR    0x0A
#define ENDCHAR   0x03
/*-------------------------------------------------------------------------*
 * Types:
 *-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*
 * Globals: (Timer related)
 *-------------------------------------------------------------------------*/

// Create the event group
EventGroupHandle_t eventgroupflags;

///////////////////
// M1 WiFi  task //
///////////////////
// ongoing timer counter for heartbeat
static int heartbeat_timer = 0;

// set heartbeat period in milliseconds
static int heartbeat_period = 60000;

// track time when last connection error occurs
long lastReconnectAttempt = 0;

///////////////
// LED task  //
///////////////
bool      g_LED_request = false; // LED request flag set by the WiFi callback

uint16_t  g_count = 0;       // counter variable
uint16_t  g_LED_select = 0;  // LED select 
uint16_t  g_LED_state = 0;   // LED state variable
bool      g_LED_alloff = false;  // turn off all led off first (for showing the card verify response)

///////////////
// RFID task //
///////////////
char rfid_str[IDSTRLEN+1];   // Store the the ID data + NULL char
char databyte;           // data byte from UART
//int index = 0;         // ESP32 does not accept this variable declaration
bool startflag = false;  // start flag
char checksum;
char hexvalue;

/*-------------------------------------------------------------------------*
 * Globals: (I/O pin related)
 *-------------------------------------------------------------------------*/
///////////////
// LED task  //
///////////////
/*  (LED's location)
  *   1. Adafruit Huzzah ESP32  (Red LED near the USB header)
  *   2. SparkFun ESP32 Thing (Blue LED at the middle of the board) 
  *   3. GPIO16 (the pin with the "16" marker)
  */
// set pin for LED
const int RED_LED_PIN = LED_BUILTIN; // GPIO 13 (Adafruit ESP32 Feather)
const int GRN_LED_PIN = 12;          // GPIO 12 (next pin)

///////////////
// RFID task //
///////////////
HardwareSerial RFIDSerial(2);  // UART2 GPIO16 (RX), GPIO17 (TX) Adafruit ESP32
int            RFIDbaud = 9600;   // board Rate


/*-------------------------------------------------------------------------*
 * Globals: (WiFi Secruity Credential related)
 *-------------------------------------------------------------------------*/
// wifi client with security
WiFiClientSecure wifiClient; 

// MQTT Connection info
//char server[] = "mqtt.mediumone.com";
//int port = 61620;
//char pub_topic[]="0/<Project MQTT ID>/<User MQTT>/esp32/";
//char sub_topic[]="1/<Project MQTT ID>/<User MQTT>/esp32/event";
//char mqtt_username[]="<Project MQTT ID>/<User MQTT>";
//char mqtt_password[]="<API Key>/<User Password>";

//char WIFI_SSID[] = "<WIFI SSID>";
//char WIFI_PASSWORD[] = "<WIFI_PASSWORD>";


/*-------------------------------------------------------------------------*
 * Prototypes:
 *-------------------------------------------------------------------------*/
// Subroutine Prototypes
// Function Prototypes

///////////////////
// M1 WiFi  task //
///////////////////
// Store a new recieved message from the MQTT broker
void    callback(char* topic, byte* payload, unsigned int length);

// Check if the MQTT connection is still good.
boolean connectMQTT();

// Send a message with the sense data to the MQTT broker
void    sensor_loop();

// Send a regular message to keep MQTT connection if no sense data is to be sent.
void    heartbeat_loop(); 

void process_request_loop();

// send a mqtt message 
void send_mqtt_msg(char * tag_name, char *data_str);

///////////////
// RFID task //
///////////////
char CONVERT_ASCII_HEX(char value);
/*-------------------------------------------------------------------------*
 * Class Varaible Globals: 
 *-------------------------------------------------------------------------*/

// Class Variable Declaratioin (Require callback and other variables to be already defined.)
PubSubClient client(server, port, callback, wifiClient);

/*-------------------------------------------------------------------------*
 * Main Program Start: (Setup)
 *-------------------------------------------------------------------------*/
void setup (){
  
  // init uart (for debug)
  Serial.begin(115200);
  while(!Serial){}   // Wait for connection.

  ///////////////
  // RFID task //
  ///////////////
  // initialize serial communication
  Serial.setTimeout(60000);  // 60 sec
  RFIDSerial.begin(RFIDbaud);   // Serial Soft UART to the fingerprint scanner UART port. 

  //////////////////
  // M1 WiFi task //
  //////////////////
  // wifi setup
  WiFi.mode(WIFI_STA);  // STA mode

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);   // new from the new release

  //not sure this is needed
  delay(5000);
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("Failed to connect, resetting"));
    //ESP.reset();  // not supported by esp32 WiFi library
  }

  // optinally 
  //while (WiFi.status() != WL_CONNECTED) {}
  
  // if you get here you have connected to the WiFi
  Serial.println(F("Connected to Wifi!"));

  Serial.println(F("Init hardware LED"));
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GRN_LED_PIN, OUTPUT);
  
  // connect to MQTT broker to send board reset msg
  connectMQTT();

  Serial.println("Complete Setup");

  /* Create new event group */
  eventgroupflags = xEventGroupCreate();

  /* Create new tasks here */
  xTaskCreate(
      WiFi_M1_Task,             /* Task function. */
      "WiFi M1 Task",           /* name of task. */
      10000,                    /* Stack size of task */
      NULL,                     /* parameter of the task */
      1,                        /* priority of the task */
      NULL);                    /* Task handle to keep track of created task */

  xTaskCreate(
      LED_Task,                 /* Task function. */
      "LED Task",               /* name of task. */
      1000,                     /* Stack size of task */
      NULL,                     /* parameter of the task */
      1,                        /* priority of the task */
      NULL);                    /* Task handle to keep track of created task */      

  xTaskCreate(
      RFID_Task,                /* Task function. */
      "RFID Task",              /* name of task. */
      1000,                     /* Stack size of task */
      NULL,                     /* parameter of the task */
      1,                        /* priority of the task */
      NULL);                    /* Task handle to keep track of created task */      

}

/*-------------------------------------------------------------------------*
 * Main Program Start: (loop [task])
 *-------------------------------------------------------------------------*/

void loop() {
  
  // Do nothing right now
  delay(200);  // To keep the watchdog timer from triggered.
               // Prevent the loop task from monopolizing the cpu0 time.
}

/*-------------------------------------------------------------------------*
 * Main Program Start: (RTOS Tasks)
 *-------------------------------------------------------------------------*/

/* WiFi M1 Task: Check for the MQTT connection. Send messages to the MQTT broker. */
void WiFi_M1_Task( void * parameter )
{
  /* loop forever */
  for(;;){
    if (!client.connected()) {
      long now = millis();
      if (now - lastReconnectAttempt > 1000) {
        lastReconnectAttempt = now;
        // Attempt to reconnect
        if (connectMQTT()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {
      // Client connected
      client.loop();
    }
    heartbeat_loop();
    process_request_loop();
  }
  /* delete a task when finish, 
  this will never happen because this is infinity loop */
  vTaskDelete( NULL );
}

void LED_Task( void * parameter ) {
  /* loop forever */
  for(;;){
    // Wait for LED request bit indefinitely.  
    EventBits_t xbit = xEventGroupWaitBits(eventgroupflags, LED_REUEST_EVENT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    // Process message to turn LED on and off
    Serial.println(F("This is LED Task."));
    if (g_LED_alloff) {
        Serial.println(F("First turn both Red and Green LEDs off if they are on."));
        digitalWrite(RED_LED_PIN, LOW);   // for Adafruit HUZZAH ESP32
        digitalWrite(GRN_LED_PIN, LOW);   // for Adafruit HUZZAH ESP32
        delay (500);   // Make one blink
    }    
    switch (g_LED_select){
         case LEDRED : 
          if (g_LED_state == 0) { 
            // Turn off LED
            digitalWrite(RED_LED_PIN, LOW);   // for Adafruit HUZZAH ESP32
            Serial.println(F("LED request: Turn Red LED off "));
          } else { 
            // Turn on LED
            digitalWrite(RED_LED_PIN, HIGH);  // for Adafruit HUZZAHESP32
            Serial.println(F("LED request: Turn Red LED on "));
          }      
          Serial.println("");
          break;
         case LEDGRN : 
          if (g_LED_state == 0) { 
            // Turn off LED
            digitalWrite(GRN_LED_PIN, LOW);   // for Adafruit HUZZAH ESP32
            Serial.println(F("LED request: Turn Green LED off "));
          } else { 
            // Turn on LED
            digitalWrite(GRN_LED_PIN, HIGH);  // for Adafruit HUZZAHESP32
            Serial.println(F("LED request: Turn Green LED on "));
          }      
          Serial.println("");
          break;
         Default :
          break;
    }
    if (g_LED_alloff) {
        delay (1000);   // Make one blink
        Serial.println(F("Turn both Red and Green LEDs off."));
        digitalWrite(RED_LED_PIN, LOW);   // for Adafruit HUZZAH ESP32
        digitalWrite(GRN_LED_PIN, LOW);   // for Adafruit HUZZAH ESP32
        g_LED_alloff = false;
    }  
    delay(100);
  }
  /* delete a task when finish, 
  this will never happen because this is infinity loop */
  vTaskDelete( NULL );  
}

void RFID_Task( void * parameter ) {
  int index;  
  
  /* loop forever */
  for(;;){
    if (RFIDSerial.available())  {
      databyte = RFIDSerial.read();
      if (databyte == STARTCHAR) {
          startflag = true;
          checksum = 0;
          index = 0;
          Serial.println("Card Key : ");
      } else if (databyte == ENDCHAR) {
          startflag = false;
          index = 0;
          Serial.print(" Done.");
          Serial.println(checksum,HEX);                    
          if (checksum == hexvalue) {
              Serial.println("Checksum check is good.");   
              rfid_str[IDSTRLEN] = '\0';
              xEventGroupSetBits(eventgroupflags,SEND_RFID_KEY_REQUEST_EVENT_BIT);   
          }else{ 
              Serial.println("Checksum check is bad.");      
          }
      } else if (index < IDCHKSTRLEN) {
          if (index < IDSTRLEN)  // Only store the first 10 ascii data.
            rfid_str[index] = databyte;
          Serial.print(index);
          Serial.print(" => (ascii) ");
          Serial.print(databyte);
          databyte = CONVERT_ASCII_HEX(databyte);
          Serial.print(" => (hex) ");                
          Serial.print(databyte,HEX);
          if (!(index%2)){
              hexvalue = databyte << 4;
          } else {
              hexvalue = hexvalue + databyte;
              if (index < 10)  // checksum only for the first 10 ascii data.
                checksum = checksum ^ hexvalue;
          }
          Serial.print(" => (hex) ");                
          Serial.println(hexvalue,HEX);                                        
          index++;
      } // ignore LF and CR  
    }   
    delay(1);  // add delay for the background processor 
  }
  /* delete a task when finish, 
  this will never happen because this is infinity loop */
  vTaskDelete( NULL );  
}

/*-------------------------------------------------------------------------*
 *  Subroutines' Body: 
 *-------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------*
 *  A callback function used by Wifi Client to pass the recived message from
 *  the MQTT broker to the MQTT client.
 *
 *  For this example, only 1 value (0 or 1) is expected and is used to turn
 *  on or off the LED light.
 *
 *  input:  topic (MQTT topic string)
 *          payload (MQTT message payload string)
 *          length  (MQTT message payload string's length)
 *  Return : None
 *  
 *-------------------------------------------------------------------------*/ 
void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  int i = 0;
  char message_buff[length + 1];
  int led;     // which led to select (1 = red, 2 = green)
  int level;   // led level (1 = on, 0 = off)

  for(i=0; i < length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';

  Serial.println(F("This is WiFi Task."));
  Serial.print(F("Received some data from the MQTT broker: "));
  Serial.println(String(message_buff));

  switch (message_buff[0]) {
     case 'G':   //G<led>:<level>
        if (sscanf(&message_buff[1],"%d:%d",&led,&level) == 2) {
          if (led > 0 && led <= LEDTOTAL) {
            g_LED_alloff = false;
            g_LED_request = true;
            g_LED_select  = led;  
            g_LED_state = level;            
          }
        }
        break;
      case 'V':   //V<pass/fail>  // 0=fail 1=pass
        if (sscanf(&message_buff[1],"%d",&level) == 1) {
          Serial.println(F("Verify case"));
          if (level < 2) {
            g_LED_alloff = true;
            g_LED_request = true;
            if (0 == level)
              g_LED_select  = LEDRED;  
            else if (1 == level)  
              g_LED_select  = LEDGRN;  
            g_LED_state = 1;
          }
        }
        break;      
     default:
        break;
  }
}
/*-------------------------------------------------------------------------*
 *  Connect to the MQTT broker.   
 *
 *  input:  mqtt_user (a unique id name is required) -- global variable access
 *          mqtt_password (the username's password)-- global variable access
 *  Return : Connection status
 *  
 *-------------------------------------------------------------------------*/ 
boolean connectMQTT()
{    
  // Important Note: MQTT requires a unique id (UUID), we are using the mqtt_username as the unique ID
  // Besure to create a new device ID if you are deploying multiple devices.
  // Learn more about Medium One's self regisration option on docs.mediumone.com
  if (client.connect((char*) mqtt_username,(char*) mqtt_username, (char*) mqtt_password)) {
    Serial.println(F("Connected to MQTT broker"));

    // send a connect message
    //if (client.publish((char*) pub_topic, "{\"event_data\":{\"mqtt_connected\":true}}")) {
    // send a connect message with add_client_ip enable.  So, M1 cloud will pull the board IP address to find
    // the board's GPS location.
    //if (client.publish((char*) pub_topic, "{\"event_data\":{\"mqtt_connected\":true}, \"add_client_ip\":true}")) {
    if (client.publish((char*) pub_topic, "{\"event_data\":{\"mqtt_connected\":true}}")) {
      Serial.println("Publish connected message ok");
    } else {
      Serial.print(F("Publish connected message failed: "));
      Serial.println(String(client.state()));
    }

    // subscrive to MQTT topic
    if (client.subscribe((char *)sub_topic,1)){
      Serial.println(F("Successfully subscribed"));
    } else {
      Serial.print(F("Subscribed failed: "));
      Serial.println(String(client.state()));
    }
  } else {
    Serial.println(F("MQTT connect failed"));
    Serial.println(F("Will reset and try again..."));
    abort();
  }
  return client.connected();
}


/*-------------------------------------------------------------------------*
 *  Send a heart beat  message to the MQTT broker to let the user
 *  know that the device is still running.
 *
 *  input:  send a message at a rate determined by heartbeat_period.
 *  Return : None
 *  
 *-------------------------------------------------------------------------*/ 
void heartbeat_loop() {
  if ((millis()- heartbeat_timer) > heartbeat_period) {
    heartbeat_timer = millis();
    String payload = "{\"event_data\":{\"millis\":";
    payload += millis();
    payload += ",\"heartbeat\":true}}";
    
    if (client.loop()){
      Serial.println(F("This is WiFi Task."));
      Serial.print(F("Sending heartbeat: "));
      Serial.println(payload);
  
      if (client.publish((char *) pub_topic, (char*) payload.c_str()) ) {
        Serial.println(F("Publish ok"));
      } else {
        Serial.print(F("Failed to publish heartbeat: "));
        Serial.println(String(client.state()));
      }
    }
    delay(1000);  // To keep the watchdog timer from triggered.
                 // Prevent the loop task from monopolizing the cpu1 time.
  }
}

/*-------------------------------------------------------------------------*
 *  Set an event request bit for the task that has been waiting for it.   
 *
 *  input:  None
 *  Return : None
 *  
 *-------------------------------------------------------------------------*/ 
void process_request_loop() {
  EventBits_t xbits;
  const TickType_t xTickstoWait = 10 / portTICK_PERIOD_MS;  // 10 ms
  const char tag_name[] = "card_key";
  
  xbits = xEventGroupWaitBits(eventgroupflags, SEND_RFID_KEY_REQUEST_EVENT_BIT, pdTRUE, pdTRUE,xTickstoWait);
  if (xbits & SEND_RFID_KEY_REQUEST_EVENT_BIT) {
     send_mqtt_msg(tag_name,rfid_str);
  }
  if (g_LED_request) {
    g_LED_request = false;
    xEventGroupSetBits(eventgroupflags,LED_REUEST_EVENT_BIT);
  }
}

/*-------------------------------------------------------------------------*
 *  Send a message to the MQTT broker.
 *
 *  input:  tag_name and its data string
 *  Return : None
 *  
 *-------------------------------------------------------------------------*/ 
void send_mqtt_msg(const char * tag_name, const char *data_str) {
    String payload = "{\"event_data\":{\"";    
    payload += String(tag_name);
    payload += "\":\"";
    payload += String(data_str);
    payload += "\"}}";
    
    if (client.loop()){
      Serial.println(F("This is WiFi Task."));
      Serial.print(F("Sending mqtt message: "));
      Serial.println(payload);
  
      if (client.publish((char *) pub_topic, (char*) payload.c_str()) ) {
        Serial.println(F("Publish ok"));
      } else {
        Serial.print(F("Failed to publish heartbeat: "));
        Serial.println(String(client.state()));
      }
    } 
}
/*-------------------------------------------------------------------------*
 *  Convert an ASCII character to its equivalent hex value.  
 *
 *  input:  An ASCII characters (0-9,A-F only)
 *  Return : Hex value
 *  
 *-------------------------------------------------------------------------*/ 
char CONVERT_ASCII_HEX(char value) {
  if (value >= '0' && value <='9') 
     return(value - '0');
  else 
     return(value - 'A' + 0x0a);
}

