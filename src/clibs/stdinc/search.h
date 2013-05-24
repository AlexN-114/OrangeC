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

#ifndef __SEARCH_H
#define __SEARCH_H

#ifndef __STDDEF_H
#include <stddef.h>
#endif

#ifdef __cplusplus
namespace std {
extern "C" {
#endif
void *      _RTL_FUNC bsearch(const void * __key, const void * __base,
                           size_t __nelem, size_t __width,
                           int (*fcmp)(const void *, const void *));
void *      _RTL_FUNC lfind(const void * __key, const void * __base,
                                size_t * __num, size_t __width,
                                int (*fcmp)(const void *, const void *));
void *      _RTL_FUNC lsearch(const void * __key, void * __base,
                                size_t * __num, size_t __width,
                                int (*fcmp)(const void *, const void *));
void        _RTL_FUNC qsort(void * __base, size_t __nelem, size_t __width,
                         int (*__fcmp)(const void *, const void *));

void * _RTL_FUNC _lfind(const void *, const void *, unsigned int *, unsigned int,
        int (*)(const void *, const void *));
void * _RTL_FUNC _lsearch(const void *, void  *, unsigned int *, unsigned int,
                                int (*)(const void *, const void *));
                          
#ifdef __cplusplus
}
}
#endif

#endif  /* __SEARCH_H */

#if defined(__cplusplus) && !defined(__USING_CNAME__) && !defined(__SEARCH_H_USING_LIST)
#define __SEARCH_H_USING_LIST
using std::bsearch;
using std::lfind;
using std::lsearch;
using std::_lfind;
using std::_lsearch;
using std::qsort;
#endif /* __USING_CNAME__ */
