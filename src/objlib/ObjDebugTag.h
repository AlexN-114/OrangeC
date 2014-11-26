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
#ifndef OBJDEBUGTAG_H
#define OBJDEBUGTAG_H
#include <cstdlib>
#include "ObjTypes.h"

class ObjLineNo;
class ObjSymbol;
class ObjFunction;

class ObjDebugTag : public ObjWrapper
{
public:
    enum eType
    {
        eVar,
        eLineNo,
        eBlockStart,
        eBlockEnd,
        eFunctionStart,
        eFunctionEnd
    } ;
    ObjDebugTag(ObjLineNo *LineNo) : type(eLineNo), lineNo(LineNo) {} ;
    ObjDebugTag(ObjSymbol *Symbol) : type(eVar), symbol(Symbol) {} ;
    ObjDebugTag(ObjSymbol *Symbol, bool Start) 
            : type (Start ? eFunctionStart : eFunctionEnd) , symbol(Symbol) {}
    ObjDebugTag(bool Start) : type(Start ? eBlockStart : eBlockEnd), lineNo(NULL) {} ;
    virtual ~ObjDebugTag() { }	
    eType GetType() { return type; }
    void SetType(eType Type) { type = Type; }
    ObjLineNo *GetLineNo()
    {
        return type == eLineNo ? lineNo : NULL;
    }
    ObjSymbol *GetSymbol()
    {
        return symbol;
    }
private:
    enum eType type;
    union {
        ObjLineNo *lineNo;
        ObjSymbol *symbol;
    } ;
} ;
#endif