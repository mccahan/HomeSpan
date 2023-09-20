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
#include <ArduinoOTA.h>
#include <nvs.h>

struct SpanOTA{                               // manages OTA process
  
  char otaPwd[33]="";                         // MD5 Hash of OTA password, represented as a string of hexadecimal characters
  nvs_handle otaNVS;                          // storage for OTA data

  static boolean auth;                        // indicates whether OTA password is required
  static int otaPercent;
  static boolean safeLoad;                    // indicates whether OTA update should reject any application update that is not another HomeSpan sketch
  
  SpanOTA(boolean auth, boolean safeLoad, const char *pwd);
  int setPassword(const char *pwd, boolean save=false);
  void init(const char *hostname);
  void erase();
  static void start();
  static void end();
  static void progress(uint32_t progress, uint32_t total);
  static void error(ota_error_t err);
  static void handle(){ArduinoOTA.handle();}
};

struct SpanPartition{
  char magicCookie[32];
  uint8_t reserved[224];
};

//////////////////////////////////////

extern SpanOTA *spanOTA;

//////////////////////////////////////
