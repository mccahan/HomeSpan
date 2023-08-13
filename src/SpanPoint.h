/*********************************************************************************
 *  MIT License
 *  
 *  Copyright (c) 2020-2023 Gregg E. Berman
 *  
 *  https://github.com/HomeSpan/HomeSpan
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *  
 ********************************************************************************/
 
#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <nvs.h>

#include <vector>

using std::vector;

class SpanPoint {

  friend class Span;

  int receiveSize;                            // size (in bytes) of messages to receive
  int sendSize;                               // size (in bytes) of messages to send
  esp_now_peer_info_t peerInfo;               // structure for all ESP-NOW peer data
  QueueHandle_t receiveQueue;                 // queue to store data after it is received
  uint32_t receiveTime=0;                     // time (in millis) of most recent data received
  
  static uint8_t lmk[16];
  static boolean initialized;
  static boolean isHub;
  static vector<SpanPoint *> SpanPoints;
  static uint16_t channelMask;                // channel mask (only used for remote devices)
  static QueueHandle_t statusQueue;           // queue for communication between SpanPoint::dataSend and SpanPoint::send
  static nvs_handle pointNVS;                 // NVS storage for channel number (only used for remote devices)
  
  static void dataReceived(const uint8_t *mac, const uint8_t *incomingData, int len);
  static void init(const char *password="HomeSpan");
  static void setAsHub(){isHub=true;}
  static uint8_t nextChannel();
  
  static void dataSent(const uint8_t *mac, esp_now_send_status_t status) {
    xQueueOverwrite( statusQueue, &status );
  }
  
  public:

  SpanPoint(const char *macAddress, int sendSize, int receiveSize, int queueDepth=1, boolean useAPaddress=false);
  static void setPassword(const char *pwd){init(pwd);};      
  static void setChannelMask(uint16_t mask);  
  boolean get(void *dataBuf);
  boolean send(const void *data);
  uint32_t time(){return(millis()-receiveTime);}
  
};
