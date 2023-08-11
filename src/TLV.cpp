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

#include "TLV.h"

extern int HS_LogLevel;

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

TLV::TLV(){
}

//////////////////////////////////////

TLV::tlv_t *TLV::find(kTLVType tag){

  for(int i=0;i<NUM_TLV_TAGS;i++){
    if(tlv[i].tag==tag)
      return(tlv+i);
  }
  
  return(NULL);
}

//////////////////////////////////////

void TLV::clear(){

  cLen=0;

  for(int i=0;i<NUM_TLV_TAGS;i++){
    tlv[i].len=-1;
    tlv[i].val=(uint8_t *)realloc(tlv[i].val,0);
  }

}

//////////////////////////////////////

int TLV::val(kTLVType tag){

  tlv_t *tlv=find(tag);

  if(tlv && tlv->len>=0){
    if(tlv->maxLen>0)
      return(tlv->val[0]);
    else
      return(0);
  }

  return(-1);
}

//////////////////////////////////////

int TLV::val(kTLVType tag, uint8_t val){

  tlv_t *tlv=find(tag);
  
  if(tlv){
    if(tlv->maxLen>0){
      tlv->val=(uint8_t *)realloc(tlv->val,1);
      tlv->val[0]=val;
    }
    tlv->len=(tlv->maxLen>0);
    cLen+=tlv->len+2;
    return(val);
  }
  
  return(-1);
}

//////////////////////////////////////

uint8_t *TLV::buf(kTLVType tag){

  tlv_t *tlv=find(tag);

  if(tlv)
    return(tlv->val);
    
  return(NULL);
}

//////////////////////////////////////

uint8_t *TLV::buf(kTLVType tag, size_t len){

  return(buf(tag,NULL,len));
}

//////////////////////////////////////

uint8_t *TLV::buf(kTLVType tag, const uint8_t *src, size_t len){

  tlv_t *tlv=find(tag);
  
  if(tlv && tlv->maxLen>0 && len<=tlv->maxLen){
    tlv->len=len;
    cLen+=tlv->len;
    tlv->val=(uint8_t *)realloc(tlv->val,len);

    for(int i=0;i<tlv->len;i+=255)
      cLen+=2;

    if(src && tlv->val)
      memcpy(tlv->val,src,len);
      
    return(tlv->val);
  }
  
  return(NULL);
}

//////////////////////////////////////

void TLV::print(int minLogLevel){

  if(HS_LogLevel<minLogLevel)
    return;
    
  for(int i=0;i<NUM_TLV_TAGS;i++){
    
    if(tlv[i].len>=0){
      Serial.printf("%s(%d) ",tlv[i].name,tlv[i].len);
      
      for(int j=0;j<tlv[i].len;j++)
        Serial.printf("%02X",tlv[i].val[j]);

      Serial.printf("\n");

    } // len>0
  } // loop over all TLVs
}    

//////////////////////////////////////

int TLV::pack(uint8_t *tlvBuf){

  int n=0;
  int nBytes;

  for(int i=0;i<NUM_TLV_TAGS;i++){     
    
    if((nBytes=tlv[i].len)>=0){
      int j=0;
      do{
        int wBytes=nBytes>255?255:nBytes;
        if(tlvBuf!=NULL){
          *tlvBuf++=tlv[i].tag;
          *tlvBuf++=wBytes;
          memcpy(tlvBuf,tlv[i].val+j,wBytes);
          tlvBuf+=wBytes;
        }
        n+=wBytes+2;
        j+=wBytes;
        nBytes-=wBytes;      
      } while(nBytes>0);
    } // len>=0
    
  } // loop over all TLVs

return(n);  
}

//////////////////////////////////////

int TLV::len(kTLVType tag){
  
  tlv_t *tlv=find(tag);

  if(tlv)
    return(tlv->len>0?tlv->len:0);
    
  return(-1);
}

//////////////////////////////////////

int TLV::unpack(const uint8_t *tlvBuf, int nBytes){

  clear();

  kTLVType tag;
  int tagLen;
  uint8_t *val;
  int currentLen;
  int state=0;

  for(int i=0;i<nBytes;i++){
    
    switch(state){
      
      case 0:                                     // ready to read next tag
        if((tag=(kTLVType)tlvBuf[i])==-1){         // read TAG; return with error if not found
          clear();
          return(0);
        }
        state=1;
      break;

      case 1:                                     // ready to read tag length
        tagLen=tlvBuf[i];                         // read LEN
        currentLen=len(tag);                      // get current length of existing tag
        if(!(val=buf(tag,tagLen+currentLen))){    // get VAL Buffer for TAG and set LEN (returns NULL if LEN > maxLen)
          clear();
          return(0);
        }

        val+=currentLen;                          // move val to end of current length (tag repeats to load more than 255 bytes)
          
        if(tagLen==0)                             // no bytes to read
          state=0;
        else                                      // move to next state
          state=2;
      break;

      case 2:                                     // ready to read another byte into VAL
        *val=tlvBuf[i];                           // copy byte into VAL buffer
        val++;                                    // increment VAL buffer (already checked for sufficient length above)
        tagLen--;                                 // decrement number of bytes to continue copying
        if(tagLen==0)                             // no more bytes to copy
          state=0;
      break;

    } // switch
  } // for-loop

  if(state==0)            // should always end back in state=0
    return(1);            // return success

  clear();
  return(0);              // return fail
}

//////////////////////////////////////
