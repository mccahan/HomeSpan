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
 
#include <ESPmDNS.h>
#include <sodium.h>
#include <MD5Builder.h>
#include <mbedtls/version.h>

#include "HAP.h"

//////////////////////////////////////

void HAPClient::init(){

  size_t len;             // not used but required to read blobs from NVS

  nvs_open("SRP",NVS_READWRITE,&srpNVS);        // open SRP data namespace in NVS 
  nvs_open("HAP",NVS_READWRITE,&hapNVS);        // open HAP data namespace in NVS

  if(strlen(homeSpan.spanOTA.otaPwd)==0){                                 // OTA password has not been specified in sketch
    if(!nvs_get_str(homeSpan.otaNVS,"OTADATA",NULL,&len)){                // if found OTA data in NVS...
    nvs_get_str(homeSpan.otaNVS,"OTADATA",homeSpan.spanOTA.otaPwd,&len);  // ...retrieve data.
    } else {                                                              // otherwise...
    homeSpan.spanOTA.setPassword(DEFAULT_OTA_PASSWORD);                   // ...use default password
    }
  }

  if(strlen(homeSpan.pairingCodeCommand)){                          // load verification setup code if provided
    homeSpan.processSerialCommand(homeSpan.pairingCodeCommand);     // if load failed due to invalid code, the logic below still runs and will pick up previous code or use the default one
  } 

  struct {                                      // temporary structure to hold SRP verification code and salt stored in NVS
    uint8_t salt[16];
    uint8_t verifyCode[384];
  } verifyData;
 
  if(!nvs_get_blob(srpNVS,"VERIFYDATA",NULL,&len)){                   // if found verification code data in NVS
    nvs_get_blob(srpNVS,"VERIFYDATA",&verifyData,&len);                  // retrieve data
    srp.loadVerifyCode(verifyData.verifyCode,verifyData.salt);           // load verification code and salt into SRP structure
  } else {
    LOG0("Generating SRP verification data for default Setup Code: %.3s-%.2s-%.3s\n",homeSpan.defaultSetupCode,homeSpan.defaultSetupCode+3,homeSpan.defaultSetupCode+5);
    srp.createVerifyCode(homeSpan.defaultSetupCode,verifyData.verifyCode,verifyData.salt);         // create verification code from default Setup Code and random salt
    nvs_set_blob(srpNVS,"VERIFYDATA",&verifyData,sizeof(verifyData));                           // update data
    nvs_commit(srpNVS);                                                                         // commit to NVS
    LOG0("Setup Payload for Optional QR Code: %s\n\n",homeSpan.qrCode.get(atoi(homeSpan.defaultSetupCode),homeSpan.qrID,atoi(homeSpan.category)));
  }

  if(!strlen(homeSpan.qrID)){                                      // Setup ID has not been specified in sketch
    if(!nvs_get_str(hapNVS,"SETUPID",NULL,&len)){                    // check for saved value
      nvs_get_str(hapNVS,"SETUPID",homeSpan.qrID,&len);                 // retrieve data
    } else {
      sprintf(homeSpan.qrID,"%s",DEFAULT_QR_ID);                     // use default
   }
  }
  
  if(!nvs_get_blob(hapNVS,"ACCESSORY",NULL,&len)){                    // if found long-term Accessory data in NVS
    nvs_get_blob(hapNVS,"ACCESSORY",&accessory,&len);                 // retrieve data
  } else {      
    LOG0("Generating new random Accessory ID and Long-Term Ed25519 Signature Keys...\n\n");
    uint8_t buf[6];
    char cBuf[18];
    
    randombytes_buf(buf,6);                                              // generate 6 random bytes using libsodium (which uses the ESP32 hardware-based random number generator)
    sprintf(cBuf,"%02X:%02X:%02X:%02X:%02X:%02X",                        // create ID in form "XX:XX:XX:XX:XX:XX" (HAP Table 6-7)
      buf[0],buf[1],buf[2],buf[3],buf[4],buf[5]);

    memcpy(accessory.ID,cBuf,17);                                        // copy into Accessory ID for permanent storage
    crypto_sign_keypair(accessory.LTPK,accessory.LTSK);                  // generate new random set of keys using libsodium public-key signature
    
    nvs_set_blob(hapNVS,"ACCESSORY",&accessory,sizeof(accessory));    // update data
    nvs_commit(hapNVS);                                               // commit to NVS
  }

  if(!nvs_get_blob(hapNVS,"CONTROLLERS",NULL,&len)){                // if found long-term Controller Pairings data from NVS
    TempBuffer<Controller> tBuf(len/sizeof(Controller));
    nvs_get_blob(hapNVS,"CONTROLLERS",tBuf,&len);             // retrieve data
    for(int i=0;i<tBuf.size();i++){
      if(tBuf[i].allocated)
        controllerList.push_back(tBuf[i]);
    }
  }
  
  LOG0("Accessory ID:      ");
  charPrintRow(accessory.ID,17);
  LOG0("                               LTPK: ");
  hexPrintRow(accessory.LTPK,32);
  LOG0("\n");

  printControllers();                                                         

//  tlv8.create(kTLVType_Separator,0,"SEPARATOR");   // define the actual TLV records needed for the implementation of HAP; one for each kTLVType needed (HAP Table 5-6)
//  tlv8.create(kTLVType_State,1,"STATE");
//  tlv8.create(kTLVType_PublicKey,384,"PUBKEY");
//  tlv8.create(kTLVType_Method,1,"METHOD");
//  tlv8.create(kTLVType_Salt,16,"SALT");
//  tlv8.create(kTLVType_Error,1,"ERROR");
//  tlv8.create(kTLVType_Proof,64,"PROOF");
//  tlv8.create(kTLVType_EncryptedData,1024,"ENC.DATA");
//  tlv8.create(kTLVType_Signature,64,"SIGNATURE");
//  tlv8.create(kTLVType_Identifier,64,"IDENTIFIER");
//  tlv8.create(kTLVType_Permissions,1,"PERMISSION");

  if(!nvs_get_blob(hapNVS,"HAPHASH",NULL,&len)){                 // if found HAP HASH structure
    nvs_get_blob(hapNVS,"HAPHASH",&homeSpan.hapConfig,&len);     // retrieve data    
  } else {
    LOG0("Resetting Database Hash...\n");
    nvs_set_blob(hapNVS,"HAPHASH",&homeSpan.hapConfig,sizeof(homeSpan.hapConfig));     // save data (will default to all zero values, which will then be updated below)
    nvs_commit(hapNVS);                                                                // commit to NVS
  }

  if(homeSpan.updateDatabase(false)){       // create Configuration Number and Loop vector
    LOG0("\nAccessory configuration has changed.  Updating configuration number to %d\n",homeSpan.hapConfig.configNumber);
  }
  else{
    LOG0("\nAccessory configuration number: %d\n",homeSpan.hapConfig.configNumber);
  }

  LOG0("\n");

}

//////////////////////////////////////

