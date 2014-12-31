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
/*  string.h

    Definitions for memory and string functions.

*/

#ifndef __STRING_H
#define __STRING_H

#ifndef __STDDEF_H
#include <stddef.h>
#endif

#ifdef __cplusplus
namespace __STD_NS__ {
extern "C" {
#endif

int          _RTL_INTRINS bcmp(const void *, const void *, size_t);

void         _RTL_INTRINS bcopy(const void *, void *, size_t);

void         _RTL_INTRINS bzero(void *, size_t);

int          _RTL_INTRINS ffs(int);

//char         _RTL_INTRINS *index(const char *, int);

//char         _RTL_INTRINS *rindex(const char *, int);

int          _RTL_INTRINS strcasecmp(const char *, const char *);

int          _RTL_INTRINS strncasecmp(const char *, const char *, size_t);

int          _RTL_INTRINS memcmp(const void *__s1,
                                       const void *__s2, size_t __n);
void *       _RTL_INTRINS memcpy(void *restrict __dest, const void *restrict __src,
                                       size_t __n);
void *       _RTL_INTRINS memmove(void *__dest, const void *__src,
                                        size_t __n);
void *       _RTL_INTRINS memset(void *__s, int __c, size_t __n);
char *       _RTL_INTRINS strcat(char *restrict __dest, const char *restrict __src);
size_t 		 _RTL_FUNC strlcat (char * dst, const char * src, size_t n);
int          _RTL_INTRINS strcmp(const char *__s1, const char *__s2);
char *       _RTL_INTRINS strcpy(char *restrict __dest, const char *restrict __src);
size_t       _RTL_INTRINS strlcpy (char * dst, const char * src, size_t n);
char *       _RTL_INTRINS stpcpy(char *restrict __dest, const char *restrict __src);
size_t       _RTL_FUNC strcspn(const char *__s1, const char *__s2);
char *       _RTL_FUNC strdup(const char *__string);
char *       _RTL_FUNC strerror(int __errnum);
size_t       _RTL_INTRINS strlen(const char *__s);
char *       _RTL_INTRINS strncat(char *restrict __dest, const char *restrict __src,
                                        size_t __maxlen);
int          _RTL_INTRINS strncmp(const char *__s1, const char *__s2,
                                        size_t __maxlen);
char *       _RTL_INTRINS strncpy(char *restrict __dest, const char *restrict __src,
                                        size_t __maxlen);
char *       _RTL_INTRINS stpncpy(char *restrict __dest, const char *restrict __src,
                                        size_t __maxlen);
size_t       _RTL_FUNC strspn(const char *__s1, const char *__s2);
char *       _RTL_FUNC strtok(char *restrict __s1, const char *restrict __s2);
char *       _RTL_FUNC _strerror(const char *__s);


void *       _RTL_INTRINS memchr(const void *__s, int __c, size_t __n);
char *       _RTL_INTRINS strchr(const char * __s, int __c);
char *       _RTL_INTRINS strrchr(const char *__s, int __c);
char *       _RTL_FUNC strpbrk(const char *__s1, const char *__s2);
char *       _RTL_FUNC strstr(const char *__s1, const char *__s2);

int          _RTL_FUNC _lstrcoll(const char *__s1, const char *__s2);
size_t       _RTL_FUNC _lstrxfrm(char *restrict __s1, const char *restrict __s2,
                                            size_t __n );
int          _RTL_FUNC strcoll(const char *__s1, const char *__s2);
size_t       _RTL_FUNC strxfrm(char *restrict __s1, const char *restrict __s2,
                                          size_t __n );

int    		 _RTL_FUNC memicmp(const void *, const void *, size_t);
void *       _RTL_FUNC memccpy(void *, const void *, int, size_t);
char * 		 _RTL_FUNC strset(char *, int);
char *  	 _RTL_FUNC strnset(char *, int, size_t);
char *		 _RTL_FUNC strrev(char *);
char *       _RTL_FUNC strupr(char * __s);
char *       _RTL_FUNC strlwr(char * __s);
int          _RTL_FUNC strnicmp(const char *__s1, const char *__s2, size_t __n);
int          _RTL_FUNC stricmp(const char *__s1, const char *__s2);
int          _RTL_FUNC strncmpi(const char *__s1, const char *__s2, size_t __n);
int          _RTL_FUNC strcmpi(const char *__s1, const char *__s2);

int    		 _RTL_FUNC _memicmp(const void *, const void *, unsigned int);
void *       _RTL_FUNC _memccpy(void *, const void *, int, unsigned int);
char * 		 _RTL_FUNC _strset(char *, int);
char * 		 _RTL_FUNC _strnset(char *, int, size_t);
char * 		 _RTL_FUNC _strrev(char *);

char *       _RTL_FUNC _strdup(const char *string);
int    		 _RTL_FUNC _stricmp(const char *, const char *);
int    		 _RTL_FUNC _strcmpi(const char *, const char *);
int    		 _RTL_FUNC _strnicmp(const char *, const char *, size_t);
int    		 _RTL_FUNC _strncmpi(const char *, const char *, size_t);
char * 		 _RTL_FUNC _strlwr(char *);
char * 		 _RTL_FUNC _strupr(char *);

#if defined(__USELOCALES__)
#define  strupr   _lstrupr
#define  strlwr   _lstrlwr
#define  strcoll  _lstrcoll
#define  strxfrm  _lstrxfrm
#endif  /* __USELOCALES__ */


#ifdef __cplusplus
};
};
#endif

