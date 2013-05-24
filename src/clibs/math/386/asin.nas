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
[export _asin]
[export _asinf]
[export _asinl]
%endif
[global _asin]
[global _asinf]
[global _asinl]
[global lasin]
SECTION data CLASS=DATA USE32
nm	db	"asin",0

SECTION code CLASS=CODE USE32
_asinf:
    lea	ecx,[esp+4]
    fld	dword[ecx]
    sub dl,dl
    jmp short asin
_asinl:
    lea	ecx,[esp+4]
    fld	tword[ecx]
    mov dl,2
    jmp short asin
_asin:
    lea	ecx,[esp+4]
    fld	qword[ecx]
    mov dl,1
asin:
    lea eax,[nm]
    call    clearmath
lasin:
    fld1
    fcomp st1
    fstsw ax
    sahf
    jb domainerr
    je retpi2
    fld1
    fchs
    fcomp st1
    fstsw ax
    sahf
    ja domainerr
    je retminpi2
    fld	st0
    fld st0
    fmulp	st1
    fld1
    fxch 	st1
    fsubp	st1,st0
    fsqrt
    fdivp	st1,st0
    fld1
    fpatan
    jmp 	wrapmath
retminpi2:
    popone
    fld1
    fchs
    fldpi
    fchs
    fscale ; won't overflow
    fxch
    popone
    jmp wrapmath
retpi2:
    popone
    fld1
    fchs
    fldpi
    fscale ; won't overflow
    fxch
    popone
    jmp wrapmath