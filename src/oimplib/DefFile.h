/* Software License Agreement
 * 
 *     Copyright(C) 1994-2018 David Lindauer, (LADSoft)
 * 
 *     This file is part of the Orange C Compiler package.
 * 
 *     The Orange C Compiler package is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version, with the addition of the 
 *     Orange C "Target Code" exception.
 * 
 *     The Orange C Compiler package is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *     contact information:
 *         email: TouchStone222@runbox.com <David Lindauer>
 */

#ifndef DefFile_h
#define DefFile_h

#include "Token.h"

#include <string>
#include <fstream>
#include <map>
#include <deque>

enum {
    edt_none = 256,
    edt_at,
    edt_dot,
    edt_equals,
    edt_name,
    edt_library,
    edt_exports,
    edt_imports,
    edt_description,
    edt_stacksize,
    edt_heapsize,
    edt_code,
    edt_data,
    edt_sections,
    edt_noname,
    edt_constant,
    edt_private,
    edt_read,
    edt_write,
    edt_execute,
    edt_shared
};

class DefFile
{
public:
    DefFile(const std::string &fname) : fileName(fname), tokenizer("", &keywords), lineno(0), imageBase(-1),
        stackSize(-1), heapSize(-1), token(nullptr)
        { Init(); }
    virtual ~DefFile();
    bool Read();
    bool Write();
    std::string GetName() { return fileName; }
    void SetFileName(const std::string &fname) { fileName = fname; }
    std::string GetLibraryName() { return library; }
    void SetLibraryName(const std::string &name) { library = name; }
    struct Export
    {
        Export() : byOrd(false), ord(-1) { }
        bool byOrd;
        unsigned ord;
        std::string id;
        std::string module;
        std::string entry;
    };
    struct Import
    {
        std::string id;
        std::string module;
        std::string entry;
    };
    void Add(Export *e) { exports.push_back(e); }
    void Add(Import *i) { imports.push_back(i); }
    typedef std::deque<Export *>::iterator ExportIterator;
    ExportIterator ExportBegin() { return exports.begin(); }
    ExportIterator ExportEnd() { return exports.end(); }
    typedef std::deque<Import *>::iterator ImportIterator;
    ImportIterator ImportBegin() { return imports.begin(); }
    ImportIterator ImportEnd() { return imports.end(); }
protected:
    void Init();
    void NextToken();
    
    void ReadName();
    void ReadLibrary();
    void ReadExports();
    void ReadImports();
    void ReadDescription();
    void ReadStacksize();
    void ReadHeapsize();
    void ReadSectionsInternal(const char *name);
    void ReadCode();
    void ReadData();
    void ReadSections();

    void WriteName();
    void WriteLibrary();
    void WriteExports();
    void WriteImports();
    void WriteDescription();
    void WriteStacksize();
    void WriteHeapsize();
    void WriteSectionBits(unsigned value);
    void WriteCode();
    void WriteData();
    void WriteSections();

private:
    std::string fileName;
    std::string name;
    std::string library;
    std::string description;
    unsigned imageBase;
    unsigned stackSize;
    unsigned heapSize;
    Tokenizer tokenizer;
    const Token *token;
    std::fstream *stream;
    int lineno;
    std::map<std::string, unsigned> sectionMap;
    std::deque<Export *> exports;
    std::deque<Import *> imports;
    static bool initted;
    static KeywordHash keywords;
} ;
#endif