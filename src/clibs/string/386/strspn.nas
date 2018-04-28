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
;     along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
; 
;     contact information:
;         email: TouchStone222@runbox.com <David Lindauer>

%ifdef __BUILDING_LSCRTL_DLL
[export _strspn]
%endif
[global _strspn]
SECTION code CLASS=CODE USE32

_strspn:
    push	ebx
        mov     ecx,[esp+8]
    sub	eax,eax
    dec eax
lp:
    inc	eax
    mov	bl,[ecx]
    or	bl,bl
    je	exit
    mov     edx,[esp+12]
    dec	edx
    inc	ecx
lp1:
    inc	edx
    cmp byte [edx], BYTE 0
    je	exit
    cmp	bl,[edx]
    jne	lp1
    jmp	lp
exit:
    pop	ebx
    ret
