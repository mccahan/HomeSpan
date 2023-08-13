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

#include "Settings.h"
#include "SpanPoint.h"

#include <WiFi.h>
#include <esp_wifi.h>
#include <mbedtls/sha256.h>
#include <nvs_flash.h>
 
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

SpanPoint::SpanPoint(const char *macAddress, int sendSize, int receiveSize, int queueDepth, boolean useAPaddress){

  if(sscanf(macAddress,"%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",peerInfo.peer_addr,peerInfo.peer_addr+1,peerInfo.peer_addr+2,peerInfo.peer_addr+3,peerInfo.peer_addr+4,peerInfo.peer_addr+5)!=6){
    LOG0("\nFATAL ERROR!  Can't create new SpanPoint(\"%s\") - Invalid MAC Address ***\n",macAddress);
    LOG0("\n=== PROGRAM HALTED ===");
    while(1);
  }

  if(sendSize<0 || sendSize>200 || receiveSize<0 || receiveSize>200 || queueDepth<1 || (sendSize==0 && receiveSize==0)){
    LOG0("\nFATAL ERROR!  Can't create new SpanPoint(\"%s\",%d,%d,%d) - one or more invalid parameters ***\n",macAddress,sendSize,receiveSize,queueDepth);
    LOG0("\n=== PROGRAM HALTED ===");
    while(1);
  }

  this->sendSize=sendSize;
  this->receiveSize=receiveSize;
  
  if(receiveSize>0)
    WiFi.mode(WIFI_AP_STA);
  else if(WiFi.getMode()==WIFI_OFF)
    WiFi.mode(WIFI_STA);    

  init();                             // initialize SpanPoint
  peerInfo.channel=0;                 // 0 = matches current WiFi channel
  
  peerInfo.ifidx=useAPaddress?WIFI_IF_AP:WIFI_IF_STA;         // specify interface as either STA or AP
  
  peerInfo.encrypt=true;              // turn on encryption for this peer
  memcpy(peerInfo.lmk, lmk, 16);      // set local key
  esp_now_add_peer(&peerInfo);        // add peer to ESP-NOW

  if(receiveSize>0)
    receiveQueue = xQueueCreate(queueDepth,receiveSize);  

  SpanPoints.push_back(this);             
}

///////////////////////////////

void SpanPoint::init(const char *password){

  if(initialized)
    return;

  if(WiFi.getMode()==WIFI_OFF)
    WiFi.mode(WIFI_STA);  
  
  wifi_config_t conf;                       // make sure AP is hidden (if WIFI_AP_STA is used), since it is just a "dummy" AP to keep WiFi alive for ESP-NOW
  esp_wifi_get_config(WIFI_IF_AP,&conf);
  conf.ap.ssid_hidden=1;
  esp_wifi_set_config(WIFI_IF_AP,&conf);
    
  uint8_t hash[32];
  mbedtls_sha256_ret((const unsigned char *)password,strlen(password),hash,0);      // produce 256-bit bit hash from password

  esp_now_init();                           // initialize ESP-NOW
  memcpy(lmk, hash, 16);                    // store first 16 bytes of hash for later use as local key
  esp_now_set_pmk(hash+16);                 // set hash for primary key using last 16 bytes of hash
  esp_now_register_recv_cb(dataReceived);   // set callback for receiving data
  esp_now_register_send_cb(dataSent);       // set callback for sending data
  
  statusQueue = xQueueCreate(1,sizeof(esp_now_send_status_t));    // create statusQueue even if not needed
  setChannelMask(channelMask);                                    // default channel mask at start-up uses channels 1-13  

  uint8_t channel;
  if(!isHub){                                                   // this is not a hub
    nvs_flash_init();                                           // initialize NVS
    nvs_open("POINT",NVS_READWRITE,&pointNVS);                  // open SpanPoint data namespace in NVS
    if(!nvs_get_u8(pointNVS,"CHANNEL",&channel)){               // if channel found in NVS...
      if(channelMask & (1<<channel))                            // ... and if channel is allowed by channel mask
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);   // set the WiFi channel
    }
  }
  
  initialized=true;
}