void HAPClient::processRequest(){

  int nBytes, messageSize;

  messageSize=client.available();        

  if(messageSize>MAX_HTTP){            // exceeded maximum number of bytes allowed
    badRequestError();
    LOG0("\n*** ERROR:  HTTP message of %d bytes exceeds maximum allowed (%d)\n\n",messageSize,MAX_HTTP);
    return;
  }
 
  TempBuffer<uint8_t> httpBuf(messageSize+1);      // leave room for null character added below
  
  if(cPair){                           // expecting encrypted message
    LOG2("<<<< #### ");
    LOG2(client.remoteIP());
    LOG2(" #### <<<<\n");

    nBytes=receiveEncrypted(httpBuf,messageSize);   // decrypt and return number of bytes read      
        
    if(!nBytes){                           // decryption failed (error message already printed in function)
      badRequestError();              
      return;          
    }
        
  } else {                                            // expecting plaintext message  
    LOG2("<<<<<<<<< ");
    LOG2(client.remoteIP());
    LOG2(" <<<<<<<<<\n");
    
    nBytes=client.read(httpBuf,messageSize);   // read expected number of bytes

    if(nBytes!=messageSize || client.available()!=0){
      badRequestError();
      LOG0("\n*** ERROR:  HTTP message not read correctly.  Expected %d bytes, read %d bytes, %d bytes remaining\n\n",messageSize,nBytes,client.available());
      return;
    }
               
  } // encrypted/plaintext
      
  httpBuf[nBytes]='\0';   // add null character to enable string functions
      
  char *body=(char *)httpBuf.get();   // char pointer to start of HTTP Body
  char *p;                            // char pointer used for searches
     
  if(!(p=strstr((char *)httpBuf.get(),"\r\n\r\n"))){
    badRequestError();
    LOG0("\n*** ERROR:  Malformed HTTP request (can't find blank line indicating end of BODY)\n\n");
    return;      
  }

  *p='\0';                            // null-terminate end of HTTP Body to faciliate additional string processing
  uint8_t *content=(uint8_t *)p+4;    // byte pointer to start of optional HTTP Content
  int cLen=0;                         // length of optional HTTP Content

  if((p=strstr(body,"Content-Length: ")))       // Content-Length is specified
    cLen=atoi(p+16);
  if(nBytes!=strlen(body)+4+cLen){
    badRequestError();
    LOG0("\n*** ERROR:  Malformed HTTP request (Content-Length plus Body Length does not equal total number of bytes read)\n\n");
    return;        
  }

  LOG2(body);
  LOG2("\n------------ END BODY! ------------\n");

  if(!strncmp(body,"POST ",5)){                       // this is a POST request

    if(cLen==0){
      badRequestError();
      LOG0("\n*** ERROR:  HTTP POST request contains no Content\n\n");
    }
           
    else if(!strncmp(body,"POST /pair-setup ",17) && strstr(body,"Content-Type: application/pairing+tlv8"))            // POST PAIR-SETUP               
      postPairSetupURL(content,cLen);

    else if(!strncmp(body,"POST /pair-verify ",18) && strstr(body,"Content-Type: application/pairing+tlv8"))           // POST PAIR-VERIFY 
      postPairVerifyURL(content,cLen);
            
    else if(!strncmp(body,"POST /pairings ",15) && strstr(body,"Content-Type: application/pairing+tlv8"))              // POST PAIRINGS                
      postPairingsURL(content,cLen);

    else {
      notFoundError();
      LOG0("\n*** ERROR:  Bad POST request - URL not found\n\n");
    }
    
    return;                          
  } // POST request
          
  if(!strncmp(body,"PUT ",4)){                       // this is a PUT request

    if(cLen==0){
      badRequestError();
      LOG0("\n*** ERROR:  HTTP PUT request contains no Content\n\n");
      return;      
    }
           
    if(!strncmp(body,"PUT /characteristics ",21) &&                          // PUT CHARACTERISTICS
       strstr(body,"Content-Type: application/hap+json")){                   // check that content is JSON

      content[cLen]='\0';                                                    // add a trailing null on end of JSON
      LOG2((char *)content);                                         // print JSON
      LOG2("\n------------ END JSON! ------------\n");
               
      putCharacteristicsURL((char *)content);                           // process URL
      return;
    }

    if(!strncmp(body,"PUT /prepare ",13) &&                          // PUT PREPARE
       strstr(body,"Content-Type: application/hap+json")){                   // check that content is JSON

      content[cLen]='\0';                                                    // add a trailing null on end of JSON
      LOG2((char *)content);                                         // print JSON
      LOG2("\n------------ END JSON! ------------\n");
               
      putPrepareURL((char *)content);                           // process URL
      return;
    }
      
    notFoundError();
    LOG0("\n*** ERROR:  Bad PUT request - URL not found\n\n");
    return;                  
        
  } // PUT request           
      
  if(!strncmp(body,"GET ",4)){                       // this is a GET request
                    
    if(!strncmp(body,"GET /accessories ",17)){       // GET ACCESSORIES
      getAccessoriesURL();
      return;
    }

    if(!strncmp(body,"GET /characteristics?",21)){   // GET CHARACTERISTICS
      getCharacteristicsURL(body+21);
      return;
    }

    if(homeSpan.webLog.isEnabled && !strncmp(body,homeSpan.webLog.statusURL.c_str(),homeSpan.webLog.statusURL.length())){       // GET STATUS - AN OPTIONAL, NON-HAP-R2 FEATURE
      getStatusURL();
      return;
    }    

    notFoundError();
    LOG0("\n*** ERROR:  Bad GET request - URL not found\n\n");
    return;                  

  } // GET request
      
  badRequestError();
  LOG0("\n*** ERROR:  Unknown or malformed HTTP request\n\n");
                        
} // processHAP

//////////////////////////////////////

int HAPClient::notFoundError(){

  char s[]="HTTP/1.1 404 Not Found\r\n\r\n";
  LOG2("\n>>>>>>>>>> ");
  LOG2(client.remoteIP());
  LOG2(" >>>>>>>>>>\n");
  LOG2(s);
  client.print(s);
  LOG2("------------ SENT! --------------\n");
  
  delay(1);
  client.stop();

  return(-1);
}

//////////////////////////////////////

int HAPClient::badRequestError(){

  char s[]="HTTP/1.1 400 Bad Request\r\n\r\n";
  LOG2("\n>>>>>>>>>> ");
  LOG2(client.remoteIP());
  LOG2(" >>>>>>>>>>\n");
  LOG2(s);
  client.print(s);
  LOG2("------------ SENT! --------------\n");
  
  delay(1);
  client.stop();

  return(-1);
}

//////////////////////////////////////

int HAPClient::unauthorizedError(){

  char s[]="HTTP/1.1 470 Connection Authorization Required\r\n\r\n";
  LOG2("\n>>>>>>>>>> ");
  LOG2(client.remoteIP());
  LOG2(" >>>>>>>>>>\n");
  LOG2(s);
  client.print(s);
  LOG2("------------ SENT! --------------\n");
  
  delay(1);
  client.stop();

  return(-1);
}

//////////////////////////////////////

