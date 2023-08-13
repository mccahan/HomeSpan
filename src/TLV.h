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

#define NUM_TLV_TAGS  11

// HAP TLV Types (HAP Table 5-6)

typedef enum {
  kTLVType_Method=0x00,
  kTLVType_Identifier=0x01,
  kTLVType_Salt=0x02,
  kTLVType_PublicKey=0x03,
  kTLVType_Proof=0x04,
  kTLVType_EncryptedData=0x05,
  kTLVType_State=0x06,
  kTLVType_Error=0x07,
  kTLVType_RetryDelay=0x08,
  kTLVType_Certificate=0x09,
  kTLVType_Signature=0x0A,
  kTLVType_Permissions=0x0B,
  kTLVType_FragmentData=0x0C,
  kTLVType_FragmentLast=0x0D,
  kTLVType_Flags=0x13,
  kTLVType_Separator=0xFF
} kTLVType;

class TLV {

  int cLen=0;            // total number of bytes in all defined TLV records, including TAG and LEN (suitable for use as Content-Length in HTTP Body)
  
  struct tlv_t {
    kTLVType tag;        // TAG
    int len;             // LENGTH
    uint8_t *val;        // VALUE buffer
    int maxLen;          // maximum length of VALUE buffer
    const char *name;    // abbreviated name of this TAG
  };

  tlv_t tlv[NUM_TLV_TAGS]= {
    {kTLVType_Separator,-1,NULL,0,"SEPARATOR"},
    {kTLVType_State,-1,NULL,1,"STATE"},
    {kTLVType_PublicKey,-1,NULL,384,"PUBKEY"},
    {kTLVType_Method,-1,NULL,1,"METHOD"},
    {kTLVType_Salt,-1,NULL,16,"SALT"},
    {kTLVType_Error,-1,NULL,1,"ERROR"},
    {kTLVType_Proof,-1,NULL,64,"PROOF"},
    {kTLVType_EncryptedData,-1,NULL,1024,"ENC.DATA"},
    {kTLVType_Signature,-1,NULL,64,"SIGNATURE"},
    {kTLVType_Identifier,-1,NULL,64,"IDENTIFIER"},
    {kTLVType_Permissions,-1,NULL,1,"PERMISSION"}
  };
  
  tlv_t *find(kTLVType tag);      // returns pointer to TLV record with matching TAG (or NULL if no match)

public:

  TLV();
    
  void clear();                                                 // clear all TLV structures
  int val(kTLVType tag);                                        // returns VAL for TLV with matching TAG (or -1 if no match)
  int val(kTLVType tag, uint8_t val);                           // sets and returns VAL for TLV with matching TAG (or -1 if no match)    
  uint8_t *buf(kTLVType tag);                                   // returns VAL Buffer for TLV with matching TAG (or NULL if no match)
  uint8_t *buf(kTLVType tag, size_t len);                       // set length and returns VAL Buffer for TLV with matching TAG (or NULL if no match, or if LEN>MAX, of if LEN=0)
  uint8_t *buf(kTLVType tag, const uint8_t *src, size_t len);   // copies len bytes of src into VAL buffer, and sets length to len, for TLV with matching TAG; returns VAL Buffer on success (or NULL if no match, or if LEN>MAX, or if LEN=0)
  int len(kTLVType tag);                                        // returns LEN for TLV matching TAG (or 0 if TAG is found but LEN not yet set; -1 if no match at all)
  void print(int minLogLevel=0);                                // prints all defined TLVs (those with length>0), subject to specified minimum log level
  int unpack(const uint8_t *tlvBuf, int nBytes);                // unpacks nBytes of TLV content from single byte buffer into individual TLV records (return 1 on success, 0 if fail) 
  int pack(uint8_t *tlvBuf);                                    // if tlvBuf!=NULL, packs all defined TLV records (LEN>0) into a single byte buffer, spitting large TLVs into separate 255-byte chunks.  Returns number of bytes (that would be) stored in buffer
  
};
