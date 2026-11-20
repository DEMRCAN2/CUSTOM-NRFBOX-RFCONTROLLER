#include <Arduino.h>
#include <U8g2lib.h>
#include <stdint.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
#include "icon.h"
#include "neopixel.h"
#include "packetmonitor.h" 
#include "jammer.h"
#include "blejammer.h"
#include "spoofer.h"
#include "sourapple.h"
#include "blescan.h"
#include "wifiscan.h"
#include "blackout.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define BUTTON_UP_PIN 26 
#define BUTTON_SELECT_PIN 32
#define BUTTON_DOWN_PIN 33 
#define CE_PIN_A 5
#define CSN_PIN_A 17
#define CE_PIN_B 16
#define CSN_PIN_B 4
#define CE_PIN_C 15
#define CSN_PIN_C 2

RF24 RadioA(CE_PIN_A, CSN_PIN_A);
RF24 RadioB(CE_PIN_B, CSN_PIN_B);
RF24 RadioC(CE_PIN_C, CSN_PIN_C);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Adafruit_NeoPixel pixels(1, 14, NEO_GRB + NEO_KHZ800);
extern uint8_t oledBrightness;

int button_up_clicked=0, button_select_clicked=0, button_down_clicked=0, item_selected=0, current_screen=0;
#define MATRIX_COLS 6
int matrixY[MATRIX_COLS] = {0, -10, -20, -15, -25, -12};

float animatedY = 0.0;     
float targetY = 0.0;       
float menuVelocity = 0.0;  
const float TORK_GUCU = 0.24;  
const float SURTUNME  = 0.65;  

const int NUM_ITEMS = 9; 
const int MAX_ITEM_LENGTH = 20; 
char menu_items [NUM_ITEMS] [MAX_ITEM_LENGTH] = {  
  { "Packet Monitor" }, { "WLAN Jammer" }, { "Proto Kill" },
  { "BLE Jammer" }, { "BLE Spoofer" }, { "Sour Apple" }, { "BLE Scan" },
  { "WiFi Scan" }, { "About" }
};

int pixelGlitchFade = 0; 

void about() {
  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(7, 15, "dolar43lira/insta"); u8g2.drawStr(12, 35, "GitHub/NOTYET"); u8g2.drawStr(7, 55, "Tiktok/demircn");
  u8g2.sendBuffer();
}

void configureNrf(RF24 &radio) {
  if (!radio.begin()) return; 
  radio.setAutoAck(false); radio.stopListening(); radio.setRetries(0,0);
  radio.setPALevel(RF24_PA_MAX,true); radio.setDataRate(RF24_2MBPS); radio.setCRCLength(RF24_CRC_DISABLED);
}

void restoreSPIBus() { 
  RadioA.powerDown(); RadioB.powerDown(); RadioC.powerDown(); delay(10);
  SPI.endTransaction(); SPI.end(); 
  u8g2.begin(); u8g2.setContrast(oledBrightness); u8g2.setBitmapMode(1);
  configureNrf(RadioA); configureNrf(RadioB); configureNrf(RadioC);
}

void triggerEntranceGlitch() {
  for (int f = 0; f < 3; f++) {
    u8g2.clearBuffer();
    for (int g = 0; g < 3; g++) u8g2.drawHLine(random(-10, 10), random(0, 64), 128);
    u8g2.sendBuffer(); delay(10);
  }
}

// İSTEDİĞİN KUSURSUZ TV KARINCALANMA MOTORU (3 Saniye Kesintisiz Saf Parazit)
void customCyberBoot() {
  unsigned long startNoise = millis();
  
  // Tam 3000 milisaniye (3 saniye) boyunca saniyede onlarca kez tam ekran gürültü basar
  while (millis() - startNoise < 1000) {
    u8g2.clearBuffer();
    
    // OLED ekran belleğini rastgele piksellerle (TV Snow) hallaç pamuğu gibi dolduruyoruz
    for (int pCount = 0; pCount < 450; pCount++) {
      u8g2.drawPixel(random(0, 128), random(0, 64));
    }
    
    // Aralara gerçek karıncalanma çizgileri atıyoruz
    if (random(0, 3) == 0) {
      u8g2.drawHLine(0, random(0, 64), 128);
      u8g2.drawVLine(random(0, 128), 0, 64);
    }
    
    u8g2.sendBuffer();
    delay(15); // TV parazit yenilenme frekansı
  }

  // Menüye bağlanırken parazitin pikselsel sönümlenerek erimesi
  for (int density = 250; density > 0; density -= 40) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_7x14_tf);
    int16_t w = u8g2.getUTF8Width(menu_items[0]);
    u8g2.drawStr((118 - w) / 2, 37, menu_items[0]);
    u8g2.drawCircle(112, 32, 2);
    
    for (int p = 0; p < density; p++) u8g2.drawPixel(random(0, 128), random(0, 64));
    u8g2.sendBuffer();
    delay(20);
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); neopixelSetup();
  configureNrf(RadioA); configureNrf(RadioB); configureNrf(RadioC);
  EEPROM.begin(512); oledBrightness = EEPROM.read(1);
  if(oledBrightness < 10 || oledBrightness > 255) oledBrightness = 255;
  u8g2.begin(); u8g2.setContrast(oledBrightness); u8g2.setBitmapMode(1);
  
  customCyberBoot(); // TV Karıncalanma şovu başladı!
  
  u8g2.clearBuffer(); u8g2.drawXBMP(0,0,128,64,logo_cifer); u8g2.sendBuffer(); delay(250);   
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP); pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP); pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP); 
  animatedY = 0.0; targetY = 0.0; current_screen = 0;
}

