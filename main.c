#include <esp32-hal-ledc.h>
#include <Adafruit_ILI9341.h>
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "SD.h"
#include "SPI.h"
#include "esp32-hal-periman.h"


//static const uint8_t MOSI  = 23;
//static const uint8_t MISO  = 19;
//static const uint8_t SCK   = 18;

#define TFT_DC  2
#define TFT_RESET 4
#define TFT_CS 5


const int LED_OUTPUT_PIN = 26;
const int PB = 33; // Bi-Directional
const int PA = 32; // Single Ended Pressure Sensor
const int SPD = 35;
const int nFault = 34;
const int chipSelect = 22;

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS,TFT_DC,MOSI,SCK,TFT_RESET,MISO);

const int BLDC_PWM = 0;    // ESP32 has 16 channels which can generate 16 independent waveforms
const int PWM_FREQ = 480;     // Recall that Arduino Uno is ~490 Hz. Official ESP32 example uses 5,000Hz
const int PWM_RESOLUTION = 12; // We'll use same resolution as Uno (8 bits, 0-255) but ESP32 can go up to 16 bits 
const int MAX_DUTY_CYCLE = (int)(pow(2, PWM_RESOLUTION) - 1); 

// The max duty cycle value based on PWM resolution (will be 255 if resolution is 8 bits)

const int DELAY_MS = 100;  // delay between fade increments
const float R1_B = 18000.0;
const float R2_B = 27000.0;
const float R_B = (R1_B+R2_B)/R2_B;


const float R1_A = 18000.0;
const float R2_A = 27000.0;
const float R_A = (R1_A+R2_A)/R2_A;

const float ADC_Conv = 3.3/4096.0;


//P_B Constnats : Output Range Min 10%Vdd(-125Pa) - 90%Vdd(125Pa)
float PB_Vdd = 5.0;
float Pa_to_cmH20 = 1.0/98.0665;
float Kpa_to_cmH20 = 10.197162129779;


// PA Constants and Pressure Conversion (Single Port)
float Vs_High = 5.25;
float Vs_Low = 4.75;
float VFSS_High = 4.6 + 0.23;
float VFSS_Low = 4.6 - 0.23;
float T_const = 0.045;
float OffSet;
int speed = 0;
int confirm = 0;

//////////

File myFile;




float Pressure_Sensor_A (){

  float PA_Voltage = (analogRead(PA) * ADC_Conv)*R_A;
  float Offset_High = 0.430;
  float Offset_Low = 0.1;
  float Sensitivity = 1.0/0.766;

  
  float Num_High = (PA_Voltage - Offset_High) + VFSS_High - (T_const*Vs_High);
  float Denom_High = Vs_High*0.1533;
  float PressureA_High = Num_High/Denom_High;

  float Num_Low = (PA_Voltage - Offset_Low) + VFSS_Low - (T_const*Vs_Low);
  float Denom_Low = Vs_Low*0.1533;
  float PressureA_Low = Num_Low/Denom_Low;

  float PressureA_Avg = (PressureA_High + PressureA_Low) / 2.0;
  float P_cmH20 = PressureA_Avg*10.197;

  float testing = PA_Voltage*Sensitivity; //KPA

  return testing * Kpa_to_cmH20;

}; 

// 𝐷𝑃 = 𝑠𝑖𝑔𝑛 (𝐴𝑂𝑢𝑡/𝑉𝐷𝐷 − 0.5) ∙ ( 𝐴𝑂𝑢𝑡/(𝑉𝐷𝐷 ∙ 0.4) − 1.25)^2 * 133

float Pressure_Sensor_B (){
  float PB_Voltage = (analogRead(PB)* ADC_Conv)*R_B;
  float DP =((PB_Voltage/PB_Vdd)-0.5)*sq(((PB_Voltage/(PB_Vdd*0.4))-1.25))*133;
  float PA_cmH20 = DP*Pa_to_cmH20;
  return DP;
};


unsigned long testText() {
  tft.fillScreen(ILI9341_BLACK);
  unsigned long start = micros();
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_WHITE);  tft.setTextSize(1);
  tft.println("Hello World!");
  tft.setTextColor(ILI9341_YELLOW); tft.setTextSize(2);
  tft.println(1234.56);
  tft.setTextColor(ILI9341_RED);    tft.setTextSize(3);
  tft.println(0xDEADBEEF, HEX);
  tft.println();
  tft.setTextColor(ILI9341_GREEN);
  tft.setTextSize(5);
  tft.println("Groop");
  tft.setTextSize(2);
  tft.println("I implore thee,");
  tft.setTextSize(1);
  tft.println("my foonting turlingdromes.");
  tft.println("And hooptiously drangle me");
  tft.println("with crinkly bindlewurdles,");
  tft.println("Or I will rend thee");
  tft.println("in the gobberwarts");
  tft.println("with my blurglecruncheon,");
  tft.println("see if I don't!");
  return micros() - start;
}