int HAPClient::postPairSetupURL(uint8_t *content, size_t len){

  HAPTLV iosTLV;
  HAPTLV responseTLV;
  HAPTLV subTLV;

  iosTLV.unpack(content,len);
  iosTLV.print();
  LOG2("------------ END TLVS! ------------\n");

  LOG2("In Pair Setup #%d (%s)...",conNum,client.remoteIP().toString().c_str());
  
  auto itState=iosTLV.find(kTLVType_State);

  if(iosTLV.len(itState)!=1){                                   // missing STATE TLV
    LOG0("\n*** ERROR: Missing or invalid 'State' TLV\n\n");
    badRequestError();                                          // return with 400 error, which closes connection      
    return(0);
  }

  int tlvState=(*itState)[0];

  if(nAdminControllers()){                                  // error: Device already paired (i.e. there is at least one admin Controller). We should not be receiving any requests for Pair-Setup!
    LOG0("\n*** ERROR: Device already paired!\n\n");
    responseTLV.add(kTLVType_State,tlvState+1);             // set response STATE to requested state+1 (which should match the state that was expected by the controller)
    responseTLV.add(kTLVType_Error,tagError_Unavailable);   // set Error=Unavailable
    tlvRespond(responseTLV);                                // send response to client
    return(0);
  };

  LOG2("Found <M%d>.  Expected <M%d>.\n",tlvState,pairStatus);

  if(tlvState!=pairStatus){                                         // error: Device is not yet paired, but out-of-sequence pair-setup STATE was received
    LOG0("\n*** ERROR: Out-of-Sequence Pair-Setup request!\n\n");
    responseTLV.add(kTLVType_State,tlvState+1);                     // set response STATE to requested state+1 (which should match the state that was expected by the controller)
    responseTLV.add(kTLVType_Error,tagError_Unknown);               // set Error=Unknown (there is no specific error type for out-of-sequence steps)
    tlvRespond(responseTLV);                                        // send response to client
    pairStatus=pairState_M1;                                        // reset pairStatus to first step of unpaired accessory (M1)
    return(0);
  };
   
  switch(tlvState){                                         // valid and in-sequence Pair-Setup STATE received -- process request!  (HAP Section 5.6)

    case pairState_M1:{                                     // 'SRP Start Request'

      auto itMethod=iosTLV.find(kTLVType_Method);

      if(iosTLV.len(itMethod)!=1 || (*itMethod)[0]!=0){                         // error: "Pair Setup" method must always be 0 to indicate setup without MiFi Authentification (HAP Table 5-3)
        LOG0("\n*** ERROR: Pair 'Method' missing or not set to 0\n\n");
        responseTLV.add(kTLVType_State,pairState_M2);                           // set State=<M2>
        responseTLV.add(kTLVType_Error,tagError_Unavailable);                   // set Error=Unavailable
        tlvRespond(responseTLV);                                                // send response to client
        return(0);
      };

      auto itPublicKey=responseTLV.add(kTLVType_PublicKey,384,NULL);        // create blank PublicKey TLV with space for 384 bytes
      auto itSalt=responseTLV.add(kTLVType_Salt,16,NULL);                   // create blank Salt TLV with space for 16 bytes

      responseTLV.add(kTLVType_State,pairState_M2);                         // set State=<M2>
      srp.createPublicKey();                                                // create accessory Public Key from Pair-Setup code (displayed to user)
      mbedtls_mpi_write_binary(&srp.B,*itPublicKey,(*itPublicKey).len);     // load server PublicKey, B, into TLV
      mbedtls_mpi_write_binary(&srp.s,*itSalt,(*itSalt).len);               // load Salt, s, into TLV
      tlvRespond(responseTLV);                                              // send response to client
      pairStatus=pairState_M3;                                              // set next expected pair-state request from client
      return(1);
    } 
    break;

    case pairState_M3:{                                     // 'SRP Verify Request'

      auto itPublicKey=iosTLV.find(kTLVType_PublicKey);
      auto itClientProof=iosTLV.find(kTLVType_Proof);

      if(iosTLV.len(itPublicKey)<=0 || iosTLV.len(itClientProof)<=0){
        LOG0("\n*** ERROR: One or both of the required 'PublicKey' and 'Proof' TLV records for this step is bad or missing\n\n");
        responseTLV.add(kTLVType_State,pairState_M4);                   // set State=<M4>
        responseTLV.add(kTLVType_Error,tagError_Unknown);               // set Error=Unknown (there is no specific error type for missing/bad TLV data)
        tlvRespond(responseTLV);                                        // send response to client
        pairStatus=pairState_M1;                                        // reset pairStatus to first step of unpaired
        return(0);
      };

      mbedtls_mpi_read_binary(&srp.A,*itPublicKey,(*itPublicKey).len);          // load client PublicKey TLV into A
      mbedtls_mpi_read_binary(&srp.M1,*itClientProof,(*itClientProof).len);     // load client Proof TLV into M1

      srp.createSessionKey();                                           // create session key, K, from receipt of client Public Key, A

      if(!srp.verifyProof()){                                           // verify client Proof, M1
        LOG0("\n*** ERROR: SRP Proof Verification Failed\n\n");
        responseTLV.add(kTLVType_State,pairState_M4);                   // set State=<M4>
        responseTLV.add(kTLVType_Error,tagError_Authentication);        // set Error=Authentication
        tlvRespond(responseTLV);                                        // send response to client
        pairStatus=pairState_M1;                                        // reset pairStatus to first step of unpaired
        return(0);        
      };

      auto itAccProof=responseTLV.add(kTLVType_Proof,64,NULL);              // create blank accessory Proof TLV with space for 64 bytes

      responseTLV.add(kTLVType_State,pairState_M4);                         // set State=<M4>
      srp.createProof();                                                    // M1 has been successully verified; now create accessory proof M2
      mbedtls_mpi_write_binary(&srp.M2,*itAccProof,(*itAccProof).len);      // load accessory Proof, M2, into TLV
      tlvRespond(responseTLV);                                              // send response to client
      pairStatus=pairState_M5;                                              // set next expected pair-state request from client
      return(1);        
    }
    break;
    
    case pairState_M5:{                                     // 'Exchange Request'

      auto itEncryptedData=iosTLV.find(kTLVType_EncryptedData);

      if(iosTLV.len(itEncryptedData)<=0){            
        LOG0("\n*** ERROR: Required 'EncryptedData' TLV record for this step is bad or missing\n\n");
        responseTLV.add(kTLVType_State,pairState_M6);                   // set State=<M6>
        responseTLV.add(kTLVType_Error,tagError_Unknown);               // set Error=Unknown (there is no specific error type for missing/bad TLV data)
        tlvRespond(responseTLV);                                        // send response to client
        pairStatus=pairState_M1;                                        // reset pairStatus to first step of unpaired
        return(0);
      };

      // THIS NEXT STEP IS MISSING FROM HAP DOCUMENTATION!
      //
      // Must FIRST use HKDF to create a Session Key from the SRP Shared Secret for use in subsequent ChaCha20-Poly1305 decryption
      // of the encrypted data TLV (HAP Sections 5.6.5.2 and 5.6.6.1).
      //
      // Note the SALT and INFO text fields used by HKDF to create this Session Key are NOT the same as those for creating iosDeviceX.
      // The iosDeviceX HKDF calculations are separate and will be performed further below with the SALT and INFO as specified in the HAP docs.

      hkdf.create(sessionKey,srp.sharedSecret,64,"Pair-Setup-Encrypt-Salt","Pair-Setup-Encrypt-Info");       // create SessionKey

      LOG2("------- DECRYPTING SUB-TLVS -------\n");
      
      // use SessionKey to decrypt encryptedData TLV with padded nonce="PS-Msg05"
                                  
      TempBuffer<uint8_t> decrypted((*itEncryptedData).len-crypto_aead_chacha20poly1305_IETF_ABYTES);        // temporary storage for decrypted data
       
      if(crypto_aead_chacha20poly1305_ietf_decrypt(decrypted, NULL, NULL, *itEncryptedData, (*itEncryptedData).len, NULL, 0, (unsigned char *)"\x00\x00\x00\x00PS-Msg05", sessionKey)==-1){          
        LOG0("\n*** ERROR: Exchange-Request Authentication Failed\n\n");
        responseTLV.add(kTLVType_State,pairState_M6);                   // set State=<M6>
        responseTLV.add(kTLVType_Error,tagError_Authentication);        // set Error=Authentication
        tlvRespond(responseTLV);                                        // send response to client
        pairStatus=pairState_M1;                                        // reset pairStatus to first step of unpaired
        return(0);        
      }

      subTLV.unpack(decrypted,decrypted.len());                         // unpack TLV      
      subTLV.print();                                                   // print decrypted TLV data
      
      LOG2("---------- END SUB-TLVS! ----------\n");

      auto itIdentifier=subTLV.find(kTLVType_Identifier);
      auto itSignature=subTLV.find(kTLVType_Signature);
      auto itPublicKey=subTLV.find(kTLVType_PublicKey);

      if(subTLV.len(itIdentifier)!=36 || subTLV.len(itSignature)!=crypto_sign_BYTES || subTLV.len(itPublicKey)!=crypto_sign_PUBLICKEYBYTES){ 
        LOG0("\n*** ERROR: One or more of required 'Identifier,' 'PublicKey,' and 'Signature' TLV records for this step is bad or missing\n\n");
        responseTLV.add(kTLVType_State,pairState_M6);                   // set State=<M6>
        responseTLV.add(kTLVType_Error,tagError_Unknown);               // set Error=Unknown (there is no specific error type for missing/bad TLV data)
        tlvRespond(responseTLV);                                        // send response to client
        pairStatus=pairState_M1;                                        // reset pairStatus to first step of unpaired
        return(0);
      };

      // Next, verify the authenticity of the TLV Records using the Signature provided by the Client.
      // But the Client does not send the entire message that was used to generate the Signature.
      // Rather, it purposely does not transmit "iosDeviceX", which is derived from the SRP Shared Secret that only the Client and this Server know.
      // Note that the SALT and INFO text fields now match those in HAP Section 5.6.6.1

      TempBuffer<uint8_t> iosDeviceX(32);
      hkdf.create(iosDeviceX,srp.sharedSecret,64,"Pair-Setup-Controller-Sign-Salt","Pair-Setup-Controller-Sign-Info");     // derive iosDeviceX (32 bytes) from SRP Shared Secret using HKDF 

      // Concatenate iosDeviceX, IOS ID, and IOS PublicKey into iosDeviceInfo
      
      TempBuffer<uint8_t> iosDeviceInfo(iosDeviceX.len()+(*itIdentifier).len+(*itPublicKey).len);
      Utils::memcat(iosDeviceInfo,3,iosDeviceX.get(),iosDeviceX.len(),(*itIdentifier).val.get(),(*itIdentifier).len,(*itPublicKey).val.get(),(*itPublicKey).len);

      if(crypto_sign_verify_detached(*itSignature, iosDeviceInfo, iosDeviceInfo.len(), *itPublicKey) != 0){                // verify signature of iosDeviceInfo using iosDeviceLTPK   
        LOG0("\n*** ERROR: LPTK Signature Verification Failed\n\n");
        responseTLV.add(kTLVType_State,pairState_M6);                   // set State=<M6>
        responseTLV.add(kTLVType_Error,tagError_Authentication);        // set Error=Authentication
        tlvRespond(responseTLV);                                        // send response to client
        pairStatus=pairState_M1;                                        // reset pairStatus to first step of unpaired
        return(0);                
      }

      addController(*itIdentifier,*itPublicKey,true);                   // save Pairing ID and LTPK for this Controller with admin privileges

      // Now perform the above steps in reverse to securely transmit the AccessoryLTPK to the Controller (HAP Section 5.6.6.2)

      TempBuffer<uint8_t> accessoryX(32);
      hkdf.create(accessoryX,srp.sharedSecret,64,"Pair-Setup-Accessory-Sign-Salt","Pair-Setup-Accessory-Sign-Info");       // derive accessoryX from SRP Shared Secret using HKDF 
      
      // Concatenate accessoryX, Accessory ID, and Accessory PublicKey into accessoryInfo

      TempBuffer<uint8_t> accessoryInfo(accessoryX.len()+sizeof(accessory.ID)+sizeof(accessory.LTPK));
      Utils::memcat(accessoryInfo,3,accessoryX.get(),accessoryX.len(),accessory.ID,sizeof(accessory.ID),accessory.LTPK,sizeof(accessory.LTPK));

      subTLV.clear();                                                                            // clear existing SUBTLV records

      itSignature=subTLV.add(kTLVType_Signature,64,NULL);                                        // create blank Signature TLV with space for 64 bytes

      crypto_sign_detached(*itSignature,NULL,accessoryInfo,accessoryInfo.len(),accessory.LTSK);  // produce signature of accessoryInfo using AccessoryLTSK (Ed25519 long-term secret key)

      subTLV.add(kTLVType_Identifier,sizeof(accessory.ID),accessory.ID);                         // set Identifier TLV record as accessoryPairingID
      subTLV.add(kTLVType_PublicKey,sizeof(accessory.LTPK),accessory.LTPK);                      // set PublicKey TLV record as accessoryLTPK

      LOG2("------- ENCRYPTING SUB-TLVS -------\n");

      subTLV.print();

      TempBuffer<uint8_t> subPack(subTLV.pack_size());                                          // create sub-TLV by packing Identifier, PublicKey and Signature TLV records together
      subTLV.pack(subPack);      

      // Encrypt the subTLV data using the same sessionKey as above with ChaCha20-Poly1305

      itEncryptedData=responseTLV.add(kTLVType_EncryptedData,subPack.len()+crypto_aead_chacha20poly1305_IETF_ABYTES,NULL);     //create blank EncryptedData TLV with space for subTLV + Authentication Tag

      crypto_aead_chacha20poly1305_ietf_encrypt(*itEncryptedData,NULL,subPack,subPack.len(),NULL,0,NULL,(unsigned char *)"\x00\x00\x00\x00PS-Msg06",sessionKey);
                                                   
      LOG2("---------- END SUB-TLVS! ----------\n");

      responseTLV.add(kTLVType_State,pairState_M6);         // set State=<M6>
      
      tlvRespond(responseTLV);                              // send response to client

      mdns_service_txt_item_set("_hap","_tcp","sf","0");    // broadcast new status
      
      LOG1("\n*** ACCESSORY PAIRED! ***\n");

      STATUS_UPDATE(on(),HS_PAIRED)      
            
      if(homeSpan.pairCallback)                             // if set, invoke user-defined Pairing Callback to indicate device has been paired
        homeSpan.pairCallback(true);
      
      return(1);        
    }       
    break;

  } // switch

  return(1);

} // postPairSetup

