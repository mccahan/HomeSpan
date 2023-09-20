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

/////////////////////////////////////////////////
// Creates a temporary buffer that is freed after
// going out of scope

template <class bufType>
class TempBuffer {

  private:
  
  bufType *buf;
  int nBytes;
  int nElements;

  public:
  
  TempBuffer(int _nElements) : nElements(_nElements) {
    nBytes=nElements*sizeof(bufType);
    buf=(bufType *)malloc(nBytes);
    if(buf==NULL){
      Serial.printf("\n*** FATAL ERROR: Requested allocation of %d bytes failed.\n*** PROGRAM HALTED ***\n\n",nBytes);
      while(1);
    }
   }

  ~TempBuffer(){
    free(buf);
  }

  int len(){
    return(nBytes);
  }

  int size(){
    return(nElements);
  }

  bufType *get(){
    return(buf);
  }

  operator bufType*(){
    return(buf);
  }
  
};

/////////////////////////////////////////////////
