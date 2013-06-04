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
#ifndef PREPROC_H
#define PREPROC_H

typedef LLONG_TYPE PPINT;
#define TRUE 1
#define FALSE 0

/* ## sequences */
#define REPLACED_TOKENIZING -80
/* left or right-hand size of a ## when an arg has been replaced by an empty string */
#define TOKENIZING_PLACEHOLDER -79
#define STRINGIZING_PLACEHOLDER -78
#define REPLACED_ALREADY -77
 
#define issymchar(x) (((x) >= 0) && (isalnum(x) || (x) == '_'))
#define isstartchar(x) (((x) >= 0) && (isalpha(x) || (x) == '_'))

#define SYMBOL_NAME_LEN 256
#define MACRO_REPLACE_SIZE 32768
#define MAX_PACK_DATA 256
#define DEFINELIST_MAX 256

#define STD_PRAGMA_FENV 1
#define STD_PRAGMA_FCONTRACT 2
#define STD_PRAGMA_CXLIMITED 4

/* struct for preprocessor if tracking */
typedef struct ifstruct
{
    struct ifstruct *next; /* next */
    short iflevel;
    short elsetaken;
    int line;
} IFSTRUCT;

typedef struct _includes_
{
    struct _includes_ *next;
    FILE	*handle;
    int 	fileindex;
    int		line;
    int		current;
    int 	ifskip;
    int		skiplevel;
    BOOL	elsetaken;
    char	*data;
    unsigned char *lptr;
    int		pos;
    int		sysflags;
    BOOL	sys_inc;
    IFSTRUCT *ifs;
    char 	*ibufPtr;
    int		inputlen;
    char	*fname;
    char inputline[MACRO_REPLACE_SIZE];
    char inputbuffer[32768];
} INCLUDES;
/* #define tracking */
typedef struct _defstruct
{
    char *name;
    char *string;
    int argcount;
    int line;
    char *file;
    char **args;
    int varargs: 1;
    int permanent: 1;
    int undefined : 1 ;
    int preprocessing: 1; /* true if is currently not a candidate for preprocessing (macros only( */
} DEFSTRUCT;

typedef struct _filelist
{
    struct _filelist *next;
    char *data;
    int hascode;
} FILELIST;

#endif /* PREPROC_H */