//////////////////////////////////////

int HAPClient::postPairVerifyURL(uint8_t *content, size_t len){

  HAPTLV iosTLV;
  HAPTLV responseTLV;
  HAPTLV subTLV;

  iosTLV.unpack(content,len);
  iosTLV.print();
  LOG2("------------ END TLVS! ------------\n");

  LOG2("In Pair Verify #%d (%s)...",conNum,client.remoteIP().toString().c_str());
  
  auto itState=iosTLV.find(kTLVType_State);

  if(iosTLV.len(itState)!=1){                                   // missing STATE TLV
    LOG0("\n*** ERROR: Missing or invalid 'State' TLV\n\n");
    badRequestError();                                          // return with 400 error, which closes connection      
    return(0);
  }

  int tlvState=(*itState)[0];

  if(!nAdminControllers()){                             // error: Device not yet paired - we should not be receiving any requests for Pair-Verify!
    LOG0("\n*** ERROR: Device not yet paired!\n\n");
    responseTLV.add(kTLVType_State,tlvState+1);         // set response STATE to requested state+1 (which should match the state that was expected by the controller)
    responseTLV.add(kTLVType_Error,tagError_Unknown);   // set Error=Unknown
    tlvRespond(responseTLV);                            // send response to client
    return(0);
  };

  LOG2("Found <M%d>\n",tlvState);          // unlike pair-setup, out-of-sequencing can be handled gracefully for pair-verify (HAP requirement). No need to keep track of pairStatus

  switch(tlvState){                        // Pair-Verify STATE received -- process request!  (HAP Section 5.7)

    case pairState_M1:{                    // 'Verify Start Request'

      auto itPublicKey=iosTLV.find(kTLVType_PublicKey);

      if(iosTLV.len(itPublicKey)!=32){            
        LOG0("\n*** ERROR: Required 'PublicKey' TLV record for this step is bad or missing\n\n");
        responseTLV.add(kTLVType_State,pairState_M2);        // set State=<M2>
        responseTLV.add(kTLVType_Error,tagError_Unknown);    // set Error=Unknown (there is no specific error type for missing/bad TLV data)
        tlvRespond(responseTLV);                             // send response to client
        return(0);        
      }

      TempBuffer<uint8_t> secretCurveKey(32);                // Accessory's secret key for Curve25519 encryption (32 bytes).  Ephemeral usage - created below and used only in this block
      crypto_box_keypair(publicCurveKey,secretCurveKey);     // generate Curve25519 public key pair (will persist until end of verification process)

      memcpy(iosCurveKey,*itPublicKey,32);                   // save iosCurveKey (will persist until end of verification process)

      crypto_scalarmult_curve25519(sharedCurveKey,secretCurveKey,iosCurveKey);      // generate (and persist) Pair Verify SharedSecret CurveKey from Accessory's Curve25519 secret key and Controller's Curve25519 public key

      // concatenate Accessory's Curve25519 Public Key, Accessory's Pairing ID, and Controller's Curve25519 Public Key into accessoryInfo
      
      TempBuffer<uint8_t> accessoryInfo(sizeof(publicCurveKey)+sizeof(accessory.ID)+sizeof(iosCurveKey));
      Utils::memcat(accessoryInfo,3,publicCurveKey,sizeof(publicCurveKey),accessory.ID,sizeof(accessory.ID),iosCurveKey,sizeof(iosCurveKey));

      auto itSignature=subTLV.add(kTLVType_Signature,crypto_sign_BYTES,NULL);                    // create blank Signature TLV

      crypto_sign_detached(*itSignature,NULL,accessoryInfo,accessoryInfo.len(),accessory.LTSK);  // produce Signature of accessoryInfo using AccessoryLTSK (Ed25519 long-term secret key)

      subTLV.add(kTLVType_Identifier,sizeof(accessory.ID),accessory.ID);                         // set Identifier TLV record as accessoryPairingID

      LOG2("------- ENCRYPTING SUB-TLVS -------\n");

      subTLV.print();

      TempBuffer<uint8_t> subPack(subTLV.pack_size());   // create sub-TLV by packing Identifier and Signature TLV records together
      subTLV.pack(subPack);                                

      // create SessionKey from Curve25519 SharedSecret using HKDF-SHA-512, then encrypt subTLV data with SessionKey using ChaCha20-Poly1305.  Output stored in EncryptedData TLV
    
      hkdf.create(sessionKey,sharedCurveKey,32,"Pair-Verify-Encrypt-Salt","Pair-Verify-Encrypt-Info");                              // create SessionKey (32 bytes)

      auto itEncryptedData=responseTLV.add(kTLVType_EncryptedData,subPack.len()+crypto_aead_chacha20poly1305_IETF_ABYTES,NULL);     //create blank EncryptedData TLV with space for subTLV + Authentication Tag

      crypto_aead_chacha20poly1305_ietf_encrypt(*itEncryptedData,NULL,subPack,subPack.len(),NULL,0,NULL,(unsigned char *)"\x00\x00\x00\x00PV-Msg02",sessionKey);
                                            
      LOG2("---------- END SUB-TLVS! ----------\n");
      
      responseTLV.add(kTLVType_State,pairState_M2);                 // set State=<M2>
      responseTLV.add(kTLVType_PublicKey,32,publicCurveKey);        // set PublicKey to Accessory's Curve25519 public key
    
      tlvRespond(responseTLV);                                      // send response to client  
    }
    break;  
   
    case pairState_M3:{                     // 'Verify Finish Request'

      auto itEncryptedData=iosTLV.find(kTLVType_EncryptedData);

      if(iosTLV.len(itEncryptedData)<=0){            
        LOG0("\n*** ERROR: Required 'EncryptedData' TLV record for this step is bad or missing\n\n");
        responseTLV.add(kTLVType_State,pairState_M4);               // set State=<M4>
        responseTLV.add(kTLVType_Error,tagError_Unknown);           // set Error=Unknown (there is no specific error type for missing/bad TLV data)
        tlvRespond(responseTLV);                                    // send response to client
        return(0);
      };

      LOG2("------- DECRYPTING SUB-TLVS -------\n");

      // use SessionKey to decrypt encrypytedData TLV with padded nonce="PV-Msg03"

      TempBuffer<uint8_t> decrypted((*itEncryptedData).len-crypto_aead_chacha20poly1305_IETF_ABYTES);        // temporary storage for decrypted data
      
      if(crypto_aead_chacha20poly1305_ietf_decrypt(decrypted, NULL, NULL, *itEncryptedData, (*itEncryptedData).len, NULL, 0, (unsigned char *)"\x00\x00\x00\x00PV-Msg03", sessionKey)==-1){          
        LOG0("\n*** ERROR: Verify Authentication Failed\n\n");
        responseTLV.add(kTLVType_State,pairState_M4);               // set State=<M4>
        responseTLV.add(kTLVType_Error,tagError_Authentication);    // set Error=Authentication
        tlvRespond(responseTLV);                                    // send response to client
        return(0);        
      }

      subTLV.unpack(decrypted,decrypted.len());                     // unpack TLV      
      subTLV.print();                                               // print decrypted TLV data
      
      LOG2("---------- END SUB-TLVS! ----------\n");

      auto itIdentifier=subTLV.find(kTLVType_Identifier);
      auto itSignature=subTLV.find(kTLVType_Signature);

      if(subTLV.len(itIdentifier)!=36 || subTLV.len(itSignature)!=crypto_sign_BYTES){ 
        LOG0("\n*** ERROR: One or more of required 'Identifier,' and 'Signature' TLV records for this step is bad or missing\n\n");
        responseTLV.add(kTLVType_State,pairState_M4);               // set State=<M4>
        responseTLV.add(kTLVType_Error,tagError_Unknown);           // set Error=Unknown (there is no specific error type for missing/bad TLV data)
        tlvRespond(responseTLV);                                    // send response to client
        return(0);
      }

      Controller *tPair;                                            // temporary pointer to Controller
      
      if(!(tPair=findController(*itIdentifier))){
        LOG0("\n*** ERROR: Unrecognized Controller ID: ");
        charPrintRow(*itIdentifier,36,2);
        LOG0("\n\n");
        responseTLV.add(kTLVType_State,pairState_M4);               // set State=<M4>
        responseTLV.add(kTLVType_Error,tagError_Authentication);    // set Error=Authentication
        tlvRespond(responseTLV);                                    // send response to client
        return(0);
      }

      LOG2("\n*** Verifying session with Controller ID: ");
      charPrintRow(tPair->ID,sizeof(tPair->ID),2);
      LOG2("...\n");

      TempBuffer<uint8_t> iosDeviceInfo(sizeof(iosCurveKey)+sizeof(tPair->ID)+sizeof(publicCurveKey));
      Utils::memcat(iosDeviceInfo,3,iosCurveKey,sizeof(iosCurveKey),tPair->ID,sizeof(tPair->ID),publicCurveKey,sizeof(publicCurveKey));
      
      if(crypto_sign_verify_detached(*itSignature, iosDeviceInfo, iosDeviceInfo.len(), tPair->LTPK) != 0){         // verify signature of iosDeviceInfo using iosDeviceLTPK   
        LOG0("\n*** ERROR: LPTK Signature Verification Failed\n\n");
        responseTLV.add(kTLVType_State,pairState_M4);               // set State=<M4>
        responseTLV.add(kTLVType_Error,tagError_Authentication);    // set Error=Authentication
        tlvRespond(responseTLV);                                    // send response to client
        return(0);                
      }

      responseTLV.add(kTLVType_State,pairState_M4);                 // set State=<M4>
      tlvRespond(responseTLV);                                      // send response to client (unencrypted since cPair=NULL)

      cPair=tPair;        // save Controller for this connection slot - connection is now verified and should be encrypted going forward

      hkdf.create(a2cKey,sharedCurveKey,32,"Control-Salt","Control-Read-Encryption-Key");        // create AccessoryToControllerKey (HAP Section 6.5.2)
      hkdf.create(c2aKey,sharedCurveKey,32,"Control-Salt","Control-Write-Encryption-Key");       // create ControllerToAccessoryKey (HAP Section 6.5.2)
      
      a2cNonce.zero();         // reset Nonces for this session to zero
      c2aNonce.zero();

      LOG2("\n*** SESSION VERIFICATION COMPLETE *** \n");
    }
    break;
  
  } // switch

  return(1);
  
} // postPairVerify

