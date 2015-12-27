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
#include <new>
#include <stdlib.h>

namespace __dls {
   const char _RTL_DATA *__dls_bad_alloc = "bad_alloc" ;
} ;

namespace std {

//   nothrow_t _RTL_DATA nothrow;
   _RTL_FUNC bad_alloc::~bad_alloc() 
   {
   }

} ;

#ifdef STD_NEWHANDLER
static std::new_handler _new_handler ;
namespace std {
#else
static new_handler _new_handler ;
#endif       

new_handler _RTL_FUNC set_new_handler(new_handler __newv)
{
   new_handler rv = _new_handler ;
   _new_handler = __newv ;
   return rv ;
}
#ifdef STD_NEWHANDLER
}
#endif       

void *__realnew(size_t n)
{
   if (!n)
      n = 1 ;
   do {
      void *rv = malloc(n) ;
      if (rv)
         return rv ;
      if (!_new_handler)
          throw std::bad_alloc() ;
      (*_new_handler)() ;
   } while (1) ;
   return 0 ; // never gets here
}