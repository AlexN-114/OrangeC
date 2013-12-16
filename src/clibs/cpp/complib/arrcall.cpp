/*
	Software License Agreement (BSD License)
	
	Copyright (c) 1997-2013, David Lindauer, (LADSoft).
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
#include <windows.h>

typedef void (*CONSDEST)(void *);
void __arrCall(void *instance, void *cons, void *dest, int elems, int size)
{
    void *pos = instance;
    if (cons)
    {
        try
        {
            for (int i=0; i < elems; i++)
            {
                CONSDEST xx = (CONSDEST)cons;
                (*xx)(pos);
                pos = (void *)((BYTE *)pos + size);
            }
            ((int *)instance)[-2] = elems; // NEXT field gets the # elems
        }
        catch(...)
        {
            if (dest)
            {
                pos = (void *)((BYTE *)pos - size);
                while (pos >= instance)
                {
                    CONSDEST xx = (CONSDEST)dest;
                    (*xx)(pos);
                    pos = (void *)((BYTE *)pos - size);
                }
            }
            throw;            
        }
    }
    else if (dest)
    {
        elems = ((int *)instance)[-2];  // get #elems from the NEXT fields
        pos = (void *)((BYTE *)pos + (elems-1) * size);
        while (pos >= instance)
        {
            CONSDEST xx = (CONSDEST)dest;
            (*xx)(pos);
            pos = (void *)((BYTE *)pos - size);
        }
    }    
}