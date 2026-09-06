#include <esp_wifi.h>
#include <esp_bt.h>
#include "driver/gpio.h"

#define BUTTON_GPIO GPIO_NUM_22
bool PRESSED = false;

#include <HardwareSerial.h>
HardwareSerial pmsSerial(2);

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

const int RX_PIN = 27;
const int TX_PIN = 22;

struct PMS5003Data {
  //calibrate factor 1
  uint16_t pm10_cf1;
  uint16_t pm25_cf1;
  uint16_t pm100_cf1;
  //atmospheric calibrated
  uint16_t pm10_atm;
  uint16_t pm25_atm;
  uint16_t pm100_atm;
  //particles
  uint16_t part03;
  uint16_t part05;
  uint16_t part10;
  uint16_t part25;
  uint16_t part50;
  uint16_t part100;
};

PMS5003Data pms;

float pm25 = 0;
int aqi = 0;

uint16_t be16(const uint8_t *buf, int idx) {
  return (uint16_t(buf[idx]) << 8) | uint16_t(buf[idx+1]);
}

bool readPMShwframe(HardwareSerial &serial, PMS5003Data &data) {
  const int FRAME_LENGTH = 32;
  uint8_t buffer[FRAME_LENGTH];

  // Wait until at least 32 bytes are available
  if (serial.available() < FRAME_LENGTH) return false;

  // Align to header 0x42 0x4D
  while (serial.peek() != 0x42) serial.read(); // discard until 0x42
  if (serial.available() < FRAME_LENGTH) return false;
  serial.readBytes(buffer, FRAME_LENGTH);

  // Verify header
  if (buffer[0] != 0x42 || buffer[1] != 0x4D) return false;

  // Parse values (big-endian)
  data.pm10_cf1  = (buffer[4]  << 8) | buffer[5];
  data.pm25_cf1  = (buffer[6]  << 8) | buffer[7];
  data.pm100_cf1 = (buffer[8]  << 8) | buffer[9];

  data.pm10_atm  = (buffer[10] << 8) | buffer[11];
  data.pm25_atm  = (buffer[12] << 8) | buffer[13];
  data.pm100_atm = (buffer[14] << 8) | buffer[15];

  data.part03  = (buffer[16] << 8) | buffer[17];
  data.part05  = (buffer[18] << 8) | buffer[19];
  data.part10  = (buffer[20] << 8) | buffer[21];
  data.part25  = (buffer[22] << 8) | buffer[23];
  data.part50  = (buffer[24] << 8) | buffer[25];
  data.part100 = (buffer[26] << 8) | buffer[27];

  // Checksum (optional)
  uint16_t checksum = (buffer[30] << 8) | buffer[31];
  uint16_t sum = 0;
  for (int i = 0; i < 30; i++) sum += buffer[i];
  if (sum != checksum) return false;

  return true;
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
  Serial.printf("[SENSOR:INFO] %i PM1:%u|%u PM2.5:%u|%u PM10:%u|%u (µg/m3), 0.3µm:%u 0.5µm:%u 10µm:%u 25µm:%u 50µm:%u 100µm:%u, AQI:%i\n", REGION, pms.pm10_cf1, pms.pm10_atm, pms.pm25_cf1, pms.pm25_atm, pms.pm100_cf1, pms.pm100_atm, pms.part03, pms.part05, pms.part10, pms.part25, pms.part50, pms.part100, aqi);
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

  String pm = sRegion + " PM1:" + pms.pm10_cf1 + "|" + pms.pm10_atm + " PM2.5:" + pms.pm25_cf1 + "|" + pms.pm25_atm + " PM10:" + pms.pm100_cf1 + "|" + pms.pm100_atm + " ug/m3";
  tft.drawString(pm, tft.width()/2, 221, 1);

  pm = String("0.3:") + pms.part03 + " 0.5:" + pms.part05 + " 10:" + pms.part10 + " 25:" + pms.part25 + " 50:" + pms.part50 + " 100:" + pms.part100 + " um";
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
        Serial.println("GPIO pressed");
        if (!PRESSED) {
          PRESSED = true;
          return true;
        }else return false;
    } else {
        Serial.println("GPIO is not pressed");
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

  //pms5003 sensor
  pmsSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial.println("[SENSOR:INFO] PMS5003 reader starting (9600 baud)...");

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

  if (readPMShwframe(pmsSerial, pms)) {
    delay(10);
    return;
  }

  pm25 = float(pms.pm25_atm);
  aqi = aqi_from_pm25(pm25);

  toSerial();
  toLCD();

  delay(2000);
}