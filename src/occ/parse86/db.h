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

*/
#ifndef DB_H
#define DB_H
#include "..\..\sqlite3\sqlite3.h"

void ccReset(void);
int ccDBOpen(char *name);
int ccBegin(void );
int ccEnd(void );
int ccDBDeleteForFile( sqlite3_int64 id);
int ccWriteName( char *name, sqlite_int64 *id);
int ccWriteStructName( char *name, sqlite_int64 *id);
int ccWriteFileName( char *name, sqlite_int64 *id);
int ccWriteFileTime( char *name, int time, sqlite_int64 *id);
int ccWriteLineNumbers( char *symname, char *typename, char *filename, 
                       int indirectCount, sqlite_int64 struct_id, 
                       sqlite3_int64 main_id, int start, int end, 
                       int flags, sqlite_int64 *id);
int ccWriteLineData(sqlite_int64 file_id, sqlite_int64 main_id, char *data, int len, int lines);
int ccWriteGlobalArg( sqlite_int64 line_id, sqlite_int64 main_id, char *symname, char *typename, int *order);
int ccWriteStructField( sqlite3_int64 name_id, char *symname, char *typename, 
                       int indirectCount, sqlite_int64 struct_id, 
                       sqlite3_int64 file_id, sqlite3_int64 main_id, 
                       int *order, sqlite_int64 *id);
int ccWriteMethodArg( sqlite_int64 struct_id, char *typename, int *order);

#endif //DB_H