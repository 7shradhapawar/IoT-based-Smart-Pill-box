
#include<SoftwareSerial.h>
#include "DHT.h"
#include "mpr121.h"
#include <Wire.h>

#include<LiquidCrystal.h>   // This library allows for use of LCD.   

String transmit(String esp_command, const int timeout, boolean debug);
String getStr;
float humidity;
float tempCelcius;
float tempFahrenheit;
float heatIndexF;
long writingTimer = 60;
//long writingTimer = 600; 
long startTime = 0;
long waitTime = 0;
// Thingspeak  
String myAPIkey = "8RYT1OLOFD67SIK4";  
unsigned char check_connection=0;
unsigned char times_check=0;
boolean error;
unsigned int touchDetect=0;
unsigned int pinNumber=0;

#define DEBUG true
#define DHTTYPE DHT11
#define DHTPIN 6

SoftwareSerial esp(4,5);    // Arduino Rx is now pin 8, Tx, is now pin 9
DHT dht(DHTPIN, DHTTYPE);

int irqpin = 2;  // Digital 2
boolean touchStates[12]; //to keep track of the previous touch states


// Initialize pins of LCD and create instance for module
const int reset = 13, enable = 12, lcdD4 = 11, lcdD5 = 10, lcdD6 = 9, lcdD7 = 8;

LiquidCrystal LCDboard(reset,enable,lcdD4,lcdD5,lcdD6,lcdD7); // Initialize LCD

void setup()
{
 
  Serial.begin(115200);      // Set baud rate of serial data transmission 
  esp.begin(115200);         // This baud rate should match the ESP8266 module (may differ)
  Wire.begin();              // i2c Communication
  dht.begin();               //for dht11 temp sensor
   LCDboard.begin(16,2);     // (Number of columns,rows of LCD module)


  //Wifi Module Initialization
  
  startTime = millis(); 
  esp.println("AT+RST");
  delay(2000);
  Serial.println("Connecting to Wifi");
   while(check_connection==0)
  {
    Serial.print(".");
    esp.print("AT+CWJAP=\"Pixel_5161\",\"12345678\"\r\n");
    //esp.print("AT+CWJAP=\"Titan Smartponics\",\"12345678\"\r\n");
    //esp.print("AT+CWJAP=\"JollyMenBearingGifts\",\"A24@1234509876\"\r\n");
    esp.setTimeout(5000);
    if(esp.find("WIFI CONNECTED\r\n")==1)
     {
     Serial.println("WIFI CONNECTED");
     break;
     }
     times_check++;
     if(times_check>3) 
     {
      times_check=0;
      Serial.println("Trying to Reconnect..");
     }
  }

  //for mpr121 touch sensor   
  pinMode(irqpin, INPUT);
  digitalWrite(irqpin, HIGH); //enable pullup resistor
  mpr121_setup();             //Configuration of MPR 121 registers
  
}

void loop() 
{
 
  //  delay(2000);
        
     //for mpr121 sensor
    readTouchInputs();  
   
    waitTime = millis()-startTime;   
  if (waitTime > (writingTimer*1000)) 
  {
    readTempHumidity();
    //writeThingSpeak();
    startTime = millis();   
    
  }
  else if(touchDetect == 1)// Touch detected
  {
    writeTouchDataThingSpeak();
    touchDetect = 0;
    }
    
}

