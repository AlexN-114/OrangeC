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
 *     along with Orange C.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *     contact information:
 *         email: TouchStone222@runbox.com <David Lindauer>
 * 
 */

#ifndef LIBFILES_H
#define LIBFILES_H
#include "ObjTypes.h"
#include <deque>
#include <stdio.h>
class ObjFile;
class ObjFactory;
class LibFiles
{
public:
    struct FileDescriptor
    {
        FileDescriptor(const ObjString &Name) : offset(0), name(Name), data(nullptr) { }
        FileDescriptor(const FileDescriptor &old) : name(old.name), offset(old.offset), data(nullptr) { }
        ~FileDescriptor() { /* if (data) delete data; */ } // data will go away when the factory goes awsy
        ObjString name;
        ObjInt offset;
        ObjFile *data;
    };
    LibFiles(bool CaseSensitive = true) { caseSensitive = CaseSensitive; }
    virtual ~LibFiles() { }
    
    void Add(ObjFile &obj);
    void Add(const ObjString &Name);
    void Remove(const ObjString &Name);
    void Extract(FILE *stream, const ObjString &Name);
    void Replace(const ObjString &Name);
    
    size_t size() { return files.size(); }
    bool ReadNames(FILE *stream, int count);
    void WriteNames(FILE *stream);
    void ReadOffsets(FILE *stream, int count);
    void WriteOffsets(FILE *stream);
    bool ReadFiles(FILE *stream, ObjFactory *factory);
    void WriteFiles(FILE *stream, ObjInt align);
    
    ObjFile *LoadModule(FILE *stream, ObjInt FileIndex, ObjFactory *factory);	
        
    typedef std::deque<FileDescriptor *>::iterator FileIterator;
    FileIterator FileBegin () { return files.begin(); }
    FileIterator FileEnd () { return files.end(); }
protected:
    ObjFile *ReadData(FILE *stream, const ObjString &name, ObjFactory *factory);
    void WriteData(FILE *stream, ObjFile *file, const ObjString &name);
    void Align(FILE *stream, ObjInt align);
private:
    std::deque<FileDescriptor *> files;
    std::deque<ObjFile *> objectFiles;
    bool caseSensitive;
} ;
#endif
