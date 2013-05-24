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
#ifndef LINKMANAGER_H
#define LINKMANAGER_H

#include <fstream>
#include <set>
#include "ObjTypes.h"
#include "ObjSymbol.h"
#include "CmdFiles.h"
#include "LinkTokenizer.h"
#include <iostream>
class LibManager;
class LinkPartition;
class LinkPartitionSpecifier;
class LinkLibrary;
class ObjSymbol;
class LinkLibrary;
class ObjFactory;
class ObjIOBase;
class ObjFile;
class ObjIndexManager;

void HookError(int);
class LinkSymbolData
{
    public:
        LinkSymbolData(ObjFile *File, ObjSymbol *Symbol): file(File), symbol(Symbol), used(false) {}
        LinkSymbolData(ObjSymbol *Symbol): file(NULL), symbol(Symbol), used(false) {}
        LinkSymbolData(): file(NULL), symbol(NULL), used(false) {}
        ~LinkSymbolData() {}
        
        ObjFile *GetFile() { return file; }
        void SetFile(ObjFile *File) { file = File; }
        ObjSymbol *GetSymbol() { return symbol; }
        void SetSymbol(ObjSymbol *sym) { symbol = sym; }
        bool GetUsed() { return used; }
        void SetUsed(bool Used) { used = Used; }
        
    private:
        bool used;
        ObjFile *file;
        ObjSymbol *symbol;
};
struct linkltcompare
{
    bool operator () (LinkSymbolData *left, LinkSymbolData *right) const
        { return strcmp(left->GetSymbol()->GetName().c_str(), right->GetSymbol()->GetName().c_str()) < 0;}
//		{ return left->GetSymbol()->GetName() < right->GetSymbol()->GetName();}
} ;
class LinkManager
{
    typedef std::vector<LinkPartitionSpecifier *> PartitionData;
    typedef std::set<LinkSymbolData *, linkltcompare> SymbolData;
    typedef std::vector<ObjFile *> FileData;
    public:
        LinkManager(ObjString &Specification, bool CaseSensitive, const ObjString &OutputFile = "", 
                        bool CompleteLink = false, ObjString DebugFile = "") :
                        specification(Specification), outputFile(OutputFile), specName(Specification),
                        factory(NULL), indexManager(NULL), completeLink(CompleteLink),
                        ioBase(NULL), caseSensitive(CaseSensitive), debugFile(DebugFile) { }
            
        ~LinkManager();

        void SetFactory(ObjFactory *Factory) { factory = Factory; }
        ObjIOBase *GetObjIo() { return ioBase; }
        void SetObjIo(ObjIOBase *IoBase) { ioBase = IoBase; }
        void SetIndexManager(ObjIndexManager *Manager) { indexManager = Manager; }
        void AddObject(const ObjString &name);
        void AddLibrary(const ObjString &name);
        void SetLibPath(const ObjString &path) { libPath = path; }
        void SetOutputFile(const ObjString &name) { outputFile = name; }
        ObjString GetOutputFile() const { return outputFile; }
        void Link();
        
        typedef PartitionData::iterator PartitionIterator;
        
        PartitionIterator PartitionBegin() { return partitions.begin(); }
        PartitionIterator PartitionEnd() { return partitions.end(); }

        typedef SymbolData::iterator SymbolIterator;
        void InsertSymbol(LinkSymbolData *data) { publics.insert(data); }

        SymbolIterator PublicBegin() { return publics.begin(); }
        SymbolIterator PublicEnd() { return publics.end(); }
        SymbolIterator PublicFind(LinkSymbolData *d) { return publics.find(d); }
        SymbolIterator ExternalBegin() { return externals.begin(); }
        SymbolIterator ExternalEnd() { return externals.end(); }
        SymbolIterator ImportBegin() { return imports.begin(); }
        SymbolIterator ImportEnd() { return imports.end(); }
        SymbolData GetImports() { return imports; }
        typedef FileData::iterator FileIterator;

        FileIterator FileBegin() { return fileData.begin(); }
        FileIterator FileEnd() { return fileData.end(); }
        
        static void LinkError(const ObjString &error) { HookError(0); std::cout << "Error: " << error << std::endl; errors++; }
        static void LinkWarning(const ObjString &error) { HookError(1); std::cout << "Warning: " << error << std::endl; warnings++; }

        bool IsExternal(ObjString &name)		
        {
            ObjSymbol symbol(name, ObjSymbol::eLabel, 0);
            LinkSymbolData data(&symbol);
            SymbolIterator it = externals.find(&data);
            if (it != externals.end())
                return true;
            return false;
        }
        int ErrCount() const { return errors; }
        int WarnCount() const { return warnings; }		
    private:
        void MergePublics(ObjFile *file, bool toerr);
        void LoadFiles();
        LinkLibrary *OpenLibrary(const ObjString &name);
        void LoadLibraries();
        void ScanLibraries();
        void CloseLibraries();
        bool ParseAssignment(LinkTokenizer &spec);
        bool CreateSeparateRegions(LinkManager *manager, CmdFiles &files, LinkTokenizer &spec);
        bool ParsePartitions();
        void CreatePartitions();
        void PlaceSections();
        void UnplacedWarnings();
        bool ExternalErrors();
        void CreateOutputFile();
        ObjString outputFile;
        LinkTokenizer specification;
        PartitionData partitions;
        SymbolData publics;
        SymbolData externals;
        SymbolData imports;
        SymbolData exports;
        FileData fileData;
        CmdFiles objectFiles;
        CmdFiles libFiles;
        std::set<LinkLibrary *> dictionaries;
        ObjIOBase *ioBase;
        ObjIndexManager *indexManager;
        ObjFactory *factory;
        ObjString libPath;
        ObjString &specName;
        ObjString debugFile;
        bool completeLink;
        bool caseSensitive;
        static int errors;
        static int warnings;
};
#endif