//////////////////////////////////////

int HAPClient::getAccessoriesURL(){

  if(!cPair){                       // unverified, unencrypted session
    unauthorizedError();
    return(0);
  }

  LOG1("In Get Accessories #");
  LOG1(conNum);
  LOG1(" (");
  LOG1(client.remoteIP());
  LOG1(")...\n");

  int nBytes = homeSpan.sprintfAttributes(NULL);        // get size of HAP attributes JSON
  TempBuffer<char> jBuf(nBytes+1);
  homeSpan.sprintfAttributes(jBuf);                  // create JSON database (will need to re-cast to uint8_t* below)

  char *body;
  asprintf(&body,"HTTP/1.1 200 OK\r\nContent-Type: application/hap+json\r\nContent-Length: %d\r\n\r\n",nBytes);
  
  LOG2("\n>>>>>>>>>> ");
  LOG2(client.remoteIP());
  LOG2(" >>>>>>>>>>\n");
  LOG2(body);
  LOG2(jBuf.get());
  LOG2("\n");
  
  sendEncrypted(body,(uint8_t *)jBuf.get(),nBytes);
  free(body);
         
  return(1);
  
} // getAccessories

//////////////////////////////////////

int HAPClient::postPairingsURL(uint8_t *content, size_t len){

  if(!cPair){                       // unverified, unencrypted session
    unauthorizedError();
    return(0);
  }

  HAPTLV iosTLV;
  HAPTLV responseTLV;

  iosTLV.unpack(content,len);
  iosTLV.print();
  LOG2("------------ END TLVS! ------------\n");

  LOG2("In Post Pairings #%d (%s)...",conNum,client.remoteIP().toString().c_str());
  
  auto itState=iosTLV.find(kTLVType_State);
  auto itMethod=iosTLV.find(kTLVType_Method);
    
  if(iosTLV.len(itState)!=1 || (*itState)[0]!=1){               // missing STATE TLV
    LOG0("\n*** ERROR: Parirings 'State' is either missing or not set to <M1>\n\n");
    badRequestError();                                          // return with 400 error, which closes connection      
    return(0);
  }

  if(iosTLV.len(itMethod)!=1){                                  // missing METHOD TLV
    LOG0("\n*** ERROR: Missing or invalid 'Method' TLV\n\n");
    badRequestError();                                          // return with 400 error, which closes connection      
    return(0);
  }

  int tlvMethod=(*itMethod)[0];

  responseTLV.add(kTLVType_State,pairState_M2);                 // all responses include State=M2
  
  switch(tlvMethod){                        // List-Pairings received -- process request!  (HAP Sections 5.10-5.12)

    case 3: {
      LOG1("Add...\n");

      auto itIdentifier=iosTLV.find(kTLVType_Identifier);
      auto itPublicKey=iosTLV.find(kTLVType_PublicKey);
      auto itPermissions=iosTLV.find(kTLVType_Permissions);
      
      if(iosTLV.len(itIdentifier)!=hap_controller_IDBYTES || iosTLV.len(itPublicKey)!=crypto_sign_PUBLICKEYBYTES || iosTLV.len(itPermissions)!=1){            
        LOG0("\n*** ERROR: One or more of required 'Identifier,' 'PublicKey,' and 'Permissions' TLV records for this step is bad or missing\n\n");
        responseTLV.add(kTLVType_Error,tagError_Unknown);
        tlvRespond(responseTLV);
        return(0);
      }
      
      if(!cPair->admin){
        LOG0("\n*** ERROR: Controller making request does not have admin privileges to add/update other Controllers\n\n");
        responseTLV.add(kTLVType_Error,tagError_Authentication);
        tlvRespond(responseTLV);
        return(0);
      } 
             
      tagError err=addController(*itIdentifier,*itPublicKey,(*itPermissions)[0]);
      if(err!=tagError_None)
        responseTLV.add(kTLVType_Error,err);
      
      tlvRespond(responseTLV);
      return(1);
    }
    break;

    case 4: {
      LOG1("Remove...\n");

      auto itIdentifier=iosTLV.find(kTLVType_Identifier);

      if(iosTLV.len(itIdentifier)!=hap_controller_IDBYTES){            
        LOG0("\n*** ERROR: Required 'Identifier' TLV record for this step is bad or missing\n\n");
        responseTLV.add(kTLVType_Error,tagError_Unknown);
        tlvRespond(responseTLV);
        return(0);
      }
      
      if(!cPair->admin){
        LOG0("\n*** ERROR: Controller making request does not have admin privileges to remove Controllers\n\n");
        responseTLV.add(kTLVType_Error,tagError_Authentication);
        tlvRespond(responseTLV);
        return(0);
      }
      
      tlvRespond(responseTLV);           // must send response before removing Controller     
      removeController(*itIdentifier);
      
      return(1);
    } 
    break;
      
    case 5: {
      LOG1("List...\n");

      if(!cPair->admin){
        LOG0("\n*** ERROR: Controller making request does not have admin privileges to remove Controllers\n\n");
        responseTLV.add(kTLVType_Error,tagError_Authentication);
        tlvRespond(responseTLV);
        return(0);
      }      

      boolean addSeparator=false;
      
      for(auto it=controllerList.begin();it!=controllerList.end();it++){
        if((*it).allocated){
          if(addSeparator)         
            responseTLV.add(kTLVType_Separator);                                        
          responseTLV.add(kTLVType_Permissions,(*it).admin);      
          responseTLV.add(kTLVType_Identifier,hap_controller_IDBYTES,(*it).ID);
          responseTLV.add(kTLVType_PublicKey,crypto_sign_PUBLICKEYBYTES,(*it).LTPK);
          addSeparator=true;
        }
      }

      tlvRespond(responseTLV);
      return(1);
    }
    break;

    default: {
      LOG0("\n*** ERROR: Undefined List-Pairings Method: %d.  Must be 3, 4, or 5\n\n",tlvMethod);
      badRequestError();                                    // return with 400 error, which closes connection      
      return(0);
    }
  } // switch
  
  return(1);
}

//////////////////////////////////////

