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
#include "LibManager.h"

void LibManager::InitHeader()
{
    memset(&header, 0, sizeof(header));
    header.sig = LibHeader::LIB_SIG;
}
bool LibManager::LoadLibrary()
{
    memset(&header, 0, sizeof(header));
    fseek(stream, 0, SEEK_SET);
    fread((char *)&header, sizeof(header), 1, stream);
    if (header.sig != LibHeader::LIB_SIG)
        return false;
    fseek(stream, header.namesOffset, SEEK_SET);
    files.ReadNames(stream, header.filesInModule);
    fseek(stream, header.offsetsOffset, SEEK_SET);
    files.ReadOffsets(stream, header.filesInModule);
    dictionary.SetBlockCount(header.dictionaryBlocks);
    return true;
}
ObjInt LibManager::Lookup(const ObjString &name)
{
    if (header.sig == LibHeader::LIB_SIG)
        return dictionary.Lookup(stream, header.dictionaryOffset, header.dictionaryBlocks, name);
    return NULL;
}