void drawRuletMenu() {
  u8g2.clearBuffer();
  if (pixelGlitchFade > 0) { u8g2.setContrast(oledBrightness / (pixelGlitchFade + 1)); pixelGlitchFade--; } 
  else { u8g2.setContrast(oledBrightness); }

  for (int i = 0; i < MATRIX_COLS; i++) {
    int colX = i * 20 + 14; u8g2.drawPixel(colX, matrixY[i]); u8g2.drawPixel(colX, matrixY[i] - 3); 
    matrixY[i] += 1; if (matrixY[i] > 64) matrixY[i] = 0;
  }

  float diff = targetY - animatedY;
  float maxIndexDist = NUM_ITEMS * 22.0;
  if (diff > maxIndexDist / 2.0) animatedY += maxIndexDist;
  else if (diff < -maxIndexDist / 2.0) animatedY -= maxIndexDist;

  menuVelocity = (menuVelocity + (targetY - animatedY) * TORK_GUCU) * SURTUNME; animatedY += menuVelocity;

  int wiggle = (int)(menuVelocity * 1.2);
  u8g2.drawCircle(112, 20 + wiggle, 2); u8g2.drawDisc(112, 32 + wiggle, 2); u8g2.drawCircle(112, 44 + wiggle, 2); 

  int vY = abs((int)(menuVelocity * 10.0)); if(vY > 14) vY = 12;
  int randomNoise = random(0, 3);
  u8g2.drawBox(2, 32 - (vY + randomNoise), 2, (vY + randomNoise) * 2 + 2); 
  u8g2.drawBox(124, 32 - (vY + randomNoise), 2, (vY + randomNoise) * 2 + 2); 

  bool applyPixelGlitch = (abs(menuVelocity) > 0.7);

  for (int i = -NUM_ITEMS; i < NUM_ITEMS * 2; i++) {
    int actualIndex = (i % NUM_ITEMS + NUM_ITEMS) % NUM_ITEMS;
    float itemY = 32.0 + (i * 22.0) - animatedY; if (itemY < -10 || itemY > 74) continue; 
    float distanceToCenter = abs(itemY - 32.0);
    int pOffset = applyPixelGlitch ? random(-1, 2) : 0;

    if (distanceToCenter < 5.5) {
      u8g2.setFont(u8g2_font_7x14_tf); int16_t w = u8g2.getUTF8Width(menu_items[actualIndex]);
      int targetX = (118 - w) / 2; int targetYInt = (int)itemY + 5;
      if(applyPixelGlitch) {
        for(int px = targetX; px < targetX + w; px += 2) {
          if(random(0,2) == 0) u8g2.drawStr(px, targetYInt + random(-1, 2), String(menu_items[actualIndex][(px - targetX)/7]).c_str());
        }
      } else { u8g2.drawStr(targetX, targetYInt, menu_items[actualIndex]); }
    } 
    else if (distanceToCenter >= 5.5 && distanceToCenter < 24.0) {
      u8g2.setFont(u8g2_font_4x6_tr); int16_t w = u8g2.getUTF8Width(menu_items[actualIndex]);
      u8g2.drawStr((118 - w) / 2, (int)itemY + 2, menu_items[actualIndex]); 
    }
  }
  u8g2.sendBuffer();
}

void loop() {
  if (current_screen == 0) { 
      if ((digitalRead(BUTTON_UP_PIN) == LOW) && (button_up_clicked == 0)) { 
        item_selected--; button_up_clicked = 1; 
        if (item_selected < 0) { item_selected = NUM_ITEMS - 1; targetY = item_selected * 22.0; } 
        else { targetY = item_selected * 22.0; }
      }
      else if ((digitalRead(BUTTON_DOWN_PIN) == LOW) && (button_down_clicked == 0)) { 
        item_selected++; button_down_clicked = 1; 
        if (item_selected >= NUM_ITEMS) { item_selected = 0; targetY = item_selected * 22.0; } 
        else { targetY = item_selected * 22.0; }
      } 
      if (digitalRead(BUTTON_UP_PIN) == HIGH) button_up_clicked = 0;
      if (digitalRead(BUTTON_DOWN_PIN) == HIGH) button_down_clicked = 0;
      drawRuletMenu();
  }

  if (current_screen == 1) {
    if (item_selected == 8) about();
    else if (item_selected == 7) wifiscanLoop();
    else if (item_selected == 6) blescanLoop();
    else if (item_selected == 5) sourappleLoop();
    else if (item_selected == 4) spooferLoop();
    else if (item_selected == 3) blejammerLoop();
    else if (item_selected == 2) blackoutLoop();
    else if (item_selected == 1) jammerLoop();
    else if (item_selected == 0) packetmonitorLoop();

    if (digitalRead(BUTTON_SELECT_PIN) == LOW && button_select_clicked == 0) {
      button_select_clicked = 1; current_screen = 0; restoreSPIBus(); delay(250);
    }
  }

  if ((digitalRead(BUTTON_SELECT_PIN) == LOW) && (button_select_clicked == 0) && (current_screen == 0)) { 
     button_select_clicked = 1; triggerEntranceGlitch(); current_screen = 1; 
     if (item_selected == 7) wifiscanSetup();
     else if (item_selected == 6) blescanSetup();
     else if (item_selected == 5) sourappleSetup();
     else if (item_selected == 4) spooferSetup();
     else if (item_selected == 3) blejammerSetup();
     else if (item_selected == 2) blackoutSetup();
     else if (item_selected == 1) jammerSetup();
     else if (item_selected == 0) packetmonitorSetup(); 
     delay(200);
  }
  if (digitalRead(BUTTON_SELECT_PIN) == HIGH) button_select_clicked = 0;
}

