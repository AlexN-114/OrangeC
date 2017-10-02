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
#include <time.h>
#include <wchar.h>
#include <locale.h>
#include "libp.h"

static char _monthdays[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };

struct tm *_RTL_FUNC gmtime(const time_t *time)
{
    struct tm *rv = &__getRtlData()->gmtime_buf;
	time_t t = *time;
	int temp1;
	
        t -= _daylight * 60 * 60;
	if (t & 0x80000000)
		return NULL;
	rv->tm_sec = t %60;
	t/=60;
	rv->tm_min = t %60;
	t /=60;
	rv->tm_hour = t %24;
	t /=24;
	rv->tm_yday = t;
	rv->tm_wday = (t +4 )%7;
	rv->tm_year = 70+(rv->tm_yday /365);
	rv->tm_yday = rv->tm_yday % 365;
	rv->tm_yday -= (rv->tm_year - 69)/4 ;
	if (rv->tm_yday <0) {
		rv->tm_year--;
		rv->tm_yday += 365 + ((rv->tm_year - 68) % 4 == 0);
	}
	if ((rv->tm_year - 68) % 4 == 0)
		_monthdays[1] = 29;
	else
		_monthdays[1] = 28;
	temp1 = rv->tm_yday;
	rv->tm_mon = -1;
	while (temp1 >=0)
		temp1-=_monthdays[++rv->tm_mon];
	rv->tm_mday = temp1 + _monthdays[rv->tm_mon]+1;
	rv->tm_isdst = 0;
   return rv;
}
