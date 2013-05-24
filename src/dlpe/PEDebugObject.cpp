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
#include "PEObject.h"
#include "Utils.h"
#include "sqlite3.h"

#include <stdio.h>
#include <ctype.h>
#include <map>
void PEDebugObject::Setup(ObjInt &endVa, ObjInt &endPhys)
{
    if (virtual_addr == 0)
    {
        virtual_addr = endVa;
    }
    else
    {
        if (virtual_addr != endVa)
            Utils::fatal("Internal error");
    }
    raw_addr = endPhys;
    size = initSize = fileName.size() + 2 + 32;
    data = new unsigned char[512];
    memset(data,0,512);
    data[0] = 'L';
    data[1] = 'S';
    data[2] = '1';
    data[3] = '4';
    data[32] = fileName.size();
    strcpy((char *)data + 33, fileName.c_str());
    
    endVa = ObjectAlign(objectAlign, endVa + size);
    endPhys = ObjectAlign(fileAlign, endPhys + initSize);
}
int PEDebugObject::NullCallback(void *NotUsed, int argc, char **argv, char **azColName)
{
    return 0;
}
void PEDebugObject::SetDebugInfo(ObjString fileName, ObjInt base)
{
    sqlite3 *dbPointer = NULL;
    if (sqlite3_open_v2(fileName.c_str(), &dbPointer, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK)
    {
        sqlite3_busy_timeout(dbPointer, 400);
        char *zErrMsg  = 0;
        static char *cmd =     "INSERT INTO dbPropertyBag (property, value)"
                               " VALUES (\"ImageBase\", %d);";
        char realCmd[256];
        sprintf(realCmd, cmd, base);

        int rc = sqlite3_exec(dbPointer, realCmd, NullCallback, 0, &zErrMsg);
        if( rc!=SQLITE_OK )
        {
          fprintf(stderr, "SQL error: %s\n", zErrMsg);
          sqlite3_free(zErrMsg);
        }

    }
    if (dbPointer)
        sqlite3_close(dbPointer);
}
