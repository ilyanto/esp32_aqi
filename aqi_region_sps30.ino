#include <esp_wifi.h>
#include <esp_bt.h>
#include "driver/gpio.h"

//tack switch: GPIO27
//esp32 rx: GPIO 35
//esp32 tx: GPIO 22

#define BUTTON_GPIO GPIO_NUM_27
bool PRESSED = false;

#include <HardwareSerial.h>
#include <SensirionUartSps30.h>
HardwareSerial sps30Serial(2);
SensirionUartSps30 sps30;

#include <SPI.h>
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

#include <LittleFS.h>
#include "Free_Fonts.h"
#define FONT_LARGE "RobotoBold120"
int ORIENTATION=3;
//1 US, 2 CN
int REGION=1;

const int RX_PIN = 35;
const int TX_PIN = 22;

struct SPS30Data {
  float pm1;
  float pm25;
  float pm4;  
  float pm10;
  float nc05;
  float nc1;
  float nc25;
  float nc4;
  float nc10;
  float typicalParticleSize;
};

SPS30Data sps30Data;

float pm25 = 0;
int aqi = 0;

bool readSPS30(SPS30Data &data) {
  int16_t error = sps30.readMeasurementValuesFloat(
      data.pm1, data.pm25, data.pm4, data.pm10,
      data.nc05, data.nc1, data.nc25, data.nc4, data.nc10,
      data.typicalParticleSize);
  if (error != 0) {
    static unsigned long lastErrorLog = 0;
    if (millis() - lastErrorLog >= 2000) {
      Serial.printf("[SPS30:ERROR] readMeasurementValuesFloat error=%d\n", error);
      lastErrorLog = millis();
    }
    return false;
  }
  return true;
}

float ncBin(float cumulativeUpper, float cumulativeLower = 0.0f) {
  float count = cumulativeUpper - cumulativeLower;
  return count > 0.0f ? count : 0.0f;
}

// AQI calculation for PM2.5 using EPA breakpoints (float)
int aqi_from_pm25(float c) {
  // c: concentration in ug/m3 (ambient)
  struct BP { float Clow, Chigh; int Ilow, Ihigh; };
  const BP USaqi[] = {
    {    0,    12,   0,  50},
    { 12.1,  35.4,  51, 100},
    { 35.5,  55.4, 101, 150},
    { 55.5, 150.4, 151, 200},
    {150.5, 250.4, 201, 300},
    {250.5, 350.4, 301, 400},
    {350.5, 500.4, 401, 500}
  };
  const BP CNaqi[] = {
    {    0,    35,   0,  50},
    { 35.1,    75,  51, 100},
    { 75.1,   115, 101, 150},
    {115.1,   150, 151, 200},
    {150.1,   250, 201, 300},
    {250.1, 350.4, 301, 400},
    {350.5, 500.4, 401, 500}
  };

  const BP* aqi_table;
  int n;

  if (REGION == 1) {
    aqi_table = USaqi;
    n = sizeof(USaqi) / sizeof(USaqi[0]);
  }
  if (REGION == 2) {
    aqi_table = CNaqi;
    n = sizeof(CNaqi) / sizeof(CNaqi[0]);
  }

  for (int i=0; i<n; i++){
    if (c >= aqi_table[i].Clow && c <= aqi_table[i].Chigh) {
      float Ilow  = aqi_table[i].Ilow;
      float Ihigh = aqi_table[i].Ihigh;
      float Clow  = aqi_table[i].Clow;
      float Chigh = aqi_table[i].Chigh;
      float aqi = ((Ihigh - Ilow) / (Chigh - Clow)) * (c - Clow) + Ilow;
      return int(round(aqi));
    }
  }

  return 999; // out of range (>500)
}

void toSerial(){
  Serial.printf("[SENSOR:INFO] %i PM1:%.1f PM2.5:%.1f PM4:%.1f PM10:%.1f ug/m3, NC0.3-0.5:%.1f NC0.5-1:%.1f NC1-2.5:%.1f NC2.5-4:%.1f NC4-10:%.1f #/cm3, typical:%.3f um, AQI:%i\n", REGION, sps30Data.pm1, sps30Data.pm25, sps30Data.pm4, sps30Data.pm10, ncBin(sps30Data.nc05), ncBin(sps30Data.nc1, sps30Data.nc05), ncBin(sps30Data.nc25, sps30Data.nc1), ncBin(sps30Data.nc4, sps30Data.nc25), ncBin(sps30Data.nc10, sps30Data.nc4), sps30Data.typicalParticleSize, aqi);
}