void readTempHumidity()
{
    humidity = dht.readHumidity();
    tempCelcius = dht.readTemperature();
    
if(tempCelcius >= 20)
{
    LCDboard.clear(); // clear the lcd screen
    delay(500);
    LCDboard.print("Temp above"); // display "Temperature: " onto lcd screen
    LCDboard.setCursor(0,1);
    LCDboard.print("threshold");
    delay(500);
}
else if (humidity > 60)
{
    LCDboard.clear(); // clear the lcd screen
    delay(500);
    LCDboard.print("Humidity above"); // display "Temperature: " onto lcd screen
    LCDboard.setCursor(0,1);
    LCDboard.print("threshold");
    delay(500);
}
else
{
       LCDboard.clear(); // clear the lcd screen
   delay(500);
  LCDboard.print("Temp: "); // display "Temperature: " onto lcd screen
  delay(500);
  // // move cursor to the next line 
  LCDboard.print(tempCelcius); // display temperature in Celcius (ex: "= 17 Â°C") 
  LCDboard.print((char)223); // ASCII value for degree symbol
  LCDboard.print("C");
  LCDboard.setCursor(0,1);
   LCDboard.print("Humidity: "); // display "Temperature: " onto lcd screen
  delay(500);  
  LCDboard.print(humidity); // display temperature in Celcius (ex: "= 17 Â°C") 
   
    Serial.print("\n humidity: ");
    Serial.print(humidity);
    Serial.print("\n Temperature: ");
    Serial.print(tempCelcius);  
      }
    
}

String transmit(String esp_command, const int timeout, boolean debug)
{
    String response = " ";    
    esp.print(esp_command); // send the read character to the esp
    
    long int time = millis();
    
    while( (time+timeout) > millis())
    {
      while(esp.available())
      {        
        // The esp has data so display its output to the serial window 
        char c = esp.read(); // read the next character.
        response+=c;
      }  
    }
    
    if(debug)
    {
      Serial.print(response);
    }   
    return response;
}

//for mpr121 sensor

void readTouchInputs()
{
  if(!checkInterrupt())
  {
    
    //read the touch state from the MPR121
    Wire.requestFrom(0x5A,2); 
    
    byte LSB = Wire.read();
    byte MSB = Wire.read();
    
    uint16_t touched = ((MSB << 8) | LSB); //16bits that make up the touch states

    for (int i=0; i < 12; i++)
    {  // Check what electrodes were pressed
      if(touched & (1<<i))
      {
      
        if(touchStates[i] == 0)
        {
          //pin i was just touched
          Serial.print("pin ");
          Serial.print(i);
          Serial.println(" was just touched");
          touchDetect = 1;
          pinNumber = i;
        }
        else if(touchStates[i] == 1)
        {
          //pin i is still being touched
        }        
        touchStates[i] = 1;      
      }
      else
      {
        if(touchStates[i] == 1)
        {
          Serial.print("pin ");
          Serial.print(i);
          Serial.println(" is no longer being touched");
          
          //pin i is no longer being touched
        }
        
        touchStates[i] = 0;
      }
    
    }
    
  }
}


void mpr121_setup(void)
{

  set_register(0x5A, ELE_CFG, 0x00); 
  
  // Section A - Controls filtering when data is > baseline.
  set_register(0x5A, MHD_R, 0x01);
  set_register(0x5A, NHD_R, 0x01);
  set_register(0x5A, NCL_R, 0x00);
  set_register(0x5A, FDL_R, 0x00);

  // Section B - Controls filtering when data is < baseline.
  set_register(0x5A, MHD_F, 0x01);
  set_register(0x5A, NHD_F, 0x01);
  set_register(0x5A, NCL_F, 0xFF);
  set_register(0x5A, FDL_F, 0x02);
  
  // Section C - Sets touch and release thresholds for each electrode
  set_register(0x5A, ELE0_T, TOU_THRESH);
  set_register(0x5A, ELE0_R, REL_THRESH);
 
  set_register(0x5A, ELE1_T, TOU_THRESH);
  set_register(0x5A, ELE1_R, REL_THRESH);
  
  set_register(0x5A, ELE2_T, TOU_THRESH);
  set_register(0x5A, ELE2_R, REL_THRESH);
  
  set_register(0x5A, ELE3_T, TOU_THRESH);
  set_register(0x5A, ELE3_R, REL_THRESH);
  
  set_register(0x5A, ELE4_T, TOU_THRESH);
  set_register(0x5A, ELE4_R, REL_THRESH);
  
  set_register(0x5A, ELE5_T, TOU_THRESH);
  set_register(0x5A, ELE5_R, REL_THRESH);
  
  set_register(0x5A, ELE6_T, TOU_THRESH);
  set_register(0x5A, ELE6_R, REL_THRESH);
  
  set_register(0x5A, ELE7_T, TOU_THRESH);
  set_register(0x5A, ELE7_R, REL_THRESH);
  
  set_register(0x5A, ELE8_T, TOU_THRESH);
  set_register(0x5A, ELE8_R, REL_THRESH);
  
  set_register(0x5A, ELE9_T, TOU_THRESH);
  set_register(0x5A, ELE9_R, REL_THRESH);
  
  set_register(0x5A, ELE10_T, TOU_THRESH);
  set_register(0x5A, ELE10_R, REL_THRESH);
  
  set_register(0x5A, ELE11_T, TOU_THRESH);
  set_register(0x5A, ELE11_R, REL_THRESH);
  
  // Section D
  // Set the Filter Configuration
  // Set ESI2
  set_register(0x5A, FIL_CFG, 0x04);
  
  // Section E
  // Electrode Configuration
  // Set ELE_CFG to 0x00 to return to standby mode
  set_register(0x5A, ELE_CFG, 0x0C);  // Enables all 12 Electrodes
  
  
  // Section F
  // Enable Auto Config and auto Reconfig
  /*set_register(0x5A, ATO_CFG0, 0x0B);
  set_register(0x5A, ATO_CFGU, 0xC9);  // USL = (Vdd-0.7)/vdd*256 = 0xC9 @3.3V   set_register(0x5A, ATO_CFGL, 0x82);  // LSL = 0.65*USL = 0x82 @3.3V
  set_register(0x5A, ATO_CFGT, 0xB5);*/  // Target = 0.9*USL = 0xB5 @3.3V
 
  set_register(0x5A, ELE_CFG, 0x0C);
  
}


