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

struct SpanWebLog{                            // optional web status/log data
  uint16_t maxEntries=0;                      // max number of log entries;
  int nEntries=0;                             // total cumulative number of log entries
  const char *timeServer;                     // optional time server to use for acquiring clock time
  const char *timeZone;                       // optional time-zone specification
  boolean timeInit=false;                     // flag to indicate time has been initialized
  char bootTime[33]="Unknown";                // boot time
  String statusURL;                           // URL of status log
  uint32_t waitTime=120000;                   // number of milliseconds to wait for initial connection to time server
  String css="";                              // optional user-defined style sheet for web log
  String lastClientIP="0.0.0.0";              // IP address of last client accessing device
    
  struct log_t {                              // log entry type
    uint64_t upTime;                          // number of seconds since booting
    struct tm clockTime;                      // clock time
    char *message;                            // pointers to log entries of arbitrary size
    String clientIP;                          // IP address of client making request (or "0.0.0.0" if not applicable)
  } *log=NULL;                                // array of log entries 

  SpanWebLog(uint16_t maxEntries, const char *serv, const char *tz, const char *url);
  void vLog(boolean sysMsg, const char *fmt, ...);

  static void initTime(void *args);  
};

//////////////////////////////////////

extern SpanWebLog *WebLog;

//////////////////////////////////////
