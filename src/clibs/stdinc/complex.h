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
#ifndef __COMPLEX_H
#define __COMPLEX_H

#ifndef __STDDEF_H
#include <stddef.h>
#endif

/* this is the default even when this file isn't included */
#pragma STDC CX_LIMITED_RANGE OFF

#define complex _Complex
#define _Complex_I (0.0F + __I)

#define imaginary _Imaginary
#define _Imaginary_I (__I)

#define I _Imaginary_I

#define CMPLX(x, y) ((double complex)((double)(x) + _Imaginary_I * (double)(y)))
#define CMPLXF(x, y) ((float complex)((float)(x) + _Imaginary_I * (float)(y)))
#define CMPLXL(x, y) ((long double complex)((long double)(x) + _Imaginary_I * (long double)(y)))

#ifdef __cplusplus
namespace std {
extern "C" {
#endif

float complex _RTL_FUNC cacosf(float complex) ;
double complex _RTL_FUNC cacos(double complex) ;
long double complex _RTL_FUNC cacosl(long double complex) ;

float complex _RTL_FUNC casinf(float complex) ;
double complex _RTL_FUNC casin(double complex) ;
long double complex _RTL_FUNC casinl(long double complex) ;

float complex _RTL_FUNC catanf(float complex) ;
double complex _RTL_FUNC catan(double complex) ;
long double complex _RTL_FUNC catanl(long double complex) ;

float complex _RTL_FUNC ccosf(float complex) ;
double complex _RTL_FUNC ccos(double complex) ;
long double complex _RTL_FUNC ccosl(long double complex) ;

float complex _RTL_FUNC csinf(float complex) ;
double complex _RTL_FUNC csin(double complex) ;
long double complex _RTL_FUNC csinl(long double complex) ;

float complex _RTL_FUNC ctanf(float complex) ;
double complex _RTL_FUNC ctan(double complex) ;
long double complex _RTL_FUNC ctanl(long double complex) ;

float complex _RTL_FUNC cacoshf(float complex) ;
double complex _RTL_FUNC cacosh(double complex) ;
long double complex _RTL_FUNC cacoshl(long double complex) ;

float complex _RTL_FUNC casinhf(float complex) ;
double complex _RTL_FUNC casinh(double complex) ;
long double complex _RTL_FUNC casinhl(long double complex) ;

float complex _RTL_FUNC catanhf(float complex) ;
double complex _RTL_FUNC catanh(double complex) ;
long double complex _RTL_FUNC catanhl(long double complex) ;

float complex _RTL_FUNC ccoshf(float complex) ;
double complex _RTL_FUNC ccosh(double complex) ;
long double complex _RTL_FUNC ccoshl(long double complex) ;

float complex _RTL_FUNC csinhf(float complex) ;
double complex _RTL_FUNC csinh(double complex) ;
long double complex _RTL_FUNC csinhl(long double complex) ;

float complex _RTL_FUNC ctanhf(float complex) ;
double complex _RTL_FUNC ctanh(double complex) ;
long double complex _RTL_FUNC ctanhl(long double complex) ;

float complex _RTL_FUNC cexpf(float complex) ;
double complex _RTL_FUNC cexp(double complex) ;
long double complex _RTL_FUNC cexpl(long double complex) ;

float complex _RTL_FUNC clogf(float complex) ;
double complex _RTL_FUNC clog(double complex) ;
long double complex _RTL_FUNC clogl(long double complex) ;

float _RTL_FUNC cabsf(float complex) ;
double _RTL_FUNC cabs(double complex) ;
long double _RTL_FUNC cabsl(long double complex) ;

float complex _RTL_FUNC cpowf(float complex, float complex) ;
double complex _RTL_FUNC cpow(double complex, double complex) ;
long double complex _RTL_FUNC cpowl(long double complex, long double complex) ;

float complex _RTL_FUNC csqrtf(float complex) ;
double complex _RTL_FUNC csqrt(double complex) ;
long double complex _RTL_FUNC csqrtl(long double complex) ;

float _RTL_FUNC cargf(float complex) ;
double _RTL_FUNC carg(double complex) ;
long double _RTL_FUNC cargl(long double complex) ;

float _RTL_FUNC cimagf(float complex) ;
double _RTL_FUNC cimag(double complex) ;
long double _RTL_FUNC cimagl(long double complex) ;

float complex _RTL_FUNC conjf(float complex) ;
double complex _RTL_FUNC conj(double complex) ;
long double complex _RTL_FUNC conjl(long double complex) ;

float complex _RTL_FUNC cprojf(float complex) ;
double complex _RTL_FUNC cproj(double complex) ;
long double complex _RTL_FUNC cprojl(long double complex) ;

float _RTL_FUNC crealf(float complex) ;
double _RTL_FUNC creal(double complex) ;
long double _RTL_FUNC creall(long double complex) ;

#ifdef __cplusplus
} ;
} ;
#endif
#endif /* __COMPLEX_H */

#if defined(__cplusplus) && !defined(__USING_CNAME__) && !defined(__COMPLEX_H_USING_LIST)
#define __COMPLEX_H_USING_LIST

using std::cacosf ;
using std::cacos ;
using std::cacosl ;
using std::casinf ;
using std::casin ;
using std::casinl ;
using std::catanf ;
using std::catan ;
using std::catanl ;
using std::ccosf ;
using std::ccos ;
using std::ccosl ;
using std::csinf ;
using std::csin ;
using std::csinl ;
using std::ctanf ;
using std::ctan ;
using std::ctanl ;
using std::cacoshf ;
using std::cacosh ;
using std::cacoshl ;
using std::casinhf ;
using std::casinh ;
using std::casinhl ;
using std::catanhf ;
using std::catanh ;
using std::catanhl ;
using std::ccoshf ;
using std::ccosh ;
using std::ccoshl ;
using std::csinhf ;
using std::csinh ;
using std::csinhl ;
using std::ctanhf ;
using std::ctanh ;
using std::ctanhl ;
using std::cexpf ;
using std::cexp ;
using std::cexpl ;
using std::clogf ;
using std::clog ;
using std::clogl ;
using std::cabsf ;
using std::cabs ;
using std::cabsl ;
using std::cposf ;
using std::cpos ;
using std::cposl ;
using std::csqrtf ;
using std::csqrt ;
using std::csqrtl ;
using std::cargf ;
using std::carg ;
using std::cargl ;
using std::cimagf ;
using std::cimag ;
using std::cimagl ;
using std::conjf ;
using std::conj ;
using std::conjl ;
using std::cprojf ;
using std::cproj ;
using std::cprojl ;
using std::crealf ;
using std::creal ;
using std::creall ;
#endif
