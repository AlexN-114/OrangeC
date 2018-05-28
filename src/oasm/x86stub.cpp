/* Software License Agreement
 * 
 *     Copyright(C) 1994-2018 David Lindauer, (LADSoft)
 * 
 *     This file is part of the Orange C Compiler package.
 * 
 *     The Orange C Compiler package is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version, with the addition of the 
 *     Orange C "Target Code" exception.
 * 
 *     The Orange C Compiler package is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with Orange C.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *     contact information:
 *         email: TouchStone222@runbox.com <David Lindauer>
 * 
 */

#include "Instruction.h"
#include "InstructionParser.h"
#include "x86Operand.h"
#include "x86Parser.h"
#include "AsmFile.h"
#include "Section.h"
#include "Fixup.h"
#include "AsmLexer.h"
#include <fstream>
#include <iostream>
char *Lexer::preData = 
"%define __SECT__\n"
"%imacro	data1	0+ .native\n"
"	[db %1]\n"
"%endmacro\n"
"%imacro	db	0+ .native\n"
"	[db %1]\n"
"%endmacro\n"
"%imacro	resb 0+ .native\n"
"	[resb %1]\n"
"%endmacro\n"
"%imacro	dw	0+ .native\n"
"	[dw %1]\n"
"%endmacro\n"
"%imacro	resw 0+ .native\n"
"	[resw %1]\n"
"%endmacro\n"
"%imacro	dd	0+ .native\n"
"	[dd %1]\n"
"%endmacro\n"
"%imacro	resd 0+ .native\n"
"	[resd %1]\n"
"%endmacro\n"
"%imacro	dq	0+ .native\n"
"	[dq %1]\n"
"%endmacro\n"
"%imacro	resq 0+ .native\n"
"	[resq %1]\n"
"%endmacro\n"
"%imacro	dt	0+ .native\n"
"	[dt %1]\n"
"%endmacro\n"
"%imacro	rest 0+ .native\n"
"	[rest %1]\n"
"%endmacro\n"
"%imacro	section 0+ .nolist\n"
"%define __SECT__ [section %1]\n"
"__SECT__\n"
"%endmacro\n"
"%imacro	segment 0+ .nolist\n"
"%define __SECT__ [section %1]\n"
"__SECT__\n"
"%endmacro\n"
"%imacro	absolute 0+ .nolist\n"
"%define __SECT__ [absolute %1]\n"
"__SECT__\n"
"%endmacro\n"
"%imacro	align 0+ .nolist\n"
"	[align %1]\n"
"%endmacro\n"
"%imacro	import 0+ .nolist\n"
"	[import %1]\n"
"%endmacro\n"
"%imacro	export 0+ .nolist\n"
"	[export %1]\n"
"%endmacro\n"
"%imacro	global 0+ .nolist\n"
"	[global %1]\n"
"%endmacro\n"
"%imacro	extern 0+ .nolist\n"
"	[extern %1]\n"
"%endmacro\n"
"%imacro equ 0+ .native\n"
"	[equ	%1]\n"
"%endmacro\n"
"%imacro incbin 0+ .nolist\n"
"	[incbin	%1]\n"
"%endmacro\n"
"%imacro times 0+ .native\n"
"	[times %1]\n"
"%endmacro\n"
"%imacro struc 1 .nolist\n"
"%push struc\n"
"%define %$name %1\n"
"[absolute 0]\n"
"%{$name}:\n"
"%endmacro\n"
"%imacro endstruc 0 .nolist\n"
"%{$name}_SIZE EQU $-%$name\n"
"%pop\n"
"__SECT__\n"
"%endmacro\n"
;

