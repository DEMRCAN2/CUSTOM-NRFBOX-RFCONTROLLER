#ifndef packetmonitor_H
#define packetmonitor_H

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <U8g2lib.h>

void packetmonitorSetup();
void packetmonitorLoop();

#endif
