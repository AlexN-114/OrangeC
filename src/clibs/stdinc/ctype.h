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
/*  ctype.h

    Defines the locale aware ctype macros.

*/


#ifndef __pctype_H
#define __pctype_H

#ifndef __STDDEF_H
#include <stddef.h>
#endif

#ifdef __cplusplus
namespace std {
extern "C" {
#endif

extern unsigned short const _RTL_DATA * _pctype;

/* character classes */

#ifndef _IS_CONSTANTS
#define _IS_CONSTANTS

#define _IS_UPP     1           /* upper case */
#define _IS_LOW     2           /* lower case */
#define _IS_DIG     4           /* digit */
#define _IS_SP      8           /* space */
#define _IS_PUN    16           /* punctuation */
#define _IS_CTL    32           /* control */
#define _IS_BLK    64           /* blank */
#define _IS_HEX   128           /* [0..9] or [A-F] or [a-f] */
#define _IS_GPH   512

#define _IS_ALPHA    (0x100 | _IS_UPP | _IS_LOW)
#define _IS_ALNUM    (_IS_DIG | _IS_ALPHA)
#define _IS_GRAPH    (_IS_ALNUM | _IS_HEX | _IS_PUN)
#define _IS_PRINT    (_IS_GRAPH | _IS_BLK)
#endif

int      _RTL_FUNC isalnum (int __c);
int      _RTL_FUNC isalpha (int __c);
int      _RTL_FUNC isblank (int __c);
int      _RTL_FUNC iscntrl (int __c);
int      _RTL_FUNC isdigit (int __c);
int      _RTL_FUNC isgraph (int __c);
int      _RTL_FUNC islower (int __c);
int      _RTL_FUNC isprint (int __c);
int      _RTL_FUNC ispunct (int __c);
int      _RTL_FUNC isspace (int __c);
int      _RTL_FUNC isupper (int __c);
int      _RTL_FUNC isxdigit(int __c);
int      _RTL_FUNC isascii (int __c);
int 	 _RTL_FUNC toascii(int);
int		 _RTL_FUNC __isascii(int);
int		 _RTL_FUNC __toascii(int);

int      _RTL_FUNC tolower(int __ch);
int      _RTL_FUNC _ltolower(int __ch);
int      _RTL_FUNC toupper(int __ch);
int      _RTL_FUNC _ltoupper(int __ch);

int		 _RTL_FUNC _isctype(int, int);
int		 _RTL_FUNC __iscsymf(int);
int		 _RTL_FUNC __iscsym(int);
#ifdef __cplusplus
};
};
#endif

#define isalnum(c)   ( __STD_NS_QUALIFIER _pctype[ (c) ] & (_IS_ALNUM))
                     
#define isalpha(c)   ( __STD_NS_QUALIFIER _pctype[ (c) ] & (_IS_ALPHA))
                     
#define isblank(c)   ( __STD_NS_QUALIFIER _pctype[ (c) ] & (_IS_BLK))
                     
#define iscntrl(c)   ( __STD_NS_QUALIFIER _pctype[ (c) ] & (_IS_CTL))
                     
#define isdigit(c)   ( __STD_NS_QUALIFIER _pctype[ (c) ] & (_IS_DIG))
                     
#define isgraph(c)   ( __STD_NS_QUALIFIER _pctype[ (c) ] & (_IS_GRAPH))
                     
#define islower(c)   ( __STD_NS_QUALIFIER _pctype[ (c) ] & (_IS_LOW))
                     
#define isprint(c)   ( __STD_NS_QUALIFIER _pctype[ (c) ] & (_IS_PRINT))
                     
#define ispunct(c)   ( __STD_NS_QUALIFIER _pctype[ (c) ] & (_IS_PUN))
                     
#define isspace(c)   ( __STD_NS_QUALIFIER _pctype[ (c) ] & (_IS_SP))
                     
#define isupper(c)   ( __STD_NS_QUALIFIER _pctype[ (c) ] & (_IS_UPP))
                     
#define isxdigit(c)  ( __STD_NS_QUALIFIER _pctype[ (c) ] & (_IS_HEX))


#define _toupper(c) ((c) + 'A' - 'a')
#define _tolower(c) ((c) + 'a' - 'A')
#define isascii(c)  ((unsigned)(c) < 128)
#define toascii(c)  ((c) & 0x7f)


#endif 

#if defined(__cplusplus) && !defined(__USING_CNAME__) && !defined(__pctype_H_USING_LIST)
#define __pctype_H_USING_LIST
    using std::isalnum;
    using std::isalpha;
    using std::isblank;
    using std::iscntrl;
    using std::isdigit;
    using std::isgraph;
    using std::islower;
    using std::isprint;
    using std::ispunct;
    using std::isspace;
    using std::isupper;
    using std::isxdigit;
    using std::isascii;
    using std::toascii;
    using std::__isascii;
    using std::__toascii;
    using std::tolower;
    using std::_ltolower;
    using std::toupper;
    using std::_ltoupper;
    using std::_pctype;
    using std::_isctype;
    using std::__iscsymf;
    using std::__iscsym;
#endif