void Instruction::Optimize(int pc, bool last)
{
    if (data && size >= 3 && data[0] == 0x0f && data[1] == 0x0f && (data[2] == 0x9a || data[2] == 0xea))
    {
        if (fixups.size() != 1)
        {
            std::cout << "Far branch cannot be resolved" << std::endl;
            memcpy(data, data+2, size -2);
            size -= 2;
        }
        else
        {
            Fixup *f = fixups[0];
            AsmExprNode *expr = AsmExpr::Eval(f->GetExpr(), pc);
            if (expr->IsAbsolute())
            {
                memcpy(data, data+1, size -1);
                size -= 3;
                f->SetInsOffs(f->GetInsOffs() - 1);
                data[0] = 0x0e; // push cs
                int n = data[f->GetInsOffs() - 1];
                if (n == 0xea) // far jmp
                    n = 0xe9; // jmp
                else 
                    n = 0xe8; // call
                data[f->GetInsOffs()-1] = n;
                f->SetRel(true);
                f->SetRelOffs(f->GetSize());
                f->SetAdjustable(true);
            }
            else
            {
                memcpy(data, data+2, size -2);
                size -= 2;
                f->SetInsOffs(f->GetInsOffs() - 2);
                AsmExprNode *n = new AsmExprNode(AsmExprNode::DIV, f->GetExpr(), new AsmExprNode(16));
                f = new Fixup(n, 2, false);
                f->SetInsOffs(size-2);
                fixups.push_back(f);
            }
            delete expr;
        }
    }
    if (type == CODE || type == DATA || type == RESERVE)
    {
        for (auto fixup : fixups)
        {
            AsmExprNode *expr = fixup->GetExpr();
            expr = AsmExpr::Eval(expr, pc);
            if (fixup->GetExpr()->IsAbsolute() || (fixup->IsRel() && expr->GetType() == AsmExprNode::IVAL))
            {
                int n = fixup->GetSize() * 8;
                int p = fixup->GetInsOffs();
                if (fixup->GetSize() < 4 || (expr->GetType() == AsmExprNode::IVAL && fixup->GetSize() == 4))
                {
                    int o;
                    bool error = false;
                    if (expr->GetType() == AsmExprNode::IVAL)
                        o = expr->ival;
                    else
                        o = (L_INT)expr->fval;
                    if (fixup->IsRel())
                    {
                        o -= size + pc;
                        if (fixup->IsAdjustable())
                        {
                            if (type == CODE)
                            {
                                if (p >= 1 && data[p-1] == 0xe9)
                                {
                                    if (o <= 127 && o >= -128 - ((n/8) - 1))
                                    {
                                        data[p-1] = 0xeb;
                                        size -= fixup->GetSize() - 1;
                                        fixup->SetSize(1);
                                        fixup->SetRelOffs(1);
                                        n = 8;
                                    }
                                }
                                else if (p >= 2 && ((data[p-1] & 0xf0) == 0x80) && data[p-2] == 0x0f)
                                {
                                    if (o <= 127 && o >= -128 - (n/8))
                                    {
                                        data[p-2] = data[p-1] - 0x80 + 0x70;
                                        size -= fixup->GetSize();
                                        fixup->SetInsOffs(p = fixup->GetInsOffs()-1);
                                        fixup->SetSize(1);
                                        fixup->SetRelOffs(1);
                                        n = 8;
                                    }
                                }
                            }
                        }
                        int t = o >> (n-1);
                        if (t != 0 && t != -1)
                        {
                            error = true;
                            if (!fixup->IsResolved())
                            {
                                if (last)
                                {
                                    Errors::IncrementCount();
                                    std::cout << "Error " << fixup->GetFileName().c_str() << "(" << fixup->GetErrorLine() << "):" << "Branch out of range" << std::endl;
                                }
                            }
                        }
                    }
                    else
                    {
                        if (n < 32)
                        {
                            int t = o >> (n);
                            if (t != 0 && t != -1)
                            {
                                error = true;
                                if (!fixup->IsResolved())
                                {
                                    if (last)
                                    {
                                        Errors::IncrementCount();
                                        std::cout << "Error " << fixup->GetFileName().c_str() << "(" << fixup->GetErrorLine() << "):" << "Value out of range" << std::endl;
                                    }
                                }
                            }
                        }
                    }
                    if (!error)
                        fixup->SetResolved();
                    if (bigEndian)
                    {
                        // never get here on an x86
                        for (int i=n-8; i >=0; i -= 8)
                            data[p + i/8] = o >> i;
                    }
                    else
                    {
                        for (int i=0; i < n; i += 8)
                            data[p + i/8] = o >> i;
                    }
                }
                else if (expr->GetType() == AsmExprNode::IVAL || expr->GetType() == AsmExprNode::FVAL)
                {
                    FPF o;
                    fixup->SetResolved();
                    if (expr->GetType() == AsmExprNode::FVAL)
                        o = expr->fval;
                    else
                        o = expr->ival;
                    switch(n)
                    {
                        case 32:
                            o.ToFloat(data + p);
                            break;
                        case 64:
                            o.ToDouble(data + p);
                            break;
                        case 80:
                            o.ToLongDouble(data + p);
                            break;
                    }
                }
            }
            delete expr;
        }
    }
}
void x86Parser::Setup(Section *sect)
{
    Setseg32(sect->beValues[0]);
}
bool x86Parser::ParseSection(AsmFile *fil, Section *sect)
{
    bool rv = false;
    if (fil->GetKeyword() == Lexer::USE16)
    {
        sect->beValues[0] = 0;
        fil->NextToken();
        rv = true;
    }
    else if (fil->GetKeyword() == Lexer::USE32)
    {
        sect->beValues[0] = 1;
        fil->NextToken();
        rv = true;
    }
    return rv;
}
