; Software License Agreement
; 
;     Copyright(C) 1994-2018 David Lindauer, (LADSoft)
; 
;     This file is part of the Orange C Compiler package.
; 
;     The Orange C Compiler package is free software: you can redistribute it and/or modify
;     it under the terms of the GNU General Public License as published by
;     the Free Software Foundation, either version 3 of the License, or
;     (at your option) any later version, with the addition of the 
;     Orange C "Target Code" exception.
; 
;     The Orange C Compiler package is distributed in the hope that it will be useful,
;     but WITHOUT ANY WARRANTY; without even the implied warranty of
;     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;     GNU General Public License for more details.
; 
;     You should have received a copy of the GNU General Public License
;     along with Orange C.  If not, see <http://www.gnu.org/licenses/>.
; 
;     contact information:
;         email: TouchStone222@runbox.com <David Lindauer>
; 

%include "matherr.inc"

%ifdef __BUILDING_LSCRTL_DLL
[export _log2]
[export _log2f]
[export _log2l]
%endif
[global _log2]
[global _log2f]
[global _log2l]
SECTION data CLASS=DATA USE32

nm	db	"log2",0

SECTION code CLASS=CODE USE32

_log2f:
    lea	ecx,[esp+4]
    fld	dword[ecx]
    sub dl,dl
    jmp short log2
_log2l:
    lea	ecx,[esp+4]
    fld	tword[ecx]
    mov dl,2
    jmp short log2
_log2:
    lea	ecx,[esp+4]
    fld	qword[ecx]
    mov dl,1
log2:
    lea	eax,[nm]
    call clearmath
    call mnegeerr
    jc xit
    fld1
    fxch
    fyl2x
    call wrapmath
xit:
    ret
