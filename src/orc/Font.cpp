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
#include "Font.h"
#include "RCFile.h"
#include "ResFile.h"
#include "ResourceData.h"
#include <stdexcept>


Font::Font(const ResourceId &Id, const ResourceInfo &info)
        : Resource(eFont, Id, info), data(NULL) 
{ 
}
Font::~Font() 
{  
    delete data; 
}
void Font::SetData(ResourceData *rdata) {
   
        delete data;
    data = rdata;
}
void Font::WriteRes(ResFile &resFile)
{
    Resource::WriteRes(resFile); 
    if (data) 
        data->WriteRes(resFile); 
    resFile.Release(); 
}
void Font::ReadRC(RCFile &rcFile)
{
    resInfo.SetFlags(resInfo.GetFlags() | ResourceInfo::Pure);
    resInfo.ReadRC(rcFile, false);
    ResourceData *rd = new ResourceData;
    rd->ReadRC(rcFile);
    rcFile.NeedEol();
    int n = rd->GetWord();
    int s = rd->GetWord();
    // check version and size
/*
    if *n != 0x200 && n != 0x300 || s != rd->GetLen() || s < 0x180)
    {
        delete rd;
        throw new std::runtime_error("Invalid font file");
    }
*/
    data = rd;
}

