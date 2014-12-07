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
#include "ObjIeee.h"
#include <stdio.h>
#include <deque>

char ObjIeeeAscii::lineend[2] = { 10 };


inline int min(int x,int y)
{
    if (x < y) 
        return x;
    else
        return y;
}
void ObjIeeeAscii::bufferup(const char *data, int len)
{
    if (len + ioBufferLen > BUFFERSIZE)
    {
        flush();
    }
    if (len + ioBufferLen > BUFFERSIZE)
    {
        fwrite(data, len, 1, sfile);
    }
    else
    {
        memcpy(ioBuffer + ioBufferLen, data, len);
        ioBufferLen += len;
    }
}
ObjString ObjIeeeAscii::GetSymbolName(ObjSymbol *Symbol)
{
    ObjString name;
    switch (Symbol->GetType())
    {
        case ObjSymbol::ePublic:
        default:
            name = "I";
            break;
        case ObjSymbol::eExternal:
            name = "X";
            break;
        case ObjSymbol::eLocal:
            name = "N";
            break;
        case ObjSymbol::eAuto:
            name = "A";
            break;
        case ObjSymbol::eReg:
            name = "E";
            break;
        
    }
    name = name + ObjUtil::ToHex(Symbol->GetIndex());
    return name;
}
ObjString ObjIeeeAscii::ToString(const ObjString strng)
{
    return ObjUtil::ToHex(strng.length(), 3) + strng;
}
ObjString ObjIeeeAscii::ToTime(std::tm tms)
{
    ObjString rv ;
    rv = ObjUtil::ToDecimal(tms.tm_year + 1900,4) +
                ObjUtil::ToDecimal(tms.tm_mon + 1,2) +
                ObjUtil::ToDecimal(tms.tm_mday,2) +
                ObjUtil::ToDecimal(tms.tm_hour,2) +
                ObjUtil::ToDecimal(tms.tm_min,2) +
                ObjUtil::ToDecimal(tms.tm_sec,2) ;
    return rv;
}
void ObjIeeeAscii::RenderFile(ObjSourceFile *File)
{
    ObjString data(ObjUtil::ToDecimal(File->GetIndex())+ "," + ToString(File->GetName()) + "," + ToTime(File->GetFileTime()));
    RenderComment(eSourceFile, data);
}
ObjString ObjIeeeAscii::GetTypeIndex(ObjType *Type)
{
    if (Type->GetType() < ObjType::eVoid)
        return ObjUtil::ToHex(Type->GetIndex());
    else
        return ObjUtil::ToHex((int)Type->GetType());
}
void ObjIeeeAscii::RenderStructure(ObjType *Type)
{
    const int MaxPerLine = 15;
    std::deque<ObjField *> fields;
    for (ObjType::FieldIterator it = Type->FieldBegin(); it != Type->FieldEnd(); ++it)
    {
        fields.push_front(*it);
    }
    ObjString lastIndex;
    while (fields.size())
    {
        unsigned n = min(MaxPerLine, (int)fields.size());
        ObjString index;
        ObjString baseType;
        // the problem with this is if they output the file twice
        // the types won't match
        if (n == fields.size())
        {
            index = ObjUtil::ToHex(Type->GetIndex());
            baseType = ObjUtil::ToHex(Type->GetType());
            RenderString("ATT" + index + ",T" + baseType + "," + ObjUtil::ToHex(Type->GetSize()));
        }
        else
        {
            index = ObjUtil::ToHex(GetFactory()->GetIndexManager()->NextType());
            baseType = ObjUtil::ToHex(ObjType::eField);
            RenderString("ATT" + index + ",T" + baseType);
        }
        for (unsigned j=0; j < n; j++)
        {
            ObjField *currentField = fields[n-1-j];
            RenderString(",T" + GetTypeIndex(currentField->GetBase()) + ",");
            RenderString(ToString(currentField->GetName()));
            RenderString("," + ObjUtil::ToHex(currentField->GetConstVal()));
        }
        RenderString(lastIndex + ".");
        endl();
        lastIndex = ",T" + index;
        for (unsigned j=0; j < n; j++)
            fields.pop_front();
    }
}
void ObjIeeeAscii::RenderFunction(ObjFunction *Function)
{
    RenderString("ATT" + GetTypeIndex(static_cast<ObjType *>(Function)) + ",");
    RenderString("T"  + ObjUtil::ToHex(ObjType::eFunction) + ",");
    RenderString("T" + GetTypeIndex(Function->GetReturnType()) + ",");
    RenderString(ObjUtil::ToHex(Function->GetLinkage()));
    // assuming a reasonable number of parameters
    // parameters are TYPES
    for (ObjFunction::ParameterIterator it = Function->ParameterBegin();
         it != Function->ParameterEnd(); ++it)
    {
        RenderString(",T" + GetTypeIndex(*it));
    }
    RenderCstr(".");
    endl();
}
void ObjIeeeAscii::RenderType(ObjType *Type)
{
    if (Type->GetType() < ObjType::eVoid && Type->GetName().size())
    {
        RenderString("NT" + ObjUtil::ToHex(Type->GetIndex()));
        RenderString("," + ToString(Type->GetName()) + ".");
        endl();
    }
    switch(Type->GetType())
    {
        case ObjType::ePointer:
            RenderString("ATT" + GetTypeIndex(Type));
            RenderString(",T" + ObjUtil::ToHex(ObjType::ePointer));
            RenderString("," + ObjUtil::ToHex(Type->GetSize()));
            RenderString(",T" + GetTypeIndex(Type->GetBaseType()) + ".");
            endl();
            break;
        case ObjType::eTypeDef:
            RenderString("ATT" + GetTypeIndex(Type));
            RenderString(",T" + ObjUtil::ToHex(ObjType::eTypeDef));
            RenderString(",T" + GetTypeIndex(Type->GetBaseType()) + ".");
            endl();
            break;
        case ObjType::eFunction:
            RenderFunction(static_cast<ObjFunction *>(Type));
            break;
        case ObjType::eStruct:
        case ObjType::eUnion:
        case ObjType::eEnum:
            RenderStructure(Type);
            break;
        case ObjType::eBitField:
            RenderString("ATT" + GetTypeIndex(Type));
            RenderString(",T" + ObjUtil::ToHex((int)ObjType::eBitField));
            RenderString("," + ObjUtil::ToHex(Type->GetSize()));
            RenderString(",T" + GetTypeIndex(Type->GetBaseType()));
            RenderString("," + ObjUtil::ToHex(Type->GetStartBit()));
            RenderString("," + ObjUtil::ToHex(Type->GetBitCount()) + ".");
            endl();
            break;
        case ObjType::eArray:
            RenderString("ATT" + GetTypeIndex(Type));
            RenderString(",T" + ObjUtil::ToHex((int)Type->GetType()));
            RenderString("," + ObjUtil::ToHex(Type->GetSize()));
            RenderString(",T" + GetTypeIndex(Type->GetBaseType()));
            RenderString(",T" + GetTypeIndex(Type->GetIndexType()));
            RenderString("," + ObjUtil::ToHex(Type->GetBase()));
            RenderString("," + ObjUtil::ToHex(Type->GetTop()) + ".");
            endl();
            break;
        case ObjType::eVla:
            RenderString("ATT" + GetTypeIndex(Type));
            RenderString(",T" + ObjUtil::ToHex((int)Type->GetType()));
            RenderString("," + ObjUtil::ToHex(Type->GetSize()));
            RenderString(",T" + GetTypeIndex(Type->GetBaseType()));
            RenderString(",T" + GetTypeIndex(Type->GetIndexType()));
            endl();
            break;
        default:
            break;
    }
}
void ObjIeeeAscii::RenderSymbol(ObjSymbol *Symbol)
{
    if (Symbol->GetType() == ObjSymbol::eDefinition)
    {
        ObjDefinitionSymbol *dsym = static_cast<ObjDefinitionSymbol *>(Symbol);
        ObjString data = ToString(dsym->GetName()) + "," + ObjUtil::ToDecimal(dsym->GetValue());
        RenderComment(eDefinition, data);
    }
    else if (Symbol->GetType() == ObjSymbol::eImport)
    {
        ObjString data;
        ObjImportSymbol *isym = static_cast<ObjImportSymbol *>(Symbol);
        if (isym->GetByOrdinal())
            data = "O," + ToString(isym->GetName()) + "," + ObjUtil::ToDecimal(isym->GetOrdinal()) + "," + ToString(isym->GetDllName());
        else
            data = "N," + ToString(isym->GetName()) + "," + ToString(isym->GetExternalName()) + "," + ToString(isym->GetDllName());
        RenderComment(eImport, data);
    }
    else if (Symbol->GetType() == ObjSymbol::eExport)
    {
        ObjString data;
        ObjExportSymbol *esym = static_cast<ObjExportSymbol *>(Symbol);
        if (esym->GetByOrdinal())
            data = "O," + ToString(esym->GetName()) + "," + ObjUtil::ToDecimal(esym->GetOrdinal());
        else
            data = "N," + ToString(esym->GetName()) + "," + ToString(esym->GetExternalName());
        if (esym->GetDllName().size())
            data = data + "," + esym->GetDllName();
        RenderComment(eExport, data);
    }
    else if (Symbol->GetType() != ObjSymbol::eLabel)
    {
        ObjString name = GetSymbolName(Symbol);
        RenderString("N" + name + "," + ToString(Symbol->GetName()) + ".");
        endl();
        if (Symbol->GetOffset())
        {
            RenderString("AS" + name + ",");
            RenderExpression(Symbol->GetOffset());
            RenderCstr(".");
            endl();
        }
        if (GetDebugInfoFlag() && Symbol->GetBaseType())
        {
            RenderString("AT" + name + ",T" + GetTypeIndex(Symbol->GetBaseType()) + ".");
            endl();
        }
    }
}
void ObjIeeeAscii::RenderSection(ObjSection *Section)
{
    // This is actually the section header information
    RenderString("ST" + ObjUtil::ToHex(Section->GetIndex()) + ",");
    ObjInt quals = Section->GetQuals();
    if (quals & ObjSection::absolute)
        RenderCstr("A,");
    if (quals & ObjSection::bit)
        RenderCstr("B,");
    if (quals & ObjSection::common)
        RenderCstr("C,");
    if (quals & ObjSection::equal)
        RenderCstr("E,");
    if (quals & ObjSection::max)
        RenderCstr("M,");
    if (quals & ObjSection::now)
        RenderCstr("N,");
    if (quals & ObjSection::postpone)
        RenderCstr("P,");
    if (quals & ObjSection::rom)
        RenderCstr("R,");
    if (quals & ObjSection::separate)
        RenderCstr("S,");
    if (quals & ObjSection::unique)
        RenderCstr("U,");
    if (quals & ObjSection::ram)
        RenderCstr("W,");
    if (quals & ObjSection::exec)
        RenderCstr("X,");
    if (quals & ObjSection::zero)
        RenderCstr("Z,");
   if (quals & ObjSection::virt)
        RenderCstr("V,");
 
    // this assums a section number < 160... otherwise it could be an attrib
    RenderString(ToString(Section->GetName()) + ".");
    endl();
    
    RenderString("SA" + ObjUtil::ToHex(Section->GetIndex()) + ","
        + ObjUtil::ToHex(Section->GetAlignment()) + ".");
    endl();
    RenderString("ASS" + ObjUtil::ToHex(Section->GetIndex()) + "," 
        + ObjUtil::ToHex(Section->GetMemoryManager().GetSize()) + ".");
    endl();
    if (quals & ObjSection::absolute)
    {
        RenderString("ASL" + ObjUtil::ToHex(Section->GetIndex()) + "," 
            + ObjUtil::ToHex(Section->GetMemoryManager().GetBase()) + ".");
        endl();
    }
}
void ObjIeeeAscii::RenderDebugTag(ObjDebugTag *Tag)
{
    ObjString data;
    switch(Tag->GetType())
    {
        case ObjDebugTag::eVar:
            /* debugger has to dereference the name */
            if (!Tag->GetSymbol()->IsSectionRelative() && 
                Tag->GetSymbol()->GetType() != ObjSymbol::eExternal)
            {
                data = GetSymbolName(Tag->GetSymbol());
                RenderComment(eVar, data);
            }
            break;
        case ObjDebugTag::eBlockStart:
        case ObjDebugTag::eBlockEnd:
            RenderComment(Tag->GetType() == ObjDebugTag::eBlockStart ?
                                 eBlockStart : eBlockEnd, ObjString (""));
            break;
        case ObjDebugTag::eFunctionStart:
        case ObjDebugTag::eFunctionEnd:
            data = GetSymbolName(Tag->GetSymbol());
            RenderComment(Tag->GetType() == ObjDebugTag::eFunctionStart ?
                                 eFunctionStart : eFunctionEnd, data);
            break;
        case ObjDebugTag::eLineNo:
            data = ObjUtil::ToDecimal(Tag->GetLineNo()->GetFile()->GetIndex()) 
                    + "," + ObjUtil::ToDecimal(Tag->GetLineNo()->GetLineNumber()) ;
            RenderComment(eLineNo, data);
            break;
        default:
            break;
    }
}
void ObjIeeeAscii::RenderMemory(ObjMemoryManager *Memory)
{
    // this function is optimized to not use C++ stream objects
    // because it is called a lot, and the resultant memory allocations
    // really slow down linker and librarian operations
    char scratch[256];
    int n;
    scratch[0] = 'L';
    scratch[1] = 'D';
    ObjMemoryManager::MemoryIterator itmem;
    n = 2;
    for (itmem = Memory->MemoryBegin(); 
         itmem != Memory->MemoryEnd(); ++itmem)
    {
        ObjMemory *memory = (*itmem);
        if ((memory->HasDebugTags() && GetDebugInfoFlag())
            || memory->GetFixup())
        {
            if (n != 2)
            {
                scratch[n++] = '.';
                scratch[n++] = 0;
                RenderCstr(scratch);
                endl();
                n = 2;
            }
            if (GetDebugInfoFlag())
            {
                ObjMemory::DebugTagIterator it;
                for (it = memory->DebugTagBegin(); it != memory->DebugTagEnd(); ++it)
                {
                    RenderDebugTag(*it);
                }
            }
            if (memory->GetFixup())
            {
                RenderCstr("LR(");
                RenderExpression(memory->GetFixup());
                RenderString("," + ObjUtil::ToHex(memory->GetSize()) + ").");
                endl();
            }
        }
        if (memory->IsEnumerated())
        {
            if (n != 2)
            {
                scratch[n++] = '.';
                scratch[n++] = 0;
                RenderCstr(scratch);
                endl();
                n = 2;
            }
            RenderCstr("LE(");
            RenderString(ObjUtil::ToHex(memory->GetSize()));
            RenderString("," + ObjUtil::ToHex(memory->GetFill()) + ").");
            endl();
            
        }
        else if (memory->GetData())
        {
            ObjByte *p = memory->GetData();
            for (int i=0; i < memory->GetSize(); i++)
            {
                int m = *p >> 4;
                if (m > 9)
                    m += 7;
                m += '0';
                scratch[n++] = m;
                m = *p++ & 0xf;
                if (m > 9)
                    m += 7;
                m += '0';
                scratch[n++] = m;
                if (n >= 66)
                {
                    scratch[n++] = '.';
                    scratch[n++] = 0;
                    RenderCstr(scratch);
                    endl();
                    n = 2;
                }
            }
        }
    }
    if (n != 2)
    {
        scratch[n++] = '.';
        scratch[n++] = 0;
        RenderCstr(scratch);
        endl();
    }
}
void ObjIeeeAscii::RenderBrowseInfo(ObjBrowseInfo *BrowseInfo)
{
    ObjString data;
    data = ObjUtil::ToHex((int)BrowseInfo->GetType()) + "," +
            ObjUtil::ToHex((int)BrowseInfo->GetQual()) + "," +
            ObjUtil::ToDecimal(BrowseInfo->GetLineNo()->GetFile()->GetIndex()) +
                    "," + ObjUtil::ToDecimal(BrowseInfo->GetLineNo()->GetLineNumber()) + 
                    "," + ObjUtil::ToDecimal(BrowseInfo->GetCharPos()) + 
                    "," + ToString(BrowseInfo->GetData());
    RenderComment(eBrowseInfo, data);
}
void ObjIeeeAscii::RenderExpression(ObjExpression *Expression)
{
    switch(Expression->GetOp())
    {
        case ObjExpression::eNop:
            break;
        case ObjExpression::eNonExpression:
            RenderExpression(Expression->GetLeft());
            RenderCstr(",");
            RenderExpression(Expression->GetRight());
            break;
        case ObjExpression::eValue:
            RenderString(ObjUtil::ToHex(Expression->GetValue()));
            break;
        case ObjExpression::eAdd:
            RenderExpression(Expression->GetLeft());
            RenderCstr(",");
            RenderExpression(Expression->GetRight());
            RenderCstr(",+");
            break;
        case ObjExpression::eSub:
            RenderExpression(Expression->GetLeft());
            RenderCstr(",");
            RenderExpression(Expression->GetRight());
            RenderCstr(",-");
            break;
        case ObjExpression::eMul:
            RenderExpression(Expression->GetLeft());
            RenderCstr(",");
            RenderExpression(Expression->GetRight());
            RenderCstr(",*");
            break;
        case ObjExpression::eDiv:
            RenderExpression(Expression->GetLeft());
            RenderCstr(",");
            RenderExpression(Expression->GetRight());
            RenderCstr(",/");
            break;
        case ObjExpression::eExpression:
            RenderExpression(Expression->GetLeft()) ;
            break;
        case ObjExpression::eSymbol:
            if (Expression->GetSymbol()->GetType() == ObjSymbol::eExternal)
            {
                // externals get embedded in the expression
                RenderString("X" + ObjUtil::ToHex(Expression->GetSymbol()->GetIndex()));
            }
            else
            {
                // other types of symbols we don't embed in the expression,
                // instead we embed their values
                RenderExpression(Expression->GetSymbol()->GetOffset()) ;
            }
            break;
        case ObjExpression::eSection:
            RenderString("R" + ObjUtil::ToHex(Expression->GetSection()->GetIndex())) ;
            break;
        case ObjExpression::ePC:
            RenderCstr("P");
            break;
        default:
            break;
    }
}
void ObjIeeeAscii::RenderComment(eCommentType Type, ObjString strng)
{
    RenderString("CO" + ObjUtil::ToDecimal((int)Type,3) 
                + "," + ToString(strng) + ".");
    endl();
}
void ObjIeeeAscii::GatherCS(const char *Cstr)
{
    for ( const char *data = Cstr; *data; data++)
    {
        if (*data>= ' ')
            cs += *data;
    }
}
bool ObjIeeeAscii::HandleWrite()
{
    ioBufferLen = 0;
    ioBuffer = new char [BUFFERSIZE];
    if (!ioBuffer)
        return false;
    ResetCS();
    WriteHeader();
    RenderCS();
    ResetCS();
    WriteFiles();
    RenderComment(eMakePass, ObjString ("Make Pass Separator"));
    RenderCS();
    ResetCS();
    WriteSectionHeaders();	
    RenderCS();
    ResetCS();
    WriteTypes();	
    RenderCS();
    ResetCS();
    WriteSymbols();	
    WriteStartAddress();
    RenderCS();
    ResetCS();
    RenderComment(eLinkPass, ObjString ("Link Pass Separator"));
    WriteSections();	
    RenderCS();
    ResetCS();
    RenderComment(eBrowsePass, ObjString ("Browse Pass Separator"));
    WriteBrowseInfo();	
    RenderCS();
    ResetCS();
    WriteTrailer();
    flush();
    delete [] ioBuffer;
    ioBuffer = NULL;
    return true;
}
void ObjIeeeAscii::WriteHeader()
{
    RenderString("MB" + translatorName + "," + ToString(file->GetName()) + ".");
    endl();
    RenderString("AD" + ObjUtil::ToHex(GetBitsPerMAU()) + ","
        + ObjUtil::ToHex(GetMAUS()) + ","
        + (GetFile()->GetBigEndian() ? "M." : "L."));
    endl();
    RenderString("DT" + ToTime(file->GetFileTime()) + ".");
    endl();
    if (file->GetInputFile())
    {
        RenderFile(file->GetInputFile());
    }
    if (absolute)
    {
        RenderComment(eAbsolute, ObjString ("Absolute file"));
    }
}
void ObjIeeeAscii::WriteFiles()
{
    for (ObjFile ::SourceFileIterator it = file->SourceFileBegin();
             it != file->SourceFileEnd(); ++it)
    {	
        RenderFile(*it);
    }
}
void ObjIeeeAscii::WriteSectionHeaders()
{
    for (ObjFile ::SectionIterator it = file->SectionBegin();
             it != file->SectionEnd(); ++it)
    {	
        RenderSection(*it);
    }
}
void ObjIeeeAscii::WriteTypes()
{
    if (GetDebugInfoFlag())
    {
        for (ObjFile ::TypeIterator it = file->TypeBegin();
             it != file->TypeEnd(); ++it)
        {
            RenderType(*it);
        }
    }	
}
void ObjIeeeAscii::WriteSymbols()
{
    for (ObjFile ::SymbolIterator it = file->PublicBegin();
             it != file->PublicEnd(); ++it)
        RenderSymbol(*it);
    for (ObjFile ::SymbolIterator it = file->ExternalBegin();
             it != file->ExternalEnd(); ++it)
        RenderSymbol(*it);
    for (ObjFile ::SymbolIterator it = file->LocalBegin();
             it != file->LocalEnd(); ++it)
        RenderSymbol(*it);
    for (ObjFile ::SymbolIterator it = file->AutoBegin();
             it != file->AutoEnd(); ++it)
        RenderSymbol(*it);
    for (ObjFile ::SymbolIterator it = file->RegBegin();
             it != file->RegEnd(); ++it)
        RenderSymbol(*it);
    for (ObjFile ::SymbolIterator it = file->DefinitionBegin();
             it != file->DefinitionEnd(); ++it)
        RenderSymbol(*it);
    for (ObjFile ::SymbolIterator it = file->ImportBegin();
             it != file->ImportEnd(); ++it)
        RenderSymbol(*it);
    for (ObjFile ::SymbolIterator it = file->ExportBegin();
             it != file->ExportEnd(); ++it)
        RenderSymbol(*it);
}
void ObjIeeeAscii::WriteStartAddress()
{
    if (startAddress)
    {
        RenderCstr("ASG,");
        RenderExpression(startAddress);
        RenderCstr(".");
        endl();
    }
}
void ObjIeeeAscii::WriteSections()
{
    for (ObjFile ::SectionIterator it = file->SectionBegin();
             it != file->SectionEnd(); ++it)
    {	
        RenderString("SB" + ObjUtil::ToHex((*it)->GetIndex()) + ".");
        endl();
        RenderMemory(&(*it)->GetMemoryManager());
    }
}
void ObjIeeeAscii::WriteBrowseInfo()
{
    for (ObjFile ::BrowseInfoIterator it = file->BrowseInfoBegin();
             it != file->BrowseInfoEnd(); ++it)
    {
        RenderBrowseInfo(*it);
    }	
}
void ObjIeeeAscii::WriteTrailer()
{
    RenderString("ME.");
    endl();
}
void ObjIeeeAscii::RenderCS()
{
    // the CS is part of the checksum, but the number and '.' are not.
    RenderCstr("CS");
    RenderString(ObjUtil::ToHex(cs & 127,2) + ".");
    endl();
}
void ObjIeeeIndexManager::ResetIndexes()
{
    Section = 0;
    Public = 0;
    Local = 0;
    External = 0;
    Type = eDerivedTypeBase;
    File = 0;
    Auto = 0;
    Reg = 0;
}
