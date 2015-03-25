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
%ifdef __BUILDING_LSCRTL_DLL
[export ___substackp]
%endif
[global ___substackp]
[global __alloca_probe]
[global __alloca_probe_8]
[global __alloca_probe_16]
SECTION code CLASS=CODE USE32
__alloca_probe_8:
__alloca_probe_16: ; these work because __substackp does a paragraph align already
__alloca_probe:
    xchg    dword [esp],eax
    push    eax
___substackp:
    push    eax
    push    ecx
    mov     ecx, [esp + 12]
    or      ecx,ecx
    jns     down
    neg     ecx
    add		ecx,15
    and		ecx,0fffffff0h	
    mov     eax, [esp + 8]
    mov     [esp + ecx + 8],eax
    mov     eax, [esp + 4]
    mov     [esp + ecx+ 4],eax
    mov     eax, [esp]
    mov     [esp + ecx],eax
    add     esp, ecx
    pop     ecx
    pop     eax
    ret     4
down:
    add		ecx,15
    and		ecx,0fffffff0h	
    mov     eax,ecx
lp:
    cmp		ecx,4096
    jc		fin
    push	ecx
    sub		esp,4092
    sub		ecx,4096
    jmp		lp
fin:
    or		ecx,ecx
    jz		done
    push	ecx
    sub		ecx,4
    sub		esp,ecx
done:
    mov     ecx,eax
    mov     eax,[esp+ecx]
    mov     [esp],eax
    mov     eax,[esp+ecx+4]
    mov     [esp+4],eax
    mov     eax,[esp+ecx+8]
    mov     [esp+8],eax
    pop     ecx
    pop     eax
    ret     4
