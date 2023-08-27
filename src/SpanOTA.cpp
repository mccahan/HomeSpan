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
#include "SpanOTA.h"
#include "HomeSpan.h"

#include <esp_ota_ops.h>

const __attribute__((section(".rodata_custom_desc"))) SpanPartition spanPartition = {HOMESPAN_MAGIC_COOKIE,0};

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

int SpanOTA::init(boolean _auth, boolean _safeLoad, const char *pwd){
  if(esp_ota_get_running_partition()==esp_ota_get_next_update_partition(NULL)){
    LOG0("\n*** WARNING: Can't start OTA Server - Partition table used to compile this sketch is not configured for OTA.\n\n");
    return(-1);     
  }
  
  safeLoad=_safeLoad;
  auth=_auth;
  homeSpan.reserveSocketConnections(1);
  if(pwd==NULL)
    return(0);
  return(setPassword(pwd));
}

///////////////////////////////

int SpanOTA::setPassword(const char *pwd){
  if(strlen(pwd)<1 || strlen(pwd)>32){
    LOG0("\n*** WARNING: Cannot change OTA password to '%s'. Password length must be between 1 and 32 characters.\n\n",pwd);
    return(-1);
  }

  MD5Builder otaPwdHash;
  otaPwdHash.begin();
  otaPwdHash.add(pwd);
  otaPwdHash.calculate();
  otaPwdHash.getChars(spanOTA->otaPwd);  
  return(0);
}

///////////////////////////////

void SpanOTA::start(){
  LOG0("\n*** Current Partition: %s\n*** New Partition: %s\n*** OTA Starting..",
    esp_ota_get_running_partition()->label,esp_ota_get_next_update_partition(NULL)->label);
  otaPercent=0;
  STATUS_UPDATE(start(LED_OTA_STARTED),HS_OTA_STARTED)
}

///////////////////////////////

void SpanOTA::end(){
  nvs_set_u8(homeSpan.otaNVS,"OTA_REQUIRED",safeLoad);
  nvs_commit(homeSpan.otaNVS);
  LOG0(" DONE!  Rebooting...\n");
  homeSpan.reboot();
}

///////////////////////////////

void SpanOTA::progress(uint32_t progress, uint32_t total){
  int percent=progress*100/total;
  if(percent/10 != otaPercent/10){
    otaPercent=percent;
    LOG0("%d%%..",progress*100/total);
  }

  if(safeLoad && progress==total){
    SpanPartition newSpanPartition;   
    esp_partition_read(esp_ota_get_next_update_partition(NULL), sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t), &newSpanPartition, sizeof(newSpanPartition));
    LOG0("Checking for HomeSpan Magic Cookie: %s..",newSpanPartition.magicCookie);
    if(strcmp(newSpanPartition.magicCookie,spanPartition.magicCookie))
      Update.abort();
  }
}

///////////////////////////////

void SpanOTA::error(ota_error_t err){
  LOG0("*** OTA Error[%u]: ", err);
  if (err == OTA_AUTH_ERROR) LOG0("Auth Failed\n\n");
    else if (err == OTA_BEGIN_ERROR) LOG0("Begin Failed\n\n");
    else if (err == OTA_CONNECT_ERROR) LOG0("Connect Failed\n\n");
    else if (err == OTA_RECEIVE_ERROR) LOG0("Receive Failed\n\n");
    else if (err == OTA_END_ERROR) LOG0("End Failed\n\n");
}

///////////////////////////////

int SpanOTA::otaPercent;
boolean SpanOTA::safeLoad;
boolean SpanOTA::auth;

//////////////////////////////////////

SpanOTA *spanOTA=NULL;        // create global pointer

//////////////////////////////////////
