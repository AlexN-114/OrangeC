/*
    Software License Agreement (BSD License)
    
    Copyright (c) 1997-2008, David Lindauer, (LADSoft).
    All rights reserved.
    
    Redistribution and use of this software in source and binary forms, with or without modification, are
    permitted provided that the following conditions are met:
    
    * Redistributions of source code must retain the above
      copyright notice, this list of conditions and the
      following disclaimer.
    
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the
      following disclaimer in the documentation and/or other
      materials provided with the distribution.
    
    * Neither the name of LADSoft nor the names of its
      contributors may be used to endorse or promote products
      derived from this software without specific prior
      written permission of LADSoft.
    
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
    WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
    ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
    ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef	_UCHAR_H_
#define	_UCHAR_H_

#pragma pack(1)

#ifndef __STDDEF_H
#include <stddef.h>
#endif


#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _MBSTATE_T_DEFINED
#define _MBSTATE_T_DEFINED
typedef struct __mbstate_t
{
  size_t left;
  wint_t value;
} mbstate_t;
#endif

typedef __char16_t char16_t;
typedef __char32_t char32_t;

size_t _RTL_FUNC _IMPORT mbrtoc16(char16_t * restrict pc16, const char * restrict s, 
                          size_t n, mbstate_t * restrict ps);

size_t _RTL_FUNC _IMPORT c16rtomb(char * restrict s, char16_t c16, mbstate_t * restrict ps);
size_t _RTL_FUNC _IMPORT mbrtoc32(char32_t * restrict pc32, const char * restrict s, 
                          size_t n, mbstate_t * restrict ps);

size_t _RTL_FUNC _IMPORT c32rtomb(char * restrict s, char32_t c32, mbstate_t * restrict ps);

#ifdef	__cplusplus
}
#endif

#pragma pack()

#endif  /* _UTIME_H_ */
