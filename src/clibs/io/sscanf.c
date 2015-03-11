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
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <locale.h>
#include "libp.h"

int _RTL_FUNC scanf(const char *format, ...)
{
   return __scanf(stdin,format,((char *)&format+sizeof(char *)));
}

int _RTL_FUNC vsscanf(char *restrict buf, const char *restrict format, va_list list)
{
   int rv ;
   FILE fil ;
   struct __file2 fil2;
   memset(&fil,0,sizeof(fil)) ;
   memset(&fil2,0,sizeof(fil2)) ;
   fil.level = strlen(buf)+1 ;
   fil.flags = _F_IN | _F_READ | _F_BUFFEREDSTRING ;
   fil.bsize = strlen(buf) ;
   fil.buffer = fil.curp = buf ;
   fil.token = FILTOK ;
   fil.extended = &fil2;
   return __scanf(&fil,format,list);
}
int _RTL_FUNC sscanf(char *restrict buf, const char *restrict format, ...)
{
   return vsscanf(buf,format,(((char *)&format)+sizeof(char *)));
}
int vfscanf(FILE *restrict fil, const char *restrict format, va_list arglist)
{
    return __scanf(fil,format,arglist);
}
int vscanf(const char *restrict format, va_list arglist)
{
    return __scanf(stdin,format,arglist);
}
int fscanf(FILE *restrict fil, const char *restrict format, ...)
{
   return __scanf(fil,format,((char *)&format+sizeof(char *)));
}
