/*
    Software License Agreement (BSD License)
    
    Copyright (c) 1997-2016, David Lindauer, (LADSoft).
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
#ifndef Listing_h
#define Listing_h

class Label;
class Instruction;

#include <fstream>
#include <deque>

static const int AddressWidth = 4;
static const int Bytes = 8;

class ListedLine
{
public:
    ListedLine(Instruction *Ins, int LineNo) : lineno(LineNo), label(nullptr), ins(Ins) { }
    ListedLine(Label *lbl, int LineNo) : lineno(LineNo), label(lbl), ins(nullptr) { }
    int lineno;
    Label *label;
    Instruction *ins;
};
class Listing
{	
public:
    Listing();
    ~Listing();
    void Add(Instruction *ins, int lineno, bool inMacro)
    {
        list.push_back(new ListedLine(ins, lineno));
    }
    void Add(Label *lbl, int lineno, bool inMacro)
    {
        list.push_back(new ListedLine(lbl, lineno));
    }
    bool Write(std::string &listingName, std::string &inName, bool listMacros);
    void SetBigEndian(bool flag) { bigEndian = flag; }
protected:
    void ListLine(std::fstream &out, std::string &line, ListedLine *cur, bool macro);
private:
    std::deque<ListedLine *> list;
    std::string blanks;
    std::string zeros;
    bool bigEndian;
} ;

#endif