boolean checkInterrupt(void)
{
  return digitalRead(irqpin);
}


void set_register(int address, unsigned char r, unsigned char v)
{
    Wire.beginTransmission(address);
    Wire.write(r);
    Wire.write(v);
    Wire.endTransmission();
}

void writeThingSpeak(void)
{
  startThingSpeakCmd();
  // preparacao da string GET
  getStr = "GET /update?api_key=";
  getStr += myAPIkey;
  getStr +="&field1=";
  getStr += String(tempCelcius);
  getStr +="&field2=";
  getStr += String(humidity);
  //getStr +=" HTTP/1.0";
  getStr += "\r\n\r\n";
  Serial.println(getStr);
  GetThingspeakcmd(getStr); 
}

void writeTouchDataThingSpeak(void)
{
  startThingSpeakCmd();
  // preparacao da string GET
  String getStr = "GET /update?api_key=";
  getStr += myAPIkey;
  getStr +="&field1=";
  getStr += String(1);
  getStr +="&field2=";
  getStr += String(pinNumber);
  getStr += "\r\n\r\n";
  Serial.println(getStr);
  GetThingspeakcmd(getStr); 
}


void startThingSpeakCmd(void)
{
  esp.flush();
  String cmd = "AT+CIPSTART=\"TCP\",\"";
  //cmd += "192.168.4.1";
  cmd += "184.106.153.149"; // api.thingspeak.com IP address
  cmd += "\",80";
  esp.println(cmd);
  Serial.print("Start Commands: ");
  Serial.println(cmd);

  if(esp.find("Error"))
  {
    Serial.println("AT+CIPSTART error");
    return;
  }
}

String GetThingspeakcmd(String getStr)
{
  String cmd = "AT+CIPSEND=";
  cmd += String(getStr.length());
  esp.println(cmd);
  Serial.print("Next Commands: ");
  Serial.println(cmd);

 // if(esp.find(">"))
  {
    esp.print(getStr);
    //Serial.println(getStr);
    delay(500);
    String messageBody = "";
    while (esp.available()) 
    {
      String line = esp.readStringUntil('\n');
      if (line.length() == 1) 
      { 
        messageBody = esp.readStringUntil('\n');
      }
    }
    Serial.print("MessageBody received: ");
    Serial.println(messageBody);
    return messageBody;
  }
  //else
  {
    esp.println("AT+CIPCLOSE");     
    Serial.println("AT+CIPCLOSE"); 
  } 
}