int HAPClient::getCharacteristicsURL(char *urlBuf){


  if(!cPair){                       // unverified, unencrypted session
    unauthorizedError();
    return(0);
  }

  LOG1("In Get Characteristics #");
  LOG1(conNum);
  LOG1(" (");
  LOG1(client.remoteIP());
  LOG1(")...\n");

  int len=strlen(urlBuf);       // determine number of IDs specificed by counting commas in URL
  int numIDs=1;
  for(int i=0;i<len;i++)
    if(urlBuf[i]==',')
      numIDs++;
  
  TempBuffer<char *> ids(numIDs);  // reserve space for number of IDs found
  int flags=GET_VALUE|GET_AID;      // flags indicating which characteristic fields to include in response (HAP Table 6-13)
  numIDs=0;                         // reset number of IDs found

  char *lastSpace=strchr(urlBuf,' ');
  if(lastSpace)
    lastSpace[0]='\0';
    
  char *p1;
  while(char *t1=strtok_r(urlBuf,"&",&p1)){      // parse request into major tokens
    urlBuf=NULL;

    if(!strcmp(t1,"meta=1")){
      flags|=GET_META;
    } else 
    if(!strcmp(t1,"perms=1")){
      flags|=GET_PERMS;
    } else 
    if(!strcmp(t1,"type=1")){
      flags|=GET_TYPE;
    } else 
    if(!strcmp(t1,"ev=1")){
      flags|=GET_EV;
    } else
    if(!strncmp(t1,"id=",3)){   
      t1+=3;
      char *p2;
      while(char *t2=strtok_r(t1,",",&p2)){      // parse IDs
        t1=NULL;
        ids[numIDs++]=t2;
      }
    }
  } // parse URL

  if(!numIDs)           // could not find any IDs
    return(0);

  int nBytes=homeSpan.sprintfAttributes(ids,numIDs,flags,NULL);          // get JSON response - includes terminating null (will be recast to uint8_t* below)
  TempBuffer<char> jsonBuf(nBytes+1);
  homeSpan.sprintfAttributes(ids,numIDs,flags,jsonBuf);

  boolean sFlag=strstr(jsonBuf,"status");          // status attribute found?

  char *body;
  asprintf(&body,"HTTP/1.1 %s\r\nContent-Type: application/hap+json\r\nContent-Length: %d\r\n\r\n",!sFlag?"200 OK":"207 Multi-Status",nBytes);
    
  LOG2("\n>>>>>>>>>> ");
  LOG2(client.remoteIP());
  LOG2(" >>>>>>>>>>\n");    
  LOG2(body);
  LOG2(jsonBuf.get());
  LOG2("\n");
  
  sendEncrypted(body,(uint8_t *)jsonBuf.get(),nBytes);        // note recasting of jsonBuf into uint8_t*
  free(body);
        
  return(1);
}

//////////////////////////////////////

int HAPClient::putCharacteristicsURL(char *json){

  if(!cPair){                       // unverified, unencrypted session
    unauthorizedError();
    return(0);
  }

  LOG1("In Put Characteristics #");
  LOG1(conNum);
  LOG1(" (");
  LOG1(client.remoteIP());
  LOG1(")...\n");

  int n=homeSpan.countCharacteristics(json);    // count number of objects in JSON request
  if(n==0)                                      // if no objects found, return
    return(0);
 
  SpanBuf pObj[n];                                        // reserve space for objects
  if(!homeSpan.updateCharacteristics(json, pObj))         // perform update
    return(0);                                            // return if failed to update (error message will have been printed in update)

  int multiCast=0;                                        // check if all status is OK, or if multicast response is request
  for(int i=0;i<n;i++)
    if(pObj[i].status!=StatusCode::OK)
      multiCast=1;    

  if(!multiCast){                                         // JSON object has no content
    
    char body[]="HTTP/1.1 204 No Content\r\n\r\n";
    
    LOG2("\n>>>>>>>>>> ");
    LOG2(client.remoteIP());
    LOG2(" >>>>>>>>>>\n");
    LOG2(body);  

    sendEncrypted(body,NULL,0);  
        
  } else {                                                       // multicast respose is required

    int nBytes=homeSpan.sprintfAttributes(pObj,n,NULL);          // get JSON response - includes terminating null (will be recast to uint8_t* below)
    TempBuffer<char> jsonBuf(nBytes+1);
    homeSpan.sprintfAttributes(pObj,n,jsonBuf);

    char *body;
    asprintf(&body,"HTTP/1.1 207 Multi-Status\r\nContent-Type: application/hap+json\r\nContent-Length: %d\r\n\r\n",nBytes);
  
    LOG2("\n>>>>>>>>>> ");
    LOG2(client.remoteIP());
    LOG2(" >>>>>>>>>>\n");    
    LOG2(body);
    LOG2(jsonBuf.get());
    LOG2("\n");
  
    sendEncrypted(body,(uint8_t *)jsonBuf.get(),nBytes);        // note recasting of jsonBuf into uint8_t*
    free(body);
  
  }

  // Create and send Event Notifications if needed

  eventNotify(pObj,n,HAPClient::conNum);                  // transmit EVENT Notification for "n" pObj objects, except DO NOT notify client making request
    
  return(1);
}

//////////////////////////////////////

int HAPClient::putPrepareURL(char *json){

  if(!cPair){                       // unverified, unencrypted session
    unauthorizedError();
    return(0);
  }

  LOG1("In Put Prepare #");
  LOG1(conNum);
  LOG1(" (");
  LOG1(client.remoteIP());
  LOG1(")...\n");

  char ttlToken[]="\"ttl\":";
  char pidToken[]="\"pid\":";
  
  char *cBuf;
  uint32_t ttl;
  uint64_t pid;
   
  if((cBuf=strstr(json,ttlToken)))
    sscanf(cBuf+strlen(ttlToken),"%u",&ttl);

  if((cBuf=strstr(json,pidToken)))
    sscanf(cBuf+strlen(ttlToken),"%llu",&pid);

  char jsonBuf[32];
  StatusCode status=StatusCode::OK;

  if(ttl>0 && pid>0){                           // found required elements
    homeSpan.TimedWrites[pid]=ttl+millis();     // store this pid/alarmTime combination 
  } else {                                      // problems parsing request
    status=StatusCode::InvalidValue;
  }

  sprintf(jsonBuf,"{\"status\":%d}",(int)status);
  int nBytes=strlen(jsonBuf);
  char *body;
  asprintf(&body,"HTTP/1.1 200 OK\r\nContent-Type: application/hap+json\r\nContent-Length: %d\r\n\r\n",nBytes);
  
  LOG2("\n>>>>>>>>>> ");
  LOG2(client.remoteIP());
  LOG2(" >>>>>>>>>>\n");    
  LOG2(body);
  LOG2(jsonBuf);
  LOG2("\n");
  
  sendEncrypted(body,(uint8_t *)jsonBuf,nBytes);        // note recasting of jsonBuf into uint8_t*
  free(body);
    
  return(1);
}

//////////////////////////////////////

int HAPClient::getStatusURL(){

  char clocktime[33];

  if(homeSpan.webLog.timeInit){
    struct tm timeinfo;
    getLocalTime(&timeinfo,10);
    strftime(clocktime,sizeof(clocktime),"%c",&timeinfo);
  } else {
    sprintf(clocktime,"Unknown");        
  }

  char uptime[32];
  int seconds=esp_timer_get_time()/1e6;
  int secs=seconds%60;
  int mins=(seconds/=60)%60;
  int hours=(seconds/=60)%24;
  int days=(seconds/=24);
    
  sprintf(uptime,"%d:%02d:%02d:%02d",days,hours,mins,secs);

  String response="HTTP/1.1 200 OK\r\nContent-type: text/html; charset=utf-8\r\n\r\n";

  response+="<html><head><title>" + String(homeSpan.displayName) + "</title>\n";
  response+="<style>body {background-color:lightblue;} th, td {padding-right: 10px; padding-left: 10px; border:1px solid black;}" + homeSpan.webLog.css + "</style></head>\n";
  response+="<body class=bod1><h2>" + String(homeSpan.displayName) + "</h2>\n";
  
  response+="<table class=tab1>\n";
  response+="<tr><td>Up Time:</td><td>" + String(uptime) + "</td></tr>\n";
  response+="<tr><td>Current Time:</td><td>" + String(clocktime) + "</td></tr>\n";
  response+="<tr><td>Boot Time:</td><td>" + String(homeSpan.webLog.bootTime) + "</td></tr>\n";
  
  response+="<tr><td>Reset Reason:</td><td>";
  switch(esp_reset_reason()) {
    case ESP_RST_UNKNOWN:
      response += "Cannot be determined";
      break;
    case ESP_RST_POWERON:
      response += "Power-on event";
      break;
    case ESP_RST_EXT:
      response += "External pin";
      break;
    case ESP_RST_SW:
      response += "Software reboot via esp_restart";
      break;
    case ESP_RST_PANIC:
      response += "Software Exception/Panic";
      break;
    case ESP_RST_INT_WDT:
      response += "Interrupt watchdog";
      break;
    case ESP_RST_TASK_WDT:
      response += "Task watchdog";
      break;
    case ESP_RST_WDT:
      response += "Other watchdogs";
      break;
    case ESP_RST_DEEPSLEEP:
      response += "Exiting deep sleep mode";
      break;
    case ESP_RST_BROWNOUT:
      response += "Brownout";
      break;
    case ESP_RST_SDIO:
      response += "SDIO";
      break;
    default:
      response += "Unknown Reset Code";
  }
  response+=" (" + String(esp_reset_reason()) + ")</td></tr>\n";
  
  response+="<tr><td>WiFi Disconnects:</td><td>" + String(homeSpan.connected/2) + "</td></tr>\n";
  response+="<tr><td>WiFi Signal:</td><td>" + String(WiFi.RSSI()) + " dBm</td></tr>\n";
  response+="<tr><td>WiFi Gateway:</td><td>" + WiFi.gatewayIP().toString() + "</td></tr>\n";
  response+="<tr><td>ESP32 Board:</td><td>" + String(ARDUINO_BOARD) + "</td></tr>\n";
  response+="<tr><td>Arduino-ESP Version:</td><td>" + String(ARDUINO_ESP_VERSION) + "</td></tr>\n";
  response+="<tr><td>ESP-IDF Version:</td><td>" + String(ESP_IDF_VERSION_MAJOR) + "." + String(ESP_IDF_VERSION_MINOR) + "." + String(ESP_IDF_VERSION_PATCH) + "</td></tr>\n";
  response+="<tr><td>HomeSpan Version:</td><td>" + String(HOMESPAN_VERSION) + "</td></tr>\n";
  response+="<tr><td>Sketch Version:</td><td>" + String(homeSpan.getSketchVersion()) + "</td></tr>\n"; 
  response+="<tr><td>Sodium Version:</td><td>" + String(sodium_version_string()) + " Lib " + String(sodium_library_version_major()) + "." + String(sodium_library_version_minor()) +"</td></tr>\n"; 

  char mbtlsv[64];
  mbedtls_version_get_string_full(mbtlsv);
  response+="<tr><td>MbedTLS Version:</td><td>" + String(mbtlsv) + "</td></tr>\n";
  
  response+="<tr><td>HomeKit Status:</td><td>" + String(HAPClient::nAdminControllers()?"PAIRED":"NOT PAIRED") + "</td></tr>\n";   
  response+="<tr><td>Max Log Entries:</td><td>" + String(homeSpan.webLog.maxEntries) + "</td></tr>\n"; 

  if(homeSpan.weblogCallback)
    homeSpan.weblogCallback(response);

  response+="</table>\n";
  response+="<p></p>";

  LOG2("\n>>>>>>>>>> ");
  LOG2(client.remoteIP());
  LOG2(" >>>>>>>>>>\n");
  
  if(homeSpan.webLog.maxEntries>0){
    response+="<table class=tab2><tr><th>Entry</th><th>Up Time</th><th>Log Time</th><th>Client</th><th>Message</th></tr>\n";
    int lastIndex=homeSpan.webLog.nEntries-homeSpan.webLog.maxEntries;
    if(lastIndex<0)
      lastIndex=0;
    
    for(int i=homeSpan.webLog.nEntries-1;i>=lastIndex;i--){
      int index=i%homeSpan.webLog.maxEntries;
      seconds=homeSpan.webLog.log[index].upTime/1e6;
      secs=seconds%60;
      mins=(seconds/=60)%60;
      hours=(seconds/=60)%24;
      days=(seconds/=24);   
      sprintf(uptime,"%d:%02d:%02d:%02d",days,hours,mins,secs);

      if(homeSpan.webLog.log[index].clockTime.tm_year>0)
        strftime(clocktime,sizeof(clocktime),"%c",&homeSpan.webLog.log[index].clockTime);
      else
        sprintf(clocktime,"Unknown");        
      
      response+="<tr><td>" + String(i+1) + "</td><td>" + String(uptime) + "</td><td>" + String(clocktime) + "</td><td>" + homeSpan.webLog.log[index].clientIP + "</td><td>" + String(homeSpan.webLog.log[index].message) + "</td></tr>\n";

      if(response.length()>1024){     // if response grows too big, transmit chunk and reset
        LOG2(response);
        client.print(response);
        delay(1);                     // slight pause seems to be required
        response.clear();
      }
    }
    response+="</table>\n";
  }
  
  response+="</body></html>";

  LOG2(response);
  LOG2("\n");
  client.print(response);
  LOG2("------------ SENT! --------------\n");
  
  delay(1);
  client.stop();
  
  return(1);
}

