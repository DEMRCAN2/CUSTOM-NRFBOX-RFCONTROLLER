/* ____________________________
   This software is licensed under the MIT License:
   https://github.com
   ________________________________________ */

#include <Arduino.h> 
#include "wifiscan.h"
#include <esp_wifi.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#define BTN_PIN_UP     27  
#define BTN_PIN_DOWN   33  
#define BTN_PIN_SELECT 26  
#define BTN_PIN_BACK   25  

int currentIndex = 0;
int listStartIndex = 0;
int totalNetworks = 0; 
bool isDetailView = false; 
unsigned long scan_StartTime = 0;
const unsigned long scanTimeout = 4000;
bool isScanComplete = false;

unsigned long lastButtonPress = 0;
unsigned long debounceTime = 200; 
unsigned long lastRateUpdateMillis = 0;
float currentPacketRate = 0.0;

// HATA DÜZELTİLDİ: Sayaç ve 6 baytlık MAC dizisi doğru tanımlandı
volatile unsigned long targetPacketCount = 0;
uint8_t targetBSSID[6]; 

#define BAR_COUNT 16  
float currentBarHeights[BAR_COUNT];
float barVelocities[BAR_COUNT];

float targetHeightGlobal = 0.0;
const float TORQUE_FORCE = 0.22; 
const float FRICTION     = 0.75; 

float wavePhase = 0.0;

void wifi_target_sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  uint8_t* payload = pkt->payload;
  if (pkt->rx_ctrl.sig_len < 24) return;
  bool match = true;
  for (int i = 0; i < 6; i++) {
    if (payload[4 + i] != targetBSSID[i] && payload[10 + i] != targetBSSID[i] && payload[16 + i] != targetBSSID[i]) {
      match = false;
      break;
    }
  }
  if (match) {
    targetPacketCount++;
  }
}

void wifiscanSetup() {
  Serial.begin(115200);
  u8g2.setFont(u8g2_font_6x10_tr);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  pinMode(BTN_PIN_UP, INPUT_PULLUP);
  pinMode(BTN_PIN_DOWN, INPUT_PULLUP);
  pinMode(BTN_PIN_SELECT, INPUT_PULLUP);
  pinMode(BTN_PIN_BACK, INPUT_PULLUP);
  currentIndex = 0;
  listStartIndex = 0;
  totalNetworks = 0;
  isDetailView = false;
  isScanComplete = false;
  for (int i = 0; i < BAR_COUNT; i++) {
    currentBarHeights[i] = 0.0;
    barVelocities[i] = 0.0;
  }
  totalNetworks = WiFi.scanNetworks(false, true); 
  isScanComplete = true;
  lastButtonPress = millis();
}

