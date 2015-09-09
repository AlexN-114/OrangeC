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
[export _strcpy]
%endif
[global _strcpy]
[global strcat_fin]

SECTION code CLASS=CODE USE32

_strcpy:
		mov	ecx,[esp+4]	;	str1
strcat_fin:
		mov	ah,3
		mov	edx,[esp+8]	;  str2
TestEdx:
		test	dl,ah
		jnz	DoAlign

		;
		; do multiple of 4 bytes at a time
		;
QuadLoop:
		mov	eax,[edx]	;	Load the bytes
		add	edx,4		;	Update pointer
		test	al,al	;	Look for null
		jz	CpyDoneLoLo
		test	ah,ah
		jz	CpyDoneLoHi
		test eax,0xFF0000
		jz	CpyDoneHiLo
		mov	[ecx],eax
		add	ecx,4		;	(or 4 if misaligned)
		test eax,0xFF000000
		jnz	QuadLoop
    
CpyDone:
		mov	eax,[esp+4]
		ret

CpyDoneLoLo:
		mov	[ecx],al	; this is a zero
		mov	eax,[esp+4]
		ret

CpyDoneLoHi:
		xor	dl,dl
		mov	[ecx],al
		mov	[ecx+1],dl
		mov	eax,[esp+4]
		ret

CpyDoneHiLo:
		mov	[ecx],al
		xor	dl,dl
		mov	[ecx+1],ah
		mov	eax,[esp+4]
		mov	[ecx+2],dl
		ret

DoAlign:
		mov	al,[edx]
		inc	edx
		mov	[ecx],al
		inc	ecx
		test	al,al
		jnz	TestEdx
		mov	eax,[esp+4]
		ret
