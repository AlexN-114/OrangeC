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
[export @__lambdaCallS$qpv$]
[export @__lambdaPtrCallS$qpvpv$]
%endif
[global @__lambdaCallS$qpv$]
[global @__lambdaPtrCallS$qpvpv$]

; frame in
;    args
;    this
;    struct ptr ; discarded on return
;    rv1
;    function ; discarded on return
;    rv2
;
; frame out
;     args
;     rv2
;     rv1
;     this
;    struct ptr
SECTION code CLASS=CODE USE32



@__lambdaCallS$qpv$:
    pop ecx
    xchg ecx,[esp + 12]
    xchg ecx,[esp+4]
    xchg ecx,[esp+8]
    xchg ecx,[esp]
    call ecx
    mov ecx,[esp]
    xchg ecx,[esp + 8];
    xchg ecx,[esp + 4]
    xchg ecx,[esp + 12]
    jmp ecx

; frame in
;    args
;    struct ptr
;    rv1
;    function ; discarded on return
;    this
;    rv2
;
; frame out
;     args
;     rv1
;     rv2
;     this
;    struct ptr
@__lambdaPtrCallS$qpvpv$:
    pop ecx
    xchg ecx,[esp + 12]
    xchg ecx,[esp + 0]
    xchg ecx,[esp + 4]
    call ecx
    xchg ecx,[esp + 4]
    xchg ecx,[esp + 0]
    xchg ecx,[esp + 12]
    jmp ecx
    