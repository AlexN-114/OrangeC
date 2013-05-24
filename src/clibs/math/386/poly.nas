;Software License Agreement (BSD License)
;
;Copyright (c) 1997-2008, David Lindauer, (LADSoft).
;All rights reserved.
;
;Redistribution and use of this software in source and binary forms, with or without modification, are
;permitted provided that the following conditions are met:
;
;* Redistributions of source code must retain the above
;  copyright notice, this list of conditions and the
;  following disclaimer.
;
;* Redistributions in binary form must reproduce the above
;  copyright notice, this list of conditions and the
;  following disclaimer in the documentation and/or other
;  materials provided with the distribution.
;
;* Neither the name of LADSoft nor the names of its
;  contributors may be used to endorse or promote products
;  derived from this software without specific prior
;  written permission of LADSoft.
;
;THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
;WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
;PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
;ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
;LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
;INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
;TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
;ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
%include "matherr.inc"
%ifdef __BUILDING_LSCRTL_DLL
[export _poly]
[export _polevl]
[export _p1evl]
%endif
[global _poly]
[global _polevl]
[global _p1evl]

SECTION data CLASS=DATA USE32
nm	db	"poly",0
nmo db  "polevl",0
nm1 db  "p1evl" , 0
SECTION code CLASS=CODE USE32
_poly:
    lea	eax,[nm]
    call clearmath
    mov	ecx,[esp+12]
    mov	eax,[esp+16]
    lea	eax,[eax + ecx * 8]
    fld	qword [esp+4]
    fld	qword [eax]
    sub	eax,8
lp:
    fmul	st0,st1
    fld		qword [eax]
    sub		eax,8
    faddp	st1
    loop	lp
    fxch
    popone
    mov dl,1
    jmp wrapmath

_polevl:
    lea	eax,[nmo]
    call clearmath
    mov	eax,[esp+12]
    mov	ecx,[esp+16]
    fld	qword [esp+4]
    fld	qword [eax]
    add	eax,8
lpo:
    fmul	st0,st1
    fld		qword [eax]
    add		eax,8
    faddp	st1
    loop	lpo
    fxch
    popone
    mov dl,1
    jmp wrapmath
_p1evl:
    lea	eax,[nm1]
    call clearmath
    mov	eax,[esp+12]
    mov	ecx,[esp+16]
    fld	qword [esp+4]
    fld	st0
    jmp	intoo
lp1:
    fmul	st0,st1
intoo:
    fld		qword [eax]
    add		eax,8
    faddp	st1
    loop	lp1
    fxch
    popone
    mov dl,1
    jmp wrapmath