void wifiscanLoop() {
  unsigned long currentMillis = millis();

  if (isDetailView) {
    if (currentMillis - lastRateUpdateMillis >= 100) {
      float instantRate = targetPacketCount * 10.0;
      targetPacketCount = 0;
      lastRateUpdateMillis = currentMillis;
      currentPacketRate = (currentPacketRate * 0.3) + (instantRate * 0.7);
      if (currentPacketRate < 10) {
        targetHeightGlobal = 1.0;
      } else if (currentPacketRate < 100) { 
        targetHeightGlobal = 2.0 + (currentPacketRate * 0.03); 
      } else if (currentPacketRate < 300) { 
        targetHeightGlobal = 5.0 + ((currentPacketRate - 100) * 0.05);
      } else { 
        targetHeightGlobal = 15.0 + ((currentPacketRate - 300) * 0.035);
        if (targetHeightGlobal > 25.0) targetHeightGlobal = 25.0; 
      }
    }
    wavePhase += 0.12 + (targetHeightGlobal * 0.01);
  }

  if (currentMillis - lastButtonPress > debounceTime) {
    if (digitalRead(BTN_PIN_UP) == LOW) {
      if (!isDetailView) {
        if (currentIndex > 0) {
          currentIndex--;
          if (currentIndex < listStartIndex) listStartIndex--;
        }
      }
      lastButtonPress = currentMillis;
    } 
    else if (digitalRead(BTN_PIN_DOWN) == LOW) {
      if (!isDetailView) {
        if (currentIndex < totalNetworks - 1) { 
          currentIndex++;
          if (currentIndex >= listStartIndex + 5) listStartIndex++;
        }
      }
      lastButtonPress = currentMillis;
    } 
    else if (digitalRead(BTN_PIN_SELECT) == LOW) {
      lastButtonPress = currentMillis;
      if (!isDetailView && totalNetworks > 0) {
        uint8_t* bssidPtr = WiFi.BSSID(currentIndex);
        for (int i = 0; i < 6; i++) targetBSSID[i] = bssidPtr[i];
        int channel = WiFi.channel(currentIndex);
        WiFi.disconnect();
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous_rx_cb(&wifi_target_sniffer_cb);
        targetPacketCount = 0;
        currentPacketRate = 0.0;
        targetHeightGlobal = 1.0;
        for (int i = 0; i < BAR_COUNT; i++) {
          currentBarHeights[i] = 0.0;
          barVelocities[i] = 0.0;
        }
        lastRateUpdateMillis = millis();
        isDetailView = true; 
        setNeoPixelColour("blue");
      }
      delay(200); 
    } 
    else if (digitalRead(BTN_PIN_BACK) == LOW) {
      lastButtonPress = currentMillis;
      if (isDetailView) {
        esp_wifi_set_promiscuous(false);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        setNeoPixelColour("0");
        isDetailView = false;
        totalNetworks = WiFi.scanNetworks(false, true); 
      }
      delay(200);
    }
  }

  if (!isDetailView && isScanComplete) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 10, "Wi-Fi Networks:");
    if (totalNetworks <= 0) {
      u8g2.drawStr(10, 30, "No Networks Found");
    } else {
      for (int i = 0; i < 5; i++) {
        int currentNetworkIndex = i + listStartIndex;
        if (currentNetworkIndex >= totalNetworks) break;
        String networkName = WiFi.SSID(currentNetworkIndex);
        int rssi = WiFi.RSSI(currentNetworkIndex);
        String networkInfo = networkName.substring(0, 10);
        String networkrssi = " | " + String(rssi) + "dB";
        if (currentNetworkIndex == currentIndex) u8g2.drawStr(0, 24 + i * 10, ">");
        u8g2.drawStr(10, 24 + i * 10, networkInfo.c_str());
        u8g2.drawStr(75, 24 + i * 10, networkrssi.c_str());
      }
    }
    u8g2.sendBuffer();
  }

  if (isDetailView) {
    String networkName = WiFi.SSID(currentIndex);
    String networkBSSID = WiFi.BSSIDstr(currentIndex);
    int rssi = WiFi.RSSI(currentIndex);
    int channel = WiFi.channel(currentIndex);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 7, ("SSID: " + networkName.substring(0, 16)).c_str());
    u8g2.drawStr(0, 15, ("BSSID: " + networkBSSID).c_str());
    u8g2.drawStr(0, 23, ("CH: " + String(channel) + " | RSSI: " + String(rssi) + "dBm").c_str());
    u8g2.drawHLine(0, 25, 128); 
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 35, ("Traffic: " + String((int)currentPacketRate) + " Pkt/s").c_str());
    for (int i = 0; i < BAR_COUNT; i++) {
      float barTarget = targetHeightGlobal + (sin(wavePhase + (i * 0.45)) * (targetHeightGlobal * 0.5));
      if (barTarget < 1.0) barTarget = 1.0;
      float force = (barTarget - currentBarHeights[i]) * TORQUE_FORCE;
      barVelocities[i] = (barVelocities[i] + force) * FRICTION;
      currentBarHeights[i] += barVelocities[i];
      int barX = i * 8 + 1;
      int barH = (int)currentBarHeights[i];
      u8g2.drawBox(barX, 60 - barH, 6, barH);
    }
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 64, "Press BACK to Return List");
    u8g2.sendBuffer();
  }
}
