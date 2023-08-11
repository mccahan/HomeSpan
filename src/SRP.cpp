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
 
#include <sodium.h>
#include <Arduino.h>

#include <mbedtls/sha512.h>

#include "SRP.h"
#include "TempBuf.h"

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

SRP6A::SRP6A(){

  // initialize MPI structures
  
  mbedtls_mpi_init(&N);     
  mbedtls_mpi_init(&g);
  mbedtls_mpi_init(&s);
  mbedtls_mpi_init(&x);
  mbedtls_mpi_init(&v);
  mbedtls_mpi_init(&A);
  mbedtls_mpi_init(&b);
  mbedtls_mpi_init(&B);
  mbedtls_mpi_init(&S);
  mbedtls_mpi_init(&k);
  mbedtls_mpi_init(&u);
  mbedtls_mpi_init(&K);
  mbedtls_mpi_init(&M1);
  mbedtls_mpi_init(&M1V);
  mbedtls_mpi_init(&M2);
  mbedtls_mpi_init(&_rr);
  mbedtls_mpi_init(&t1);
  mbedtls_mpi_init(&t2);
  mbedtls_mpi_init(&t3);  
}

//////////////////////////////////////

void SRP6A::clear(){

  mbedtls_mpi_free(&N);     
  mbedtls_mpi_free(&g);
  mbedtls_mpi_free(&s);
  mbedtls_mpi_free(&x);
  mbedtls_mpi_free(&v);
  mbedtls_mpi_free(&A);
  mbedtls_mpi_free(&b);
  mbedtls_mpi_free(&B);
  mbedtls_mpi_free(&S);
  mbedtls_mpi_free(&k);
  mbedtls_mpi_free(&u);
  mbedtls_mpi_free(&K);
  mbedtls_mpi_free(&M1);
  mbedtls_mpi_free(&M1V);
  mbedtls_mpi_free(&M2);
  mbedtls_mpi_free(&_rr);
  mbedtls_mpi_free(&t1);
  mbedtls_mpi_free(&t2);
  mbedtls_mpi_free(&t3);
}

//////////////////////////////////////

void SRP6A::createVerifyCode(const char *setupCode, uint8_t *verifyCode, uint8_t *salt){

  TempBuffer<uint8_t> tBuf(80);    // temporary buffer for staging
  TempBuffer<uint8_t> tHash(64);   // temporary buffer for storing SHA-512 results  
  TempBuffer<char> icp(22);        // storage for I:P

  randombytes_buf(salt,16);                 // generate 16 random bytes using libsodium (which uses the ESP32 hardware-based random number generator)    
  mbedtls_mpi_read_binary(&s,salt,16);

  sprintf(icp,"Pair-Setup:%.3s-%.2s-%.3s",setupCode,setupCode+3,setupCode+5);

  // compute x = SHA512( s | SHA512( I | ":" | P ) )

  mbedtls_mpi_write_binary(&s,tBuf,16);                             // write s into first 16 bytes of staging buffer            
  mbedtls_sha512_ret((uint8_t *)icp.get(),strlen(icp),tBuf+16,0);   // create hash of username:password and write into last 64 bytes of staging buffer
  mbedtls_sha512_ret(tBuf,80,tHash,0);                              // create second hash of salted, hashed username:password 
  mbedtls_mpi_read_binary(&x,tHash,64);                             // load hash result into mpi structure x

  // compute v = g^x % N

  mbedtls_mpi_read_string(&N,16,N3072);                       // load N
  mbedtls_mpi_lset(&g,5);                                     // load g
  mbedtls_mpi_exp_mod(&v,&g,&x,&N,&_rr);                      // create verifier, v (_rr is an internal "helper" structure that mbedtls uses to speed up subsequent exponential calculations)
  mbedtls_mpi_write_binary(&v,verifyCode,384);                // write v into verifyCode

  mbedtls_mpi_free(&x);
  mbedtls_mpi_free(&N);
  mbedtls_mpi_free(&g);
  mbedtls_mpi_free(&v);
  mbedtls_mpi_free(&s);
  mbedtls_mpi_free(&_rr);
}

//////////////////////////////////////

SRP6A SRP;      // create global structure SRP

//////////////////////////////////////