#endif  /* __STRING_H */
#if defined(__cplusplus) && !defined(__USING_CNAME__) && !defined(__STRING_H_USING_LIST)
#define __STRING_H_USING_LIST
    using __STD_NS_QUALIFIER memcmp;
    using __STD_NS_QUALIFIER memcpy;
    using __STD_NS_QUALIFIER memccpy;
    using __STD_NS_QUALIFIER memmove;
    using __STD_NS_QUALIFIER memset;
    using __STD_NS_QUALIFIER strcat;
    using __STD_NS_QUALIFIER strcmp;
    using __STD_NS_QUALIFIER strcpy;
    using __STD_NS_QUALIFIER stpcpy;
    using __STD_NS_QUALIFIER strcspn;
    using __STD_NS_QUALIFIER strdup;
    using __STD_NS_QUALIFIER strerror;
    using __STD_NS_QUALIFIER strlen;
    using __STD_NS_QUALIFIER strncat;
    using __STD_NS_QUALIFIER strncmp;
    using __STD_NS_QUALIFIER strncpy;
    using __STD_NS_QUALIFIER stpncpy;
    using __STD_NS_QUALIFIER strspn;
    using __STD_NS_QUALIFIER strtok;
    using __STD_NS_QUALIFIER _strerror;
    using __STD_NS_QUALIFIER memchr;
    using __STD_NS_QUALIFIER strchr;
    using __STD_NS_QUALIFIER strrchr;
    using __STD_NS_QUALIFIER strpbrk;
    using __STD_NS_QUALIFIER strstr;
    using __STD_NS_QUALIFIER _lstrcoll;
    using __STD_NS_QUALIFIER _lstrxfrm;
    using __STD_NS_QUALIFIER strcoll;
    using __STD_NS_QUALIFIER strxfrm;
    using __STD_NS_QUALIFIER strupr;
    using __STD_NS_QUALIFIER strlwr;
    using __STD_NS_QUALIFIER strnicmp;
    using __STD_NS_QUALIFIER stricmp;
    using __STD_NS_QUALIFIER strncmpi;
    using __STD_NS_QUALIFIER strcmpi;
    using __STD_NS_QUALIFIER _strdup;
    using __STD_NS_QUALIFIER _memicmp;
    using __STD_NS_QUALIFIER memicmp;
    using __STD_NS_QUALIFIER _strset;
    using __STD_NS_QUALIFIER strset;
    using __STD_NS_QUALIFIER _strnset;
    using __STD_NS_QUALIFIER strnset;
    using __STD_NS_QUALIFIER _strrev;
    using __STD_NS_QUALIFIER strrev;
    using __STD_NS_QUALIFIER _stricmp;
    using __STD_NS_QUALIFIER _strcmpi;
    using __STD_NS_QUALIFIER _strnicmp;
    using __STD_NS_QUALIFIER _strncmpi;
    using __STD_NS_QUALIFIER _strlwr;
    using __STD_NS_QUALIFIER _strupr;
    using __STD_NS_QUALIFIER strlcpy;
    using __STD_NS_QUALIFIER strlcat;
    using __STD_NS_QUALIFIER bcmp;
    using __STD_NS_QUALIFIER bcopy;
    using __STD_NS_QUALIFIER bzero;
    using __STD_NS_QUALIFIER ffs;
    using __STD_NS_QUALIFIER strcasecmp;
    using __STD_NS_QUALIFIER strncasecmp;
#endif /* __USING_CNAME__ */