//////////////////////////////////////

void HAPClient::checkNotifications(){

  if(!homeSpan.Notifications.empty()){                                          // if there are Notifications to process    
    eventNotify(&homeSpan.Notifications[0],homeSpan.Notifications.size());      // transmit EVENT Notifications
    homeSpan.Notifications.clear();                                             // clear Notifications vector
  }
}

//////////////////////////////////////

void HAPClient::checkTimedWrites(){

  unsigned long cTime=millis();                                       // get current time

  char c[64];

  auto tw=homeSpan.TimedWrites.begin();
  while(tw!=homeSpan.TimedWrites.end()){
    if(cTime>tw->second){                                                               // timer has expired
       sprintf(c,"Removing PID=%llu  ALARM=%u\n",tw->first,tw->second);
       LOG2(c);
       tw=homeSpan.TimedWrites.erase(tw);
      }
    else
      tw++; 
  }
 
}

//////////////////////////////////////

void HAPClient::eventNotify(SpanBuf *pObj, int nObj, int ignoreClient){
  
  for(int cNum=0;cNum<homeSpan.maxConnections;cNum++){        // loop over all connection slots
    if(hap[cNum]->client && cNum!=ignoreClient){       // if there is a client connected to this slot and it is NOT flagged to be ignored (in cases where it is the client making a PUT request)

      int nBytes=homeSpan.sprintfNotify(pObj,nObj,NULL,cNum);          // get JSON response for notifications to client cNum - includes terminating null (will be recast to uint8_t* below)

      if(nBytes>0){                                                    // if there are notifications to send to client cNum
        TempBuffer<char> jsonBuf(nBytes+1);
        homeSpan.sprintfNotify(pObj,nObj,jsonBuf,cNum);

        char *body;
        asprintf(&body,"EVENT/1.0 200 OK\r\nContent-Type: application/hap+json\r\nContent-Length: %d\r\n\r\n",nBytes);

        LOG2("\n>>>>>>>>>> ");
        LOG2(hap[cNum]->client.remoteIP());
        LOG2(" >>>>>>>>>>\n");    
        LOG2(body);
        LOG2(jsonBuf.get());
        LOG2("\n");
  
        hap[cNum]->sendEncrypted(body,(uint8_t *)jsonBuf.get(),nBytes);        // note recasting of jsonBuf into uint8_t*
        free(body);

      } // if there are characteristic updates to notify client cNum
    } // if client exists
  }

}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

void HAPClient::tlvRespond(TLV8 &tlv8){

  TempBuffer<uint8_t> tBuf(tlv8.pack_size());    // create buffer to hold TLV data    
  tlv8.pack(tBuf);                               // pack TLV records into buffer

  char *body;
  asprintf(&body,"HTTP/1.1 200 OK\r\nContent-Type: application/pairing+tlv8\r\nContent-Length: %d\r\n\r\n",tBuf.len());      // create Body with Content Length = size of TLV data
  
  LOG2("\n>>>>>>>>>> %s >>>>>>>>>>\n",client.remoteIP().toString().c_str());
  LOG2(body);
  tlv8.print();

  if(!cPair){                       // unverified, unencrypted session
    client.print(body);
    client.write(tBuf,tBuf.len());      
    LOG2("------------ SENT! --------------\n");
  } else {
    sendEncrypted(body,tBuf,tBuf.len());
  }

  free(body);

} // tlvRespond

//////////////////////////////////////

//void HAPClient::tlvRespond(){
//
//  TempBuffer<uint8_t> tBuf(tlv8.pack(NULL));    // create buffer to hold TLV data    
//  tlv8.pack(tBuf);                                   // pack TLV records into buffer
//
//  char *body;
//  asprintf(&body,"HTTP/1.1 200 OK\r\nContent-Type: application/pairing+tlv8\r\nContent-Length: %d\r\n\r\n",tBuf.len());      // create Body with Content Length = size of TLV data
//  
//  LOG2("\n>>>>>>>>>> ");
//  LOG2(client.remoteIP());
//  LOG2(" >>>>>>>>>>\n");
//  LOG2(body);
//  tlv8.print(2);
//
//  if(!cPair){                       // unverified, unencrypted session
//    client.print(body);
//    client.write(tBuf,tBuf.len());      
//    LOG2("------------ SENT! --------------\n");
//  } else {
//    sendEncrypted(body,tBuf,tBuf.len());
//  }
//
//  free(body);
//
//} // tlvRespond

//////////////////////////////////////

int HAPClient::receiveEncrypted(uint8_t *httpBuf, int messageSize){

  uint8_t aad[2];
  int nBytes=0;

  while(client.read(aad,2)==2){    // read initial 2-byte AAD record

    int n=aad[0]+aad[1]*256;                // compute number of bytes expected in message after decoding

    if(nBytes+n>messageSize){      // exceeded maximum number of bytes allowed in plaintext message
      LOG0("\n\n*** ERROR:  Decrypted message of %d bytes exceeded maximum expected message length of %d bytes\n\n",nBytes+n,messageSize);
      return(0);
      }

    TempBuffer<uint8_t> tBuf(n+16);      // expected number of total bytes = n bytes in encoded message + 16 bytes for appended authentication tag      

    if(client.read(tBuf,tBuf.len())!=tBuf.len()){      
      LOG0("\n\n*** ERROR: Malformed encrypted message frame\n\n");
      return(0);      
    }                

    if(crypto_aead_chacha20poly1305_ietf_decrypt(httpBuf+nBytes, NULL, NULL, tBuf, tBuf.len(), aad, 2, c2aNonce.get(), c2aKey)==-1){
      LOG0("\n\n*** ERROR: Can't Decrypt Message\n\n");
      return(0);        
    }

    c2aNonce.inc();

    nBytes+=n;          // increment total number of bytes in plaintext message
    
  } // while

  return(nBytes);
    
} // receiveEncrypted

//////////////////////////////////////