///////////////////////////////

void SpanPoint::setChannelMask(uint16_t mask){
  channelMask = mask & 0x3FFE;

  if(isHub)
    return;

  uint8_t channel=0;

  for(int i=1;i<=13 && channel==0;i++)          // find first "allowed" channel based on mask
    channel=(channelMask & (1<<i))?i:0;

  if(channel==0){
    LOG0("\nFATAL ERROR!  SpanPoint::setChannelMask(0x%04X) - mask must allow for at least one channel ***\n",mask);
    LOG0("\n=== PROGRAM HALTED ===");
    while(1);
  }

  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

///////////////////////////////

uint8_t SpanPoint::nextChannel(){

  uint8_t channel;
  wifi_second_chan_t channel2; 
  esp_wifi_get_channel(&channel,&channel2);     // get current channel

  if(isHub || channelMask==(1<<channel))        // do not change channel if device is either a hub, or channel mask does not allow for any other channels
    return(channel);

  do {
    channel=(channel<13)?channel+1:1;       // advance to next channel
  } while(!(channelMask & (1<<channel)));   // until we find next valid one

  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);         // set the WiFi channel
  
  nvs_set_u8(pointNVS,"CHANNEL",channel);
  nvs_commit(pointNVS);  
     
  return(channel);  
}

///////////////////////////////

boolean SpanPoint::get(void *dataBuf){

  if(receiveSize==0)
    return(false);

  return(xQueueReceive(receiveQueue, dataBuf, 0));
}

///////////////////////////////

boolean SpanPoint::send(const void *data){

  if(sendSize==0)
    return(false);
  
  uint8_t channel;
  wifi_second_chan_t channel2; 
  esp_wifi_get_channel(&channel,&channel2);     // get current channel
  uint8_t startingChannel=channel;              // set starting channel to current channel

  esp_now_send_status_t status = ESP_NOW_SEND_FAIL;

  do {
    for(int i=1;i<=3;i++){
      
      LOG1("SpanPoint: Sending %d bytes to MAC Address %02X:%02X:%02X:%02X:%02X:%02X using channel %hhu...\n",
        sendSize,peerInfo.peer_addr[0],peerInfo.peer_addr[1],peerInfo.peer_addr[2],peerInfo.peer_addr[3],peerInfo.peer_addr[4],peerInfo.peer_addr[5],channel);
        
      esp_now_send(peerInfo.peer_addr, (uint8_t *) data, sendSize);
      xQueueReceive(statusQueue, &status, pdMS_TO_TICKS(2000));
      if(status==ESP_NOW_SEND_SUCCESS)
        return(true);
      delay(10);
    }    
    channel=nextChannel();
  } while(channel!=startingChannel);

  return(false);
} 

///////////////////////////////

void SpanPoint::dataReceived(const uint8_t *mac, const uint8_t *incomingData, int len){
  
  auto it=SpanPoints.begin();
  for(;it!=SpanPoints.end() && memcmp((*it)->peerInfo.peer_addr,mac,6)!=0; it++);
  
  if(it==SpanPoints.end())
    return;

  if((*it)->receiveSize==0)
    return;

  if(len!=(*it)->receiveSize){
    LOG0("SpanPoint Warning! %d bytes received from %02X:%02X:%02X:%02X:%02X:%02X does not match %d-byte queue size\n",len,mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],(*it)->receiveSize);
    return;
  }

  (*it)->receiveTime=millis();                             // set time of receive
  xQueueSend((*it)->receiveQueue, incomingData, 0);        // send to queue - do not wait if queue is full and instead fail immediately since we need to return from this function ASAP
}

///////////////////////////////

uint8_t SpanPoint::lmk[16];
boolean SpanPoint::initialized=false;
boolean SpanPoint::isHub=false;
vector<SpanPoint *> SpanPoint::SpanPoints;
uint16_t SpanPoint::channelMask=0x3FFE;
QueueHandle_t SpanPoint::statusQueue;
nvs_handle SpanPoint::pointNVS;

//////////////////////////////////////