void toLCD() {
  tft.fillScreen(TFT_BLACK);
  
  //aqi category info
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(FF1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);    tft.drawString("0-50 :",    205, 0, 2);    tft.drawString("  Good",           205, 15, 2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);   tft.drawString("51-100 :",  205, 30, 2);   tft.drawString("  Moderate",       205, 45, 2);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);   tft.drawString("101-150 :", 205, 60, 2);   tft.drawString("  Unhealthy for",  205, 75, 2);  tft.drawString("  Sensitive Groups", 205, 90, 2);
  tft.setTextColor(TFT_RED, TFT_BLACK);      tft.drawString("151-200 :", 205, 105, 2);  tft.drawString("  Unhealthy",      205, 120, 2);
  tft.setTextColor(TFT_PURPLE, TFT_BLACK);   tft.drawString("201-300 :", 205, 135, 2);  tft.drawString("  Very Unhealthy", 205, 150, 2);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK); tft.drawString("301+ :",    205, 165, 2);  tft.drawString("  Hazardous",      205, 180, 2);

  //aqi value
  String cat = "";
  if (aqi >= 0 && aqi <= 50){
    spr.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    cat = "Good";
  } else {
    if (aqi > 50 && aqi <= 100){
      spr.setTextColor(TFT_YELLOW, TFT_BLACK); 
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      cat = "Moderate";
    } else {
      if (aqi > 100 && aqi <= 150){
        spr.setTextColor(TFT_ORANGE, TFT_BLACK); 
        tft.setTextColor(TFT_ORANGE, TFT_BLACK); 
        cat = "Unhealthy for Sensitive Groups";
      } else {
        if (aqi > 150 && aqi <= 200){
          spr.setTextColor(TFT_RED, TFT_BLACK); 
          tft.setTextColor(TFT_RED, TFT_BLACK);
          cat = "Unhealthy";
        } else {
          if (aqi > 200 && aqi <= 300){
            spr.setTextColor(TFT_PURPLE, TFT_BLACK); 
            tft.setTextColor(TFT_PURPLE, TFT_BLACK);
            cat = "Very Unhealthy";
          } else {
            if (aqi > 300) {
              spr.setTextColor(TFT_DARKGREY, TFT_BLACK); 
              tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
              cat = "Hazardous";
            }
          }
        }
      }
    }
  }

  spr.loadFont(FONT_LARGE, LittleFS);
  spr.createSprite(200,200);
  spr.setTextDatum(TC_DATUM);
  spr.setCursor(5,0);
  spr.print(String(aqi));
  spr.pushSprite(0,40);
  spr.unloadFont();
  spr.deleteSprite();

  //aqi category
  tft.setTextDatum(TC_DATUM);
  
  tft.setFreeFont(FSS9);
  tft.drawString(cat, tft.width()/2, 198);
  
  //pm detail
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setFreeFont(FF0);

  String sRegion = "";
  if (REGION == 1) sRegion = "US";
  if (REGION == 2) sRegion = "CN";

  String pm = sRegion + " PM1:" + sps30Data.pm1 + " PM2.5:" + sps30Data.pm25 + " PM4:" + sps30Data.pm4 + " PM10:" + sps30Data.pm10 + " ug/m3";
  tft.drawString(pm, tft.width()/2, 221, 1);

  pm = String(".3:") + String(ncBin(sps30Data.nc05), 1) + " .5:" + String(ncBin(sps30Data.nc1, sps30Data.nc05), 1) + " 1:" + String(ncBin(sps30Data.nc25, sps30Data.nc1), 1) + " 2.5:" + String(ncBin(sps30Data.nc4, sps30Data.nc25), 1) + " 4:" + String(ncBin(sps30Data.nc10, sps30Data.nc4), 1) + " #/cm3";
  tft.drawString(pm, tft.width()/2, 232, 1);
}

bool checkGPIOPressedOnce(){
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    delay(10);

    if (gpio_get_level(BUTTON_GPIO) == 0) {
        //Serial.println("GPIO pressed");
        if (!PRESSED) {
          PRESSED = true;
          return true;
        }else return false;
    } else {
        //Serial.println("GPIO is not pressed");
        PRESSED = false;
        return false;
    }
}

void setup() {
  Serial.begin(115200);
  
  //disable radio
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_bt_controller_disable();
  esp_bt_controller_deinit();

  //sps30 sensor
  sps30Serial.enableRxInternalPull(false);
  sps30Serial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  sps30.begin(sps30Serial);
  sps30.stopMeasurement();
  int16_t startError = sps30.startMeasurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_FLOAT);
  if (startError != 0) {
    Serial.printf("[SPS30:ERROR] startMeasurement error=%d\n", startError);
  } else {
    Serial.println("[SENSOR:INFO] SPS30 reader starting (115200 baud)...");
    delay(1000);  // Wait for the first SPS30 measurement to become available.
  }

  //access LittleFS, for font
  if (!LittleFS.begin()) {
    Serial.println("[LITTLEFS:ERROR] Init");
  }
  else{
    Serial.println("[LITTLEFS:INFO] Init OK");
  }
  
  //tft
  spr.setColorDepth(16);
  tft.init();
  tft.setRotation(ORIENTATION);
}

void loop() {
  if (checkGPIOPressedOnce()){
    if (REGION == 1) {
      REGION = 2;
    }
    else{
      if (REGION == 2) {
        REGION = 1;
      }
    }
  }

  if (!readSPS30(sps30Data)) {
    delay(1000);  // Retry at the sensor sampling interval.
    return;
  }

  pm25 = sps30Data.pm25;
  aqi = aqi_from_pm25(pm25);

  toSerial();
  toLCD();

  delay(2000);
}