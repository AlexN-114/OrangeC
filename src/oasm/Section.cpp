/*
    Software License Agreement (BSD License)
    
    Copyright (c) 1997-2011, David Lindauer, (LADSoft).
    All rights reserved.
    
    Redistribution and use of this software in source and binary forms, 
    with or without modification, are permitted provided that the following 
    conditions are met:
    
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
    
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
    PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
    OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
    OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
    ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    contact information:
        email: TouchStone222@runbox.com <David Lindauer>
*/
#include "ObjFactory.h"
#include "ObjSection.h"
#include "ObjMemory.h"
#include "ObjSymbol.h"
#include "Section.h"
#include "Label.h"
#include "Fixup.h"
#include "AsmFile.h"

#include <exception>
#include <limits.h>
#include <fstream>
#include <iostream>

Section::~Section()
{
    for (int i=0; i < instructions.size(); i++)
    {
        Instruction *s = instructions[i];
        delete s;
    }
}
void Section::Parse(AsmFile *fil)
{
    while (!fil->AtEof() && fil->GetKeyword() != Lexer::closebr)
    {
        if (fil->GetKeyword() == Lexer::ALIGN)
        {
            fil->NextToken();
            if (fil->GetKeyword() != Lexer::assn)
                throw new std::runtime_error("Expected '='");
            fil->NextToken();
            if (!fil->IsNumber())
                throw new std::runtime_error("Expected alignment value");
            align = fil->GetValue();
        }
        else if (fil->GetKeyword() == Lexer::CLASS)
        {
            fil->NextToken();
            if (fil->GetKeyword() != Lexer::assn)
                throw new std::runtime_error("Expected '='");
            fil->NextToken();
            fil->GetId();
        }
        else if (fil->GetKeyword() == Lexer::VIRTUAL)
        {
            Section *old = fil->GetCurrentSection();
            if (!old)
                throw new std::runtime_error("Virtual section must be enclosed in other section");
            align = old->align;
            memcpy(beValues, old->beValues, sizeof(beValues));
            isVirtual = true;
            fil->NextToken();
        }
        else if (fil->GetKeyword() == Lexer::STACK)
        {
            fil->NextToken();
        }
        else if (!fil->GetParser()->ParseSection(fil, this))
            throw new std::runtime_error("Invalid section qualifier");
    }
}
void Section::Optimize(AsmFile *fil)
{
    AsmExpr::SetSection(this);
    bool done = false;
    while (!done)
    {
        int pc = 0;
        done = true;
        for (int i=0; i < instructions.size(); i++)
        {
            if (instructions[i]->IsLabel())
            {
                Label *l = instructions[i]->GetLabel();
                if (l)
                {
                    l->SetOffset(pc);
                    labels[l->GetName()] = pc;
                }
            }
            else
            {
                int n = instructions[i]->GetSize() ;
                instructions[i]->SetOffset(pc);
                instructions[i]->Optimize(pc, false);
                int m = instructions[i]->GetSize() ;
                pc += m;
                if (n != m)
                    done = false;
            }
        }
    }
    int pc = 0;
    for (int i=0; i < instructions.size(); i++)
    {
        instructions[i]->Optimize(pc, true);
        pc += instructions[i]->GetSize();
    }
}
void Section::Resolve(AsmFile *fil)
{
    Optimize(fil);
}
ObjSection *Section::CreateObject(ObjFactory &factory)
{
    objectSection = factory.MakeSection(name);
    if (isVirtual)
        objectSection->SetQuals(objectSection->GetQuals() | ObjSection::equal);
    return objectSection;
}
ObjExpression *Section::ConvertExpression(AsmExprNode *node, AsmFile *fil, ObjFactory &factory)
{
    ObjExpression *xleft = NULL;
    ObjExpression *xright = NULL;
    if (node->GetLeft())
        xleft = ConvertExpression(node->GetLeft(), fil, factory);
    if (node->GetRight())
        xright = ConvertExpression(node->GetRight(), fil, factory);
    switch (node->GetType())
    {
        case AsmExprNode::IVAL:
            return factory.MakeExpression(node->ival);
        case AsmExprNode::FVAL:
            throw new std::runtime_error("Floating point in relocatable expression not allowed");
        case AsmExprNode::PC:
            return factory.MakeExpression(ObjExpression::ePC);
        case AsmExprNode::SECTBASE:
            return factory.MakeExpression(objectSection);
        case AsmExprNode::BASED:
        {
            ObjExpression *left = factory.MakeExpression(node->GetSection()->GetObjectSection());
            ObjExpression *right = factory.MakeExpression(node->ival);
            return factory.MakeExpression(ObjExpression::eAdd, left, right);
        }
            break;
        case AsmExprNode::LABEL:
        {
            AsmExprNode *num = AsmExpr::GetEqu(node->label);
            if (num)
            {
                return ConvertExpression(num, fil, factory);
            }
            else
            {
                Label * label = fil->Lookup(node->label);
                if (label != NULL)
                {
                    
                    ObjExpression *t;
                    if (label->IsExtern())
                    {
                        t = factory.MakeExpression(label->GetObjSymbol());
                    }
                    else
                    {
                        ObjExpression *left = factory.MakeExpression(label->GetObjectSection());
                        ObjExpression *right = ConvertExpression(label->GetOffset(), fil, factory);
                        t = factory.MakeExpression(ObjExpression::eAdd, left, right);
                    }
                    return t;
                }
                else
                {
                    ObjSection *s = fil->GetSectionByName(node->label);
                    if (s)
                    {
                        ObjExpression *left = factory.MakeExpression(s);
                        ObjExpression *right = factory.MakeExpression(16);
                        return factory.MakeExpression(ObjExpression::eDiv, left, right);
                    }
                    else
                    {
                        std::string name = node->label;
                        if (name.substr(0,3) == "..@" && isdigit(name[3]))
                        {
                            int i;
                            for ( i = 4; i < name.size() && isdigit(name[i]); i++);
                            if (name[i] == '.')
                            {
                                name = std::string("%") + name.substr(i+1);
                            }
                            else if (name[i] == '@')
                            {
                                name = std::string("%$") + name.substr(i+1);
                            }
                        }
                        throw new std::runtime_error(std::string("Label '") + name + "' does not exist.");
                    }
                }
            }
        }
        case AsmExprNode::ADD:
            return factory.MakeExpression(ObjExpression::eAdd, xleft, xright);
        case AsmExprNode::SUB:
            return factory.MakeExpression(ObjExpression::eSub, xleft, xright);
        case AsmExprNode::NEG:
            return factory.MakeExpression(ObjExpression::eNeg, xleft, xright);
        case AsmExprNode::CMPL:
            return factory.MakeExpression(ObjExpression::eCmpl, xleft, xright);
        case AsmExprNode::MUL:
            return factory.MakeExpression(ObjExpression::eMul, xleft, xright);
        case AsmExprNode::DIV:
            return factory.MakeExpression(ObjExpression::eDiv, xleft, xright);
        case AsmExprNode::OR:
        case AsmExprNode::XOR:
        case AsmExprNode::AND:
        case AsmExprNode::GT:
        case AsmExprNode::LT:
        case AsmExprNode::GE:
        case AsmExprNode::LE:
        case AsmExprNode::EQ:
        case AsmExprNode::NE:
        case AsmExprNode::MOD:
        case AsmExprNode::LSHIFT:
        case AsmExprNode::RSHIFT:
        case AsmExprNode::LAND:
        case AsmExprNode::LOR:
            throw new std::runtime_error("Operator not allowed in address expression");
    }
}
bool Section::SwapSectionIntoPlace(ObjExpression *t)
{
    ObjExpression *left = t->GetLeft();
    ObjExpression *right = t->GetRight();
    if (t->GetOperator() == ObjExpression::eSub || t->GetOperator() == ObjExpression::eDiv
            || (left && !right))
    {
        return SwapSectionIntoPlace(left);
    }
    else if (left && right)
    {
        bool n1 = SwapSectionIntoPlace(left);
        bool n2 = SwapSectionIntoPlace(right);
        if (n2 && !n1)
        {
            t->SetLeft(right);
            t->SetRight(left);
        }
        return n1 || n2;
    }
    else 
    {
        return t->GetOperator() == ObjExpression::eSection;
    }
}
bool Section::MakeData(ObjFactory &factory, AsmFile *fil)
{
    bool rv = true;
    int pc = 0;
    int pos = 0;
    unsigned char buf[1024];
    Fixup f;
    int n;
    ObjSection *sect = objectSection;
    if (sect)
    {
        sect->SetAlignment(align);
        instructionPos = 0;
        while ((n = GetNext(f, buf+pos, sizeof(buf) - pos)) != 0)
        {
            if (n > 0)
            {
                pos += n;
                if (pos == sizeof(buf))
                {
                    ObjMemory *mem = factory.MakeData(buf, pos);
                    sect->Add(mem);
                    pos = 0;
                }
                pc += n;
            }
            else
            {
                if (pos)
                {
                    ObjMemory *mem = factory.MakeData(buf, pos);
                    sect->Add(mem);
                    pos = 0;
                }
                ObjExpression *t;
                try
                {
                    t = ConvertExpression(f.GetExpr(), fil, factory);
                    SwapSectionIntoPlace(t);
                }
                catch (std::runtime_error *e)
                {
                    Errors::IncrementCount();
                    std::cout << "Error " << f.GetFileName() << "(" << f.GetErrorLine() << "):" << e->what() << std::endl;
                    delete e;
                    t = NULL;
                    rv = false;
                }
                if (t && f.IsRel())
                {
                    ObjExpression *left = factory.MakeExpression(f.GetRelOffs());
                    t = factory.MakeExpression(ObjExpression::eSub, t, left);
                    left = factory.MakeExpression(ObjExpression::ePC);
                    t = factory.MakeExpression(ObjExpression::eSub, t, left);
                }
                if (t)
                {
                
                    ObjMemory *mem = factory.MakeFixup(t, f.GetSize());
                    if (mem)
                        sect->Add(mem);
                }
                pc += f.GetSize();
            }
        }
        if (pos)
        {
            ObjMemory *mem = factory.MakeData(buf, pos);
            sect->Add(mem);
        }
    }
    return rv;
}
int Section::GetNext(Fixup &f, unsigned char *buf, int len)
{
    static int blen = 0;
    char buf2[256];
    if (!blen)
    {
        while (instructionPos < instructions.size() && (blen = instructions[instructionPos]->GetNext(f, (unsigned char *)&buf2[0])) == 0)
            instructionPos ++;
        if (instructionPos >= instructions.size())
            return 0;
    }
    if (blen == -1)
    {
        blen = 0;
        return -1;
    }
    if (blen <= len)
    {
        memcpy(buf, buf2, blen);
        int rv = blen;
        blen = 0;
        return rv;
    }
    else
    {
        memcpy(buf, buf2, len);
        memcpy(buf2, buf2+len, blen- len);
        blen -= len;
        return len;
    }
} 