void HAPClient::sendEncrypted(char *body, uint8_t *dataBuf, int dataLen){

  const int FRAME_SIZE=1024;          // number of bytes to use in each ChaCha20-Poly1305 encrypted frame when sending encrypted JSON content to Client
  
  int bodyLen=strlen(body);

  unsigned long long nBytes;

  int maxFrameSize=bodyLen>dataLen?bodyLen:dataLen;       // set maxFrameSize to greater of bodyLen or dataLen
  if(maxFrameSize>FRAME_SIZE)                             // cap maxFrameSize by FRAME_SIZE (HAP restriction)
    maxFrameSize=FRAME_SIZE;

  TempBuffer<uint8_t> tBuf(2+maxFrameSize+16);           // 2-byte AAD + encrypted data + 16-byte authentication tag
  
  tBuf[0]=bodyLen%256;         // store number of bytes in first frame that encrypts the Body (AAD bytes)
  tBuf[1]=bodyLen/256;
  
  crypto_aead_chacha20poly1305_ietf_encrypt(tBuf+2,&nBytes,(uint8_t *)body,bodyLen,tBuf,2,NULL,a2cNonce.get(),a2cKey);   // encrypt the Body with authentication tag appended

  client.write(tBuf,nBytes+2);   // transmit encrypted frame
  a2cNonce.inc();                      // increment nonce
  
  for(int i=0;i<dataLen;i+=FRAME_SIZE){      // encrypt FRAME_SIZE number of bytes in dataBuf in sequential frames
    
    int n=dataLen-i;           // number of bytes remaining
    
    if(n>FRAME_SIZE)           // maximum number of bytes to encrypt=FRAME_SIZE
      n=FRAME_SIZE;                                     
    
    tBuf[0]=n%256;    // store number of bytes that encrypts this frame (AAD bytes)
    tBuf[1]=n/256;

    crypto_aead_chacha20poly1305_ietf_encrypt(tBuf+2,&nBytes,dataBuf+i,n,tBuf,2,NULL,a2cNonce.get(),a2cKey);   // encrypt the next portion of dataBuf with authentication tag appended

    client.write(tBuf,nBytes+2);   // transmit encrypted frame
    a2cNonce.inc();                      // increment nonce

  }
 
  LOG2("-------- SENT ENCRYPTED! --------\n");
      
} // sendEncrypted

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

void HAPClient::hexPrintColumn(uint8_t *buf, int n, int minLogLevel){

  if(homeSpan.logLevel<minLogLevel)
    return;
  
  for(int i=0;i<n;i++)
    Serial.printf("%d) %02X\n",i,buf[i]);
}

//////////////////////////////////////

void HAPClient::hexPrintRow(uint8_t *buf, int n, int minLogLevel){

  if(homeSpan.logLevel<minLogLevel)
    return;

  for(int i=0;i<n;i++)
    Serial.printf("%02X",buf[i]);
}

//////////////////////////////////////

void HAPClient::charPrintRow(uint8_t *buf, int n, int minLogLevel){

  if(homeSpan.logLevel<minLogLevel)
    return;
  
  for(int i=0;i<n;i++)
    Serial.printf("%c",buf[i]);
}

//////////////////////////////////////

Controller *HAPClient::findController(uint8_t *id){

  for(auto it=controllerList.begin();it!=controllerList.end();it++){
    if(!memcmp((*it).ID,id,36))
      return(&*it);
  }

  return(NULL);       // no match
}

//////////////////////////////////////

int HAPClient::nAdminControllers(){

  int n=0;
  for(auto it=controllerList.begin();it!=controllerList.end();it++)
    n+=((*it).admin);
  return(n);
}

//////////////////////////////////////

tagError HAPClient::addController(uint8_t *id, uint8_t *ltpk, boolean admin){

  Controller *cTemp=findController(id);

  tagError err=tagError_None;
  
  if(!cTemp){                                            // new controller    
    if(controllerList.size()<MAX_CONTROLLERS){
      controllerList.emplace_back(id,ltpk,admin);        // create and store data
      LOG2("\n*** Added Controller: ");
      charPrintRow(id,36,2);
      LOG2(admin?" (admin)\n\n":" (regular)\n\n");
      saveControllers();
    } else {
      LOG0("\n*** ERROR: Can't pair more than %d Controllers\n\n",MAX_CONTROLLERS);
      err=tagError_MaxPeers;
    }    
  } else if(!memcmp(ltpk,cTemp->LTPK,sizeof(cTemp->LTPK))){   // existing controller with same LTPK
    LOG2("\n*** Updated Controller: ");
    charPrintRow(id,36,2);
    LOG2(" from %s to %s\n\n",cTemp->admin?"(admin)":"(regular)",admin?"(admin)":"(regular)");
    cTemp->admin=admin;
    saveControllers();    
  } else {
    LOG0("\n*** ERROR: Invalid request to update the LTPK of an existing Controller\n\n");
    err=tagError_Unknown;    
  }

  return(err);
}          

//////////////////////////////////////

void HAPClient::removeController(uint8_t *id){

  auto it=std::find_if(controllerList.begin(), controllerList.end(), [id](const Controller& cTemp){return(!memcmp(cTemp.ID,id,sizeof(cTemp.ID)));});

  if(it==controllerList.end()){
    LOG2("\n*** Request to Remove Controller Ignored - Controller Not Found: ");
    charPrintRow(id,36,2);
    LOG2("\n");
    return;
  }

  LOG1("\n*** Removing Controller: ");
  charPrintRow((*it).ID,36,2);
  LOG1((*it).admin?" (admin)\n":" (regular)\n");
  
  tearDown((*it).ID);         // teardown any connections using this Controller
  controllerList.erase(it);   // remove Controller

  if(!nAdminControllers()){   // no more admin Controllers
    
    LOG1("That was last Admin Controller!  Removing any remaining Regular Controllers and unpairing Accessory\n");    
    
    tearDown(NULL);                                              // teardown all remaining connections
    controllerList.clear();                                      // remove all remaining Controllers
    mdns_service_txt_item_set("_hap","_tcp","sf","1");           // set Status Flag = 1 (Table 6-8)
    STATUS_UPDATE(start(LED_PAIRING_NEEDED),HS_PAIRING_NEEDED)   // set optional Status LED
    if(homeSpan.pairCallback)                                    // if set, invoke user-defined Pairing Callback to indicate device has been un-paired
      homeSpan.pairCallback(false);    
  }

  saveControllers();
}

//////////////////////////////////////

void HAPClient::tearDown(uint8_t *id){
  
  for(int i=0;i<homeSpan.maxConnections;i++){     // loop over all connection slots
    if(hap[i]->client && (id==NULL || (hap[i]->cPair && !memcmp(id,hap[i]->cPair->ID,36)))){
      LOG1("*** Terminating Client #%d\n",i);
      hap[i]->client.stop();
    }
  }
}

//////////////////////////////////////

//int HAPClient::listControllers(uint8_t *tlvBuf){
//
//  int nBytes=0;
//  int n;
//    
//  tlv8.clear();
//  tlv8.val(kTLVType_State,pairState_M2);      
//
//  for(auto it=controllerList.begin();it!=controllerList.end();it++){
//    if((*it).allocated){          
//      if(tlv8.val(kTLVType_State)==-1)                // if State is not set then this is not the first controller found
//        tlv8.val(kTLVType_Separator,1);                                        
//      tlv8.val(kTLVType_Permissions,(*it).admin);      
//      tlv8.buf(kTLVType_Identifier,(*it).ID,36);
//      tlv8.buf(kTLVType_PublicKey,(*it).LTPK,32);
//      n=tlv8.pack(tlvBuf);
//      nBytes+=n;
//      if(tlvBuf){
//        tlvBuf+=n;
//        tlv8.print();
//      }
//      tlv8.clear();
//    }
//  }
//
//  return(nBytes);       
//}

//////////////////////////////////////

void HAPClient::printControllers(int minLogLevel){

  if(homeSpan.logLevel<minLogLevel)
    return;

  if(controllerList.empty()){
    Serial.printf("No Paired Controllers\n");
    return;    
  }
  
  for(auto it=controllerList.begin();it!=controllerList.end();it++){
    Serial.printf("Paired Controller: ");
    charPrintRow((*it).ID,36);
    Serial.printf("%s  LTPK: ",(*it).admin?"   (admin)":" (regular)");
    hexPrintRow((*it).LTPK,32);
    Serial.printf("\n");    
  }
}

//////////////////////////////////////

void HAPClient::saveControllers(){

  if(controllerList.empty()){
    nvs_erase_key(hapNVS,"CONTROLLERS");
    return;
  }

  TempBuffer<Controller> tBuf(controllerList.size());                    // create temporary buffer to hold Controller data
  std::copy(controllerList.begin(),controllerList.end(),tBuf.get());      // copy data from linked list to buffer
  
  nvs_set_blob(hapNVS,"CONTROLLERS",tBuf,tBuf.len());      // update data
  nvs_commit(hapNVS);                                            // commit to NVS  
}


//////////////////////////////////////

Nonce::Nonce(){
  zero();
}

//////////////////////////////////////

void Nonce::zero(){
  memset(x,0,12);
}

//////////////////////////////////////

uint8_t *Nonce::get(){
  return(x);
}

//////////////////////////////////////

void Nonce::inc(){
  x[4]++;
  if(x[4]==0)
    x[5]++;
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

// instantiate all static HAP Client structures and data

//TLV<kTLVType,11> HAPClient::tlv8;
nvs_handle HAPClient::hapNVS;
nvs_handle HAPClient::srpNVS;
HKDF HAPClient::hkdf;                                   
pairState HAPClient::pairStatus;                        
Accessory HAPClient::accessory;                         
list<Controller, Mallocator<Controller>> HAPClient::controllerList;
SRP6A HAPClient::srp;
int HAPClient::conNum;
 
