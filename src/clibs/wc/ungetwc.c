/*
    Software License Agreement (BSD License)
    
    Copyright (c) 1997-2008, David Lindauer, (LADSoft).
    All rights reserved.
    
    Redistribution and use of this software in source and binary forms, with or without modification, are
    permitted provided that the following conditions are met:
    
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
    
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
    WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
    ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
    ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <wchar.h>
#undef ungetc
wint_t _RTL_FUNC ungetwc(wint_t c, FILE *stream)
{
    if (stream->token != FILTOK) {
        errno = _dos_errno = ENOENT;
        return WEOF;
    }
    if (stream->orient == __or_narrow) {
        errno = EINVAL;
        return WEOF;
    }
    stream->orient = __or_wide;
    stream->flags &= ~_F_VBUF;
    if (c == WEOF)
        return WEOF;
    if (stream->buffer) {
        if ((stream->flags & _F_IN) && stream->curp != stream->buffer) {
            if (stream->level == stream->bsize) {
                errno = _dos_errno = ENOSPC;
                return WEOF;
            }
            if (stream->curp == stream->buffer) {
                int len;
                memmove (stream->buffer + (len = stream->bsize - stream->level), 
                        stream->buffer, stream->level);
                stream->curp += len;
            }
        }
        else {
            if (fflush(stream))
                return EOF;
            stream->flags |= _F_IN;
             stream->level = 0;
            stream->curp = stream->buffer+stream->bsize;
        }
        stream->level++;
        *--stream->curp = (char)c;
    }
    else {
        if (stream->hold) {
            errno = _dos_errno = ENOSPC;
            return WEOF;
        }
        stream->hold = (char)c;
    }
    stream->flags &= ~_F_EOF;
    return c;
}
int _RTL_FUNC _ungetwc(int c, FILE *stream)
{
    return ungetwc(c, stream);
}