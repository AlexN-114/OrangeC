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
/*  memory.h

    Memory manipulation functions

*/

#if !defined(__MEM_H)
#define __MEM_H

#ifndef __STDDEF_H
#include <stddef.h>
#endif

#ifdef __cplusplus
namespace std {
extern "C" {
#endif

void *    _RTL_FUNC memccpy(void *__dest, const void *__src,
                                        int __c, size_t __n);
int       _RTL_FUNC memcmp(const void *__s1, const void *__s2,
                                       size_t __n);
void *    _RTL_FUNC memcpy(void *__dest, const void *__src,
                                       size_t __n);
int       _RTL_FUNC memicmp(const void *__s1, const void *__s2,
                                        size_t __n);
void *    _RTL_FUNC memmove(void *__dest, const void *__src,
                                        size_t __n);
void *    _RTL_FUNC memset(void *__s, int __c, size_t __n);

void *    _RTL_FUNC memchr(const void *__s, int __c, size_t __n);

int    		 _RTL_FUNC _memicmp(const void *, const void *, unsigned int);
void *       _RTL_FUNC _memccpy(void *, const void *, int, unsigned int);

#ifdef __cplusplus
};
};
#endif

#endif  /* __MEM_H */
#if defined(__cplusplus) && !defined(__USING_CNAME__) && !defined(__MEMORY_H_USING_LIST)
#define __MEMORY_H_USING_LIST
    using std::memccpy;
    using std::memcmp;
    using std::memcpy;
    using std::memicmp;
    using std::memmove;
    using std::memset;
    using std::memchr;
#endif