int motor_speed(){
    int btnState = 0;
    int prevBtnState =0;
    int counter = 0;
    int Times[2];

    while(counter < 2){

    btnState = digitalRead(SPD);
    if(btnState != prevBtnState){

    if(btnState == HIGH ){
      int counter_rise = micros();
      Times[counter] = counter_rise;
      counter++;
    }

  }

  // save the previous button state for the next loop
  prevBtnState = btnState;
  //delay(1000);

  }

  return (Times[1]-Times[0]);

}




float Avg_motor_speed(){
  float Speed_Sum = 0;
  float Avg_speed;
  int Avg_samples = 10;
  for(int x=0;x<=Avg_samples;x++){
    Speed_Sum += motor_speed();

  }

  Avg_speed = Speed_Sum/Avg_samples;
  return Avg_speed;

}


void setup() {

  // Sets up a channel (0-15), a PWM duty cycle frequency, and a PWM resolution (1 - 16 bits) 
  // ledcSetup(uint8_t channel, double freq, uint8_t resolution_bits);
  ledcSetup(BLDC_PWM, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(LED_OUTPUT_PIN, BLDC_PWM);
  pinMode(SPD,INPUT);
  pinMode(TFT_CS,OUTPUT);
  pinMode(TFT_DC,OUTPUT);
  ledcWrite(BLDC_PWM, 1000);
	tft.begin();
  tft.startWrite();
  tft.fillScreen(ILI9341_BLUE);
  testText();
  // wait for Serial Monitor to connect. Needed for native USB port boards only:
  delay(500);
  Serial.begin(115200);
  Serial.print("Begin!");

  while (!Serial) 
  {
    // Wait for serial port to connect. Needed for native USB port only
    ; 
  }
  Serial.println("Setup start");
  SPI.begin(SCK, MISO, MOSI, chipSelect);
  if (!SD.begin(chipSelect)) 
  {
    Serial.println("SD Card MOUNT FAIL");
  } 
  else 
  {
    Serial.println("SD Card MOUNT SUCCESS");
    Serial.println("");
    uint32_t cardSize = SD.cardSize() / (1024 * 1024);
    String str = "SDCard Size: " + String(cardSize) + "MB";
    Serial.println(str);
    uint8_t cardType = SD.cardType();
    if(cardType == CARD_NONE)
    {
      Serial.println("No SD card attached");
    }
    Serial.print("SD Card Type: ");
    if(cardType == CARD_MMC)
    {
        Serial.println("MMC");
    } 
    else if(cardType == CARD_SD)
    {
        Serial.println("SDSC");
    } 
    else if(cardType == CARD_SDHC)
    {
        Serial.println("SDHC");
    } 
    else 
    {
        Serial.println("UNKNOWN");
    }
    myFile = SD.open("/");
    //printDirectory(myFile, 0);
    myFile.close();
    Serial.println("");
    // open a new file and immediately close it:
    Serial.println("Creating helloworld.txt...");
    myFile = SD.open("/helloworld.txt", FILE_WRITE);
    // Check to see if the file exists:
    if (SD.exists("/helloworld.txt")) 
    {
      Serial.println("helloworld.txt exists.");
    } 
    else 
    {
      Serial.println("helloworld.txt doesn't exist.");
    }
    // delete the file:
    Serial.println("Removing helloworld.txt...");
    SD.remove("/helloworld.txt");
    if (SD.exists("/helloworld.txt")) 
    {
      Serial.println("helloworld.txt exists.");
    } 
    else 
    {
      Serial.println("helloworld.txt doesn't exist.");
    }
    myFile.close();
    Serial.println("");
    // Open a file. Note that only one file can be open at a time,
    // so you have to close this one before opening another.
    myFile = SD.open("/test.txt", FILE_WRITE);
    // if the file opened okay, write to it.
    if (myFile) 
    {
      Serial.print("Writing to test.txt...");
      myFile.println("testing 1, 2, 3.");
      
    // close the file:
      myFile.close();
      Serial.println("done.");
    } 
    else 
    {
      // if the file didn't open, print an error.
      Serial.println("error opening test.txt");
    }
    // Re-open the file for reading.
    myFile = SD.open("/test.txt");
    if (myFile) 
    {
      Serial.println("test.txt:");
      // Read from the file until there's nothing else in it.
      while (myFile.available()) 
      {
        Serial.write(myFile.read());
      }
      
     // Close the file.
      myFile.close();
    } 
    else 
    {
    
     // If the file didn't open, print an error.
      Serial.println("error opening test.txt");
    }
    myFile.close();
  }
  SD.end();
  SPI.end();
  Serial.println("INFO: Setup complete");

}

void loop() {

  
  for(int x = 200; x<2500 ; x++){
    ledcWrite(BLDC_PWM, x);
    Serial.print(motor_speed());
    Serial.print(",");
    Serial.println(Pressure_Sensor_A());
    delay(500);
    x+=10;
  }

  for(int x = 2500; x>200 ; x--){
    ledcWrite(BLDC_PWM, x);
    Serial.print(motor_speed());
    Serial.print(",");
    Serial.println(Pressure_Sensor_A());
    delay(500);
    x-=10;
  }
}
