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
 
#include "WebLog.h"
#include "Settings.h"

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

SpanWebLog::SpanWebLog(uint16_t maxEntries, const char *serv, const char *tz, const char *url){
  this->maxEntries=maxEntries;
  timeServer=serv;
  timeZone=tz;
  statusURL="GET /" + String(url) + " ";
  log = (log_t *)calloc(maxEntries,sizeof(log_t));
}

///////////////////////////////

void SpanWebLog::initTime(void *args){
  SpanWebLog *wLog = (SpanWebLog *)args;
  
  WEBLOG("Acquiring Time from %s (%s)",wLog->timeServer,wLog->timeZone,wLog->waitTime/1000);
  configTzTime(wLog->timeZone,wLog->timeServer);
  struct tm timeinfo;
  if(getLocalTime(&timeinfo,wLog->waitTime)){
    strftime(wLog->bootTime,sizeof(wLog->bootTime),"%c",&timeinfo);
    wLog->timeInit=true;
    WEBLOG("Time Acquired: %s",wLog->bootTime);
  } else {
    WEBLOG("Can't access Time Server after %d seconds",wLog->waitTime/1000);
  }

  vTaskDelete(NULL);  
}

///////////////////////////////

void SpanWebLog::vLog(boolean sysMsg, const char *fmt, ...){

  char *buf;
  va_list ap;
  va_start(ap,fmt);
  vasprintf(&buf,fmt,ap);
  va_end(ap);  

  if(!sysMsg)
    LOG1("WEBLOG: %s\n",buf);
  
  if(maxEntries>0){
    int index=nEntries%maxEntries;
  
    log[index].upTime=esp_timer_get_time();
    if(timeInit)
      getLocalTime(&log[index].clockTime,10);
    else
      log[index].clockTime.tm_year=0;
  
    log[index].message=(char *)realloc(log[index].message, strlen(buf) + 1);
    strcpy(log[index].message, buf);
    
    log[index].clientIP=lastClientIP;
    nEntries++;
  }

  free(buf);
}
  
//////////////////////////////////////

SpanWebLog *WebLog=NULL;        // create global pointer

//////////////////////////////////////
