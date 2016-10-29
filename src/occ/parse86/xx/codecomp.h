/*
    Software License Agreement (BSD License)
    
    Copyright (c) 1997-2012, David Lindauer, (LADSoft).
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
#include "sqlite3.h"
typedef struct
{
    int argCount;
    char *baseType;
    BOOL member;
    struct _ProtoData
    {
        char *fieldType;
        char *fieldName;
    } *data;
} CCPROTODATA;
typedef struct
{
    int fieldCount;
    int indirectCount;
    struct _structData
    {
        char *fieldType;
        char *fieldName;
        sqlite3_int64 subStructId;
        int indirectCount;
        int flags;
    } *data;
} CCSTRUCTDATA;
typedef struct _ccfuncdata
{
    struct _ccfuncdata *next;
    char *fullname;
    CCPROTODATA *args;
} CCFUNCDATA;
void CodeCompInit(void);
void DoParse(char *name);
void ccLineChange(char *name, int drawnLineno, int delta);
void deleteFileData(char *name);
CCFUNCDATA *ccLookupFunctionList(int lineno, char *file, char *name);
void ccFreeFunctionList(CCFUNCDATA *data);
int ccLookupType(char *buffer, char *name, char *module, int line, int *rflags, sqlite_int64 *rtype);
int ccLookupStructType(char *name, char *module, int line, sqlite3_int64 *structId, int *indirectCount);
CCSTRUCTDATA *ccLookupStructElems(char *module, sqlite3_int64 structId, int indirectCount);
void ccFreeStructData(CCSTRUCTDATA *data);
int ccLookupContainingNamespace(char *file, int lineno, char *ns);
int ccLookupContainingMemberFunction(char *file, int lineno, char *func);
int ccLookupFunctionType(char *name, char *module, sqlite3_int64 *protoId, sqlite3_int64 *typeId);
CCPROTODATA *ccLookupArgList(sqlite3_int64 protoId, sqlite3_int64 argId);
void ccFreeArgList(CCPROTODATA *data);
BYTE * ccGetLineData(char *name, int *max);
