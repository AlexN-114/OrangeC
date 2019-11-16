/* Software License Agreement
 *
 *     Copyright(C) 1994-2019 David Lindauer, (LADSoft)
 *
 *     This file is part of the Orange C Compiler package.
 *
 *     The Orange C Compiler package is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
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

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <limits.h>
#include "iexpr.h"
#include "beinterf.h"
#include "ildata.h"
#include "iexpr.h"
#include "../occ/Winmode.h"
#include "../occ/be.h"
#include <deque>
#include <functional>
#include <map>

extern std::vector<SimpleSymbol*> externals;
extern std::vector<SimpleSymbol*> globalCache;
extern std::vector<SimpleSymbol*> typeSymbols;
extern std::vector<SimpleSymbol*> typedefs;
extern std::vector<BROWSEINFO*> browseInfo;
extern std::vector<BROWSEFILE*> browseFiles;
extern std::deque<BaseData*> baseData;
extern std::list<MsilProperty> msilProperties;

extern int registersAssigned;

extern std::string prm_libPath;
extern std::string prm_include;
extern const char* pinvoke_dll;
extern std::string prm_snkKeyFile;
extern std::string prm_assemblyVersion;
extern std::string prm_namespace_and_class;
extern std::string prm_OutputDefFile;
extern std::string compilerName;
extern std::string intermediateName;
extern std::string backendName;

extern int exitBlock;
extern int nextLabel;
extern bool assembling;
extern int fastcallAlias;

extern int showBanner;
extern int verbosity;
extern int dataAlign;
extern int bssAlign;
extern int constAlign;
extern int architecture;

extern std::list<std::string> inputFiles;
extern std::list<std::string> backendFiles;
extern std::list<std::string> libIncludes;
extern std::list<std::string> toolArgs;
extern std::list<std::string> prm_Using;
extern std::map<std::string, std::string> bePragma;

extern std::string outputFileName;
extern std::string prm_assemblerSpecifier;

extern FILE* inputFile;
extern BLOCK* currentBlock;

static std::list<std::string> textRegion;
static std::map <std::string, int> cachedText;
static size_t textOffset;
static FunctionData* current, *lastFunction;
static std::map<int, std::string> texts;
static std::vector<SimpleSymbol*> temps;

inline void dothrow()
{
    std::runtime_error e("");
    throw e;

}
inline static int UnstreamByte()
{
    int n = fgetc(inputFile);
    if (n == -1)
    {
        dothrow();
    }
    return n;
}
inline static void UnstreamBlockType(int blockType, bool end)
{
    int n = UnstreamByte();
    if (n != blockType + (end ? 0x80 : 0x40))
        dothrow();
}
template <class T>
inline static void UnstreamBlock(T blockType, std::function<void(void)> blockRenderer)
{
    UnstreamBlockType(blockType, false);
    blockRenderer();
    UnstreamBlockType(blockType, true);
}

inline static int UnstreamInt()
{
    int value = UnstreamByte() << 8;
    value |= UnstreamByte();
    if (value & 0x8000)
    {
        value &= 0x7fff;
        value <<= 16;
        value |= UnstreamByte() << 8;
        value |= UnstreamByte() << 0;

    }
    return value;
}
inline static int UnstreamTextIndex()
{
    return UnstreamInt();
}
inline static void UnstreamString(std::string&value)
{
    int v = UnstreamInt();
    value.resize(v, 0);
    for (auto&& c : value)
        c = UnstreamByte();
}
static void UnstreamStringList(std::list<std::string> & list)
{
    int v = UnstreamInt();
    for (int i = 0; i < v; i++)
    {
        list.push_back("");
        UnstreamString(list.back());
    }
}
static void UnstreamBuffer(void *buf, int len)
{
    if (1 != fread(buf, len, 1, inputFile))
        dothrow();
}
inline static void UnstreamIntValue(void *buf, int len)
{
    UnstreamBlock(STT_INT, [buf, len]() {
        for (int i = len - 1; i >= 0; i--)
            ((unsigned char *)buf)[i] = UnstreamByte();
    });
}
static void UnstreamFloatValue(FPF& fv)
{
    UnstreamBlock(STT_FLOAT, [&fv]() {
        fv.type = UnstreamInt();
        fv.sign = UnstreamInt();
        fv.exp = UnstreamInt();
        for (int i = 0; i < INTERNAL_FPF_PRECISION; i++)
            fv.mantissa[i] = UnstreamInt();
    });

}
static SimpleSymbol* UnstreamSymbol();
static LIST* UnstreamSymbolTable()
{
    LIST *syms = nullptr;
    int i = UnstreamInt();
    LIST **p = &syms;
    for (;i>0; i--)
    {
        *p = (LIST*)Alloc(sizeof(LIST));
        (*p)->data = (SimpleSymbol*)UnstreamInt();
        p = &(*p)->next;
    }
    return syms;
}
static BaseList* UnstreamBases()
{
    BaseList* bases= nullptr;
    int i = UnstreamInt();
    BaseList**p = &bases;
    for (;i > 0; i--)
    {
        *p = (BaseList*)Alloc(sizeof(BaseList));
        UnstreamBlock(STT_BASE, [p]() {
            (*p)->sym = (SimpleSymbol*)UnstreamInt();
            (*p)->offset = UnstreamInt();
        });
        p = &(*p)->next;
    }
    return bases;
}
static SimpleType* UnstreamType()
{
    SimpleType* rv = nullptr;
    UnstreamBlock(STT_TYPE, [&rv]() {
        st_type type = (st_type)UnstreamInt();
        if (type != st_none)
        {
            rv = (SimpleType*)Alloc(sizeof(SimpleType));
            rv->type = type;
            rv->size = UnstreamInt();
            rv->sizeFromType = UnstreamInt();
            rv->bits = UnstreamInt();
            rv->startbit = UnstreamInt();
            rv->sp = (SimpleSymbol*)UnstreamInt();
            rv->flags = UnstreamInt();
            rv->btp = UnstreamType();
        }
    });
    return rv;
}
static SimpleSymbol* UnstreamSymbol()
{
    SimpleSymbol* rv = nullptr;
    UnstreamBlock(STT_SYMBOL, [&rv]() {
        int storage_class = UnstreamInt();
        if (storage_class != scc_none)
        {
            rv = (SimpleSymbol*)Alloc(sizeof(SimpleSymbol));
            rv->storage_class = (e_scc_type)storage_class;
            rv->name = (const char*)UnstreamTextIndex();
            rv->outputName = (const char*)UnstreamTextIndex();
            rv->importfile = (const char*)UnstreamTextIndex();
            rv->namespaceName = (const char*)UnstreamTextIndex();
            rv->msil = (const char*)UnstreamTextIndex();
            rv->i = UnstreamInt();
            rv->regmode = UnstreamInt();
            UnstreamIntValue(&rv->offset, 4);
            rv->label = UnstreamInt();
            rv->templateLevel = UnstreamInt();
            rv->flags = (unsigned long long)UnstreamInt()<< 32;
            rv->flags |= UnstreamInt();
            rv->sizeFromType = UnstreamInt();
            rv->align = UnstreamInt();
            rv->size = UnstreamInt();
            rv->parentClass = (SimpleSymbol*)UnstreamInt();
            rv->tp = UnstreamType();
            rv->syms = UnstreamSymbolTable();
            rv->baseClasses = UnstreamBases();
        }
    });
    return rv;
}
static SimpleSymbol* GetTempref(int n)
{
    if (!temps[n])
    {
        SimpleSymbol* sym = temps[n] = (SimpleSymbol*)Alloc(sizeof(SimpleSymbol));
        char buf[256];
        sym->storage_class = scc_temp;
        sprintf(buf, "$$t%d", n);
        sym->name = sym->outputName = litlate(buf);
        sym->i = n;

    }
    return temps[n];
}
static SimpleExpression* UnstreamExpression()
{
    SimpleExpression* rv = nullptr;
    UnstreamBlock(STT_EXPRESSION, [&rv]() {
        int type = UnstreamInt();
        if (type != se_none)
        {
            rv = (SimpleExpression*)Alloc(sizeof(SimpleExpression));
            rv->type = (se_type)type;
            rv->flags = UnstreamInt();
            switch (rv->type)
            {
            case se_i:
            case se_ui:
                UnstreamIntValue(&rv->i, 8);
                break;
            case se_f:
            case se_fi:
                UnstreamFloatValue(rv->f);
                break;
            case se_fc:
                UnstreamFloatValue(rv->c.r);
                UnstreamFloatValue(rv->c.i);
                break;
            case se_const:
            case se_absolute:
            case se_auto:
            case se_global:
            case se_threadlocal:
            case se_pc:
            case se_structelem:
                rv->sp = (SimpleSymbol*)UnstreamInt();
                break;
            case se_labcon:
                rv->i = UnstreamInt();
                break;
            case se_tempref:
            {
                int n = UnstreamInt();
                rv->sp = GetTempref(n);
                break;
            }
            case se_msil_array_access:
                rv->msilArrayTP = UnstreamType();
                break;
            case se_msil_array_init:
                rv->tp = UnstreamType();
                break;
            case se_string:
                {
                std::string val;
                int count = UnstreamInt();
                val.resize(count, 0);
                for (auto&& c : val)
                    c = UnstreamByte();
                break;
                }
            }
            rv->left = UnstreamExpression();
            rv->right = UnstreamExpression();
            rv->altData = UnstreamExpression();
        }
    });
    return rv;
}
static BROWSEFILE* UnstreamBrowseFile()
{
    BROWSEFILE* rv = (BROWSEFILE*)Alloc(sizeof(BROWSEFILE));
    UnstreamBlock(STT_BROWSEFILE, [&rv]() {
        rv->name = (const char *)UnstreamTextIndex();
        rv->filenum = UnstreamInt();
    });
    return rv;
}
static BROWSEINFO* UnstreamBrowseInfo()
{
    BROWSEINFO* rv = (BROWSEINFO*)Alloc(sizeof(BROWSEINFO));
    UnstreamBlock(STT_BROWSEINFO, [&rv]() {
        rv->name = (const char *)UnstreamTextIndex();
        rv->filenum = UnstreamInt();
        rv->type = UnstreamInt();
        rv->lineno = UnstreamInt();
        rv->charpos = UnstreamInt();
        rv->flags = UnstreamInt();
    });
    return rv;
}

static AMODE* UnstreamAssemblyOperand()
{
    AMODE *rv = nullptr;
    int mode = UnstreamInt();
    if (mode != am_none)
    {
        rv = (AMODE*)Alloc(sizeof(AMODE));
        rv->mode = (e_am)mode;
        rv->preg = UnstreamInt();
        rv->sreg = UnstreamInt();
        rv->tempflag = UnstreamInt();
        rv->scale = UnstreamInt();
        rv->length = UnstreamInt();
        rv->addrlen = UnstreamInt();
        rv->seg = UnstreamInt();
        UnstreamIntValue(&rv->liveRegs, 8);
        rv->keepesp = UnstreamInt();
        rv->offset = UnstreamExpression();

    }
    return rv;
}
static OCODE* UnstreamAssemblyInstruction()
{
    OCODE *rv = (OCODE*)Alloc(sizeof(OCODE));
    rv->opcode = (e_opcode)UnstreamInt();
    rv->diag = UnstreamInt();
    rv->noopt = UnstreamInt();
    rv->size = UnstreamInt();
    rv->blocknum = UnstreamInt();
    rv->oper1 = UnstreamAssemblyOperand();
    rv->oper2 = UnstreamAssemblyOperand();
    rv->oper3 = UnstreamAssemblyOperand();
    return rv;
}
static IMODE* UnstreamOperand()
{
    IMODE *rv = (IMODE*)Alloc(sizeof(IMODE));
    UnstreamBlock(STT_OPERAND, [&rv]() {
        rv->mode = (i_adr)UnstreamInt();
        rv->scale = UnstreamInt();
        rv->useindx = UnstreamInt();
        rv->size = UnstreamInt();
        rv->ptrsize = UnstreamInt();
        rv->startbit = UnstreamInt();
        rv->bits = UnstreamInt();
        rv->seg = UnstreamInt();
        rv->flags = UnstreamInt();
        rv->offset = UnstreamExpression();
        rv->offset2 = UnstreamExpression();
        rv->offset3 = UnstreamExpression();
        rv->vararg = UnstreamExpression();
    });
    return rv;
}

static QUAD* UnstreamInstruction(FunctionData& fd)
{
    QUAD* rv = (QUAD*)Alloc(sizeof(QUAD));
    UnstreamBlock(STT_BROWSEFILE, [&rv, &fd]() {
        rv->dc.opcode = (i_ops)UnstreamInt();
        rv->block = currentBlock;        
        if (currentBlock)
        {
            currentBlock->tail->fwd = rv;
            rv->back = currentBlock->tail;

            if (rv->dc.opcode != i_block)
                currentBlock->tail = rv;
        }
        if (rv->dc.opcode == i_passthrough)
        {
            rv->dc.left = (IMODE*)UnstreamAssemblyInstruction();
        }
        else
        {
            int i;
            switch (rv->dc.opcode)
            {
            case i_icon:
                UnstreamIntValue(&rv->dc.v.i, 8);
                break;
            case i_imcon:
            case i_fcon:
                UnstreamFloatValue(rv->dc.v.f);
                break;
            case i_cxcon:
                UnstreamFloatValue(rv->dc.v.c.r);
                UnstreamFloatValue(rv->dc.v.c.i);
                break;
            case i_label:
                rv->dc.v.label = UnstreamInt();
                break;
            case i_line:
            {
                i = UnstreamInt();
                LINEDATA* ld  = nullptr, **p = &ld;
                for(;i;i--)
                {
                    *p = (LINEDATA*)Alloc(sizeof(LINEDATA));
                    (*p)->lineno = UnstreamInt();
                    (*p)->line = (const char *)UnstreamTextIndex();
                    p = &(*p)->next;
                }
                rv->dc.left = (IMODE*)ld;
            }
                break;
            case i_block:
                currentBlock = (BLOCK*)Alloc(sizeof(BLOCK));
                rv->dc.v.label = currentBlock->blocknum = UnstreamInt();
                currentBlock->head = currentBlock->tail = rv;
                rv->block = currentBlock;
                break;
            case i_blockend:
                rv->dc.v.label = currentBlock->blocknum = UnstreamInt();
                break;
            case i_dbgblock:
            case i_dbgblockend:
            case i_livein:
                break;
            case i_func:
            {
                rv->dc.v.label = UnstreamInt();
                int n = UnstreamInt();
                if (n)
                    rv->dc.left = fd.imodeList[n - 1];
                break;
            }
            case i_jc:
            case i_jnc:
            case i_jbe:
            case i_ja:
            case i_je:
            case i_jne:
            case i_jge:
            case i_jg:
            case i_jle:
            case i_jl:
            case i_swbranch:
            case i_coswitch:
            case i_goto:
            case i_cmpblock:
                rv->dc.v.label = UnstreamInt();
                // fallthrough
            default:
                int n = UnstreamInt();
                if (n)
                    rv->dc.left = fd.imodeList[n-1];
                n = UnstreamInt();
                if (n)
                    rv->dc.right = fd.imodeList[n - 1];
                break;
            }
            int n = UnstreamInt();
            if (n)
                rv->ans = fd.imodeList[n - 1];
            rv->altsp = (SimpleSymbol*)UnstreamInt();
            rv->alttp = UnstreamType();
            i = UnstreamInt();
            ArgList** p = (ArgList**)&rv->altargs;
            for (;i;i--)
            {
                *p = (ArgList*)Alloc(sizeof(ArgList));
                (*p)->tp = UnstreamType();
                (*p)->exp = UnstreamExpression();
            }
            rv->ansColor = UnstreamInt();
            rv->leftColor = UnstreamInt();
            rv->rightColor = UnstreamInt();
            rv->scaleColor = UnstreamInt();
            rv->flags = UnstreamInt();
            rv->definition = UnstreamInt();
            rv->available = UnstreamInt();
            rv->sourceindx = UnstreamInt();
            rv->copy = UnstreamInt();
            rv->retcount = UnstreamInt();
            rv->sehMode = UnstreamInt();
            rv->fastcall = UnstreamInt();
            rv->oldmode = UnstreamInt();
            rv->novalue = UnstreamInt();
            rv->temps = UnstreamInt();
            rv->precolored = UnstreamInt();
            rv->moved = UnstreamInt();
            rv->livein = UnstreamInt();
            rv->liveRegs = UnstreamInt();
            if (rv->alwayslive)
                rv->block->alwayslive = true;
        }
    });
    return rv;
}
static void UnstreamSymbolList(std::vector<SimpleSymbol*>& list)
{
    int i = UnstreamInt();
    for (; i; i--)
    {
        list.push_back(UnstreamSymbol());
    }
}

static void UnstreamHeader()
{
    char newmagic[sizeof(magic)];
    int vers;
    UnstreamBuffer(newmagic, strlen(magic));
    if (memcmp(newmagic, magic, strlen(magic)) != 0)
        dothrow();
    UnstreamIntValue(&vers, sizeof(vers));
    if (vers != fileVersion)
        dothrow();
    architecture = UnstreamInt();
}
static void UnstreamParams()
{
    UnstreamBlock(SBT_PARAMS, []() {
        UnstreamBuffer(&cparams, sizeof(cparams));
    });
}
static void UnstreamXParams()
{
    UnstreamBlock(SBT_XPARAMS, []() {
        UnstreamString(compilerName);
        UnstreamString(intermediateName);
        UnstreamString(backendName);
        showBanner = UnstreamInt();
        verbosity = UnstreamInt();
        assembling = UnstreamInt();
        dataAlign = UnstreamInt();
        bssAlign = UnstreamInt();
        constAlign = UnstreamInt();
        nextLabel = UnstreamInt();
        registersAssigned = UnstreamInt();
        UnstreamString(prm_assemblerSpecifier);
        UnstreamString(prm_libPath);
        UnstreamString(prm_include);
        std::string temp;
        UnstreamString(outputFileName);
        UnstreamString(prm_OutputDefFile);
        UnstreamString(temp);
        pinvoke_dll = litlate(temp.c_str());
        UnstreamString(prm_snkKeyFile);
        UnstreamString(prm_assemblyVersion);
        UnstreamString(prm_namespace_and_class);
        UnstreamStringList(inputFiles);
        UnstreamStringList(backendFiles);
        UnstreamStringList(libIncludes);
        UnstreamStringList(toolArgs);
        UnstreamStringList(prm_Using);
        int i = UnstreamInt();
        for (; i; i--)
        {
            std::string key, val;
            UnstreamString(key);
            UnstreamString(val);
            bePragma[key] = val;
        }
    });
}
static void UnstreamGlobals()
{
    UnstreamBlock(SBT_GLOBALSYMS, []() {
        UnstreamSymbolList(globalCache);
    });
}
static void UnstreamExternals()
{
    UnstreamBlock(SBT_EXTERNALS, []() {
        UnstreamSymbolList(externals);
    });
}
static void UnstreamTypes()
{
    UnstreamBlock(SBT_TYPES, []() {
        UnstreamSymbolList(typeSymbols);
    });
}
static void UnstreamMSILProperties()
{
    UnstreamBlock(SBT_MSILPROPS, []() {
        int i = UnstreamInt();
        for (;i;i--)
        {
            MsilProperty p;
            p.prop = (SimpleSymbol*)UnstreamInt();
            p.getter = (SimpleSymbol*)UnstreamInt();
            p.setter = (SimpleSymbol*)UnstreamInt();
            msilProperties.push_back(p);
        }
    });
}
static void UnstreamTypedefs()
{
    UnstreamBlock(SBT_TYPEDEFS, []() {
        UnstreamSymbolList(typedefs);
    });

}
static void UnstreamBrowse()
{
    UnstreamBlock(SBT_BROWSEFILES, []() {
        int i = UnstreamInt();
        for (; i; i--)
        {
            browseFiles.push_back(UnstreamBrowseFile());
        }            
    });
    UnstreamBlock(SBT_BROWSEINFO, []() {
        int i = UnstreamInt();
        for (; i; i--)
        {
            browseInfo.push_back(UnstreamBrowseInfo());
        }
    });

}
static QUAD* UnstreamInstructions(FunctionData& fd)
{
    QUAD *rv = nullptr, *last = nullptr;

    int i = UnstreamInt();
    for (; i; i--)
    {
        QUAD *newQuad = UnstreamInstruction(fd);
        if (last)
            last->fwd = newQuad;
        else
            rv = newQuad;
        newQuad->back = last;
        last = newQuad;
    }
    return rv;
}
static void UnstreamIModes(FunctionData& fd)
{
    UnstreamBlock(SBT_IMODES, [&fd]() {
        int len = UnstreamInt();
        for (int i = 0; i < len; i++)
        {
            fd.imodeList.push_back(UnstreamOperand());
        }
    });
}
static void UnstreamTemps()
{
    int i = UnstreamInt();
    for (;i;i--)
    { 
        int temp = UnstreamInt();
        int val = UnstreamByte();
        if (temps[temp])
        {
            if (val & 1)
                temps[temp]->loadTemp = true;
            if (val & 2)
                temps[temp]->pushedtotemp = true;
        }
    }
}
static void UnstreamLoadCache(FunctionData* fd, std::unordered_map<IMODE*, IMODE*>& hash)
{
    int i = UnstreamInt();
    for (; i; i--)
    {
        int n = UnstreamInt();
        if (n)
        {
            IMODE* key = fd->imodeList[n - 1];
            n = UnstreamInt();
            if (n) // might fail in the backend...
            {
                IMODE* value = fd->imodeList[n - 1];
                hash[key] = value;
            }
        }
        else
        {
            n = UnstreamInt();
        }
    }
}
static FunctionData *UnstreamFunc()
{
    currentBlock = nullptr;
    FunctionData *fd = new FunctionData;
    std::vector<SimpleSymbol*> temporarySymbols;
    std::vector<SimpleSymbol*> variables;
    QUAD *instructionList;

    fd->name = (SimpleSymbol*)UnstreamInt();
    int flgs = UnstreamInt();
    if (flgs & 1)
        fd->setjmp_used = true;
    if (flgs & 2)
        fd->hasAssembly = true;
    fd->blockCount = UnstreamInt();
    fd->tempCount = UnstreamInt();
    fd->exitBlock = UnstreamInt();
    temps.clear();
    temps.resize(fd->tempCount);
    UnstreamSymbolList(fd->variables);
    UnstreamSymbolList(fd->temporarySymbols);
    UnstreamIModes(*fd);
    fd->objectArray_exp = UnstreamExpression();
    fd->instructionList = UnstreamInstructions(*fd);
    UnstreamTemps();
    UnstreamLoadCache(fd, fd->loadHash);
    return fd;
}
static void UnstreamData()
{
    UnstreamBlock(SBT_DATA, []() {
        int len = UnstreamInt();
        for (int i=0; i < len; i++)
        {
            BaseData *data = (BaseData*)Alloc(sizeof(BaseData));
            baseData.push_back(data);
            data->type = (DataType)UnstreamInt();
            UnstreamBlock(data->type, [data]() {
                switch (data->type)
                {
                case DT_NONE:
                    break;
                case DT_SEG:
                case DT_SEGEXIT:
                    data->i = UnstreamInt();
                    break;
                case DT_DEFINITION:
                    data->symbol.sym = (SimpleSymbol*)UnstreamInt();
                    data->symbol.i = UnstreamInt();
                    break;
                case DT_LABELDEFINITION:
                    data->i = UnstreamInt();
                    break;
                case DT_RESERVE:
                    data->i = UnstreamInt();
                    break;
                case DT_SYM:
                    data->symbol.sym = (SimpleSymbol*)UnstreamInt();
                    break;
                case DT_SRREF:
                    data->symbol.sym = (SimpleSymbol*)UnstreamInt();
                    data->symbol.i = UnstreamInt();
                    break;
                case DT_PCREF:
                    data->symbol.sym = (SimpleSymbol*)UnstreamInt();
                    break;
                case DT_FUNCREF:
                    data->symbol.sym = (SimpleSymbol*)UnstreamInt();
                    data->symbol.i = UnstreamInt();
                    break;
                case DT_LABEL:
                    data->i = UnstreamInt();
                    break;
                case DT_LABDIFFREF:
                    data->diff.l1 = UnstreamInt();
                    data->diff.l2 = UnstreamInt();
                    break;
                case DT_STRING:
                {
                    bool instring = false;
                    data->astring.i = UnstreamInt();
                    data->astring.str = (char *)Alloc(data->astring.i + 1);
                    for (int i = 0; i < data->astring.i; i++)
                    {
                        data->astring.str[i] = UnstreamByte();
                    }
                }
                break;
                case DT_BIT:
                    break;
                case DT_BOOL:
                    UnstreamIntValue(&data->i, 1);
                    break;
                case DT_BYTE:
                    UnstreamIntValue(&data->i, 1);
                    break;
                case DT_USHORT:
                    UnstreamIntValue(&data->i, 2);
                    break;
                case DT_UINT:
                    UnstreamIntValue(&data->i, 4);
                    break;
                case DT_ULONG:
                    UnstreamIntValue(&data->i, 8);
                    break;
                case DT_ULONGLONG:
                    UnstreamIntValue(&data->i, 8);
                    break;
                case DT_16:
                    UnstreamIntValue(&data->i, 2);
                    break;
                case DT_32:
                    UnstreamIntValue(&data->i, 4);
                    break;
                case DT_ENUM:
                    UnstreamIntValue(&data->i, 4);
                    break;
                case DT_FLOAT:
                    UnstreamFloatValue(data->f);
                    break;
                case DT_DOUBLE:
                    UnstreamFloatValue(data->f);
                    break;
                case DT_LDOUBLE:
                    UnstreamFloatValue(data->f);
                    break;
                case DT_CFLOAT:
                    UnstreamFloatValue(data->c.r);
                    UnstreamFloatValue(data->c.i);
                    break;
                case DT_CDOUBLE:
                    UnstreamFloatValue(data->c.r);
                    UnstreamFloatValue(data->c.i);
                    break;
                case DT_CLONGDOUBLE:
                    UnstreamFloatValue(data->c.r);
                    UnstreamFloatValue(data->c.i);
                    break;
                case DT_ADDRESS:
                    UnstreamIntValue(&data->i, 8);
                    break;
                case DT_VIRTUAL:
                    data->symbol.sym = (SimpleSymbol*)UnstreamInt();
                    data->symbol.i = UnstreamInt();
                    break;
                case DT_ENDVIRTUAL:
                    data->symbol.sym = (SimpleSymbol*)UnstreamInt();
                    break;
                case DT_ALIGN:
                    data->i = UnstreamInt();
                    break;
                case DT_VTT:
                    data->symbol.sym = (SimpleSymbol*)UnstreamInt();
                    data->symbol.i = UnstreamInt();
                    break;
                case DT_IMPORTTHUNK:
                    data->symbol.sym = (SimpleSymbol*)UnstreamInt();
                    break;
                case DT_VC1:
                    data->symbol.sym = (SimpleSymbol*)UnstreamInt();
                    break;
                case DT_AUTOREF:
                    data->symbol.sym = (SimpleSymbol*)UnstreamInt();
                    data->symbol.i = UnstreamInt();
                    break;
                case DT_FUNC:
                    data->funcData = UnstreamFunc();
                    break;
                }
            });
        }
    });
}
void ReadText(std::map<int ,std::string>& texts)
{
    UnstreamBlock(SBT_TEXT, [&texts]() {
        textOffset = UnstreamInt();
        for (int i = 1; i < textOffset;)
        {
            int len = UnstreamInt();
            std::string val;
            val.resize(len, 0);
            for (auto&& c : val)
                c = UnstreamByte();
            texts[i] = val;
            i += len;
        }
    });
}
static SimpleSymbol *SymbolName(SimpleSymbol* selection, std::vector<SimpleSymbol*>* table)
{
    // symbol index was multiplied by two and the low bit was set
    int index = ((int)selection - 1)/2;
    if (index > 0)
    {
        index--;
        if (table == &globalCache && index >= globalCache.size())
        {
            index -= globalCache.size();
            table = &externals;
        }
        else if (current && table == &current->variables && index >= current->variables.size())
        {
            index -= current->variables.size();
            table = &current->temporarySymbols;
        }
        return (*table)[index];
    }
    return nullptr;
}
static void ResolveSymbol(SimpleSymbol*& sym, std::map<int, std::string>& texts, std::vector<SimpleSymbol*>& table);
static void ResolveType(SimpleType* tp, std::map<int, std::string>& texts, std::vector<SimpleSymbol*>& table)
{
    while (tp)
    {
        if (tp->sp)
        {
            ResolveSymbol(tp->sp, texts, typeSymbols);
        }
        tp = tp->btp;
    }
}
static void ResolveExpression(SimpleExpression* exp, std::map<int, std::string>& texts)
{
    if (exp)
    {
        switch (exp->type)
        {
        case se_auto:
            ResolveSymbol(exp->sp, texts, current->variables);
            break;
        case se_const:
        case se_absolute:
        case se_global:
        case se_threadlocal:
        case se_pc:
            ResolveSymbol(exp->sp, texts, globalCache);
            break;
        case se_structelem:
            ResolveSymbol(exp->sp, texts, typeSymbols);
            break;
        case se_msil_array_access:
            exp->sp = SymbolName(exp->sp, &typeSymbols);
            break;
        case se_msil_array_init:
            ResolveType(exp->tp, texts, typeSymbols);
            break;
        }
        ResolveExpression(exp->left, texts);
        ResolveExpression(exp->right, texts);
        ResolveExpression(exp->altData, texts);
    }
}
static void ResolveAssemblyInstruction(OCODE *c, std::map<int, std::string>& texts)
{
    if (c->oper1)
        ResolveExpression(c->oper1->offset, texts);
    if (c->oper2)
        ResolveExpression(c->oper2->offset, texts);
    if (c->oper3)
        ResolveExpression(c->oper3->offset, texts);
}
static void ResolveInstruction(QUAD* q, std::map<int, std::string>& texts)
{
    switch (q->dc.opcode)
    {
    case i_passthrough:
        ResolveAssemblyInstruction((OCODE*)q->dc.left, texts);
        break;
    case i_icon:
    case i_imcon:
    case i_fcon:
    case i_cxcon:
    case i_label:
        break;
    case i_line:
    {
        auto ld = (LINEDATA*)q->dc.left;
        while (ld)
        {
            ld->line = texts[(int)ld->line].c_str();
            ld = ld->next;
        }
        break;
    }
    default:
        break;
    }
    if (q->altsp)
    {
        ResolveSymbol(q->altsp, texts, globalCache);
    }
}
static void ResolveSymbol(std::vector<SimpleSymbol*> symbols, std::map<int, std::string>& texts, std::vector<SimpleSymbol*>& table)
{
    for (auto&& v : symbols)
    {
        ResolveSymbol(v, texts, table);
    }
}
static void ResolveSymbol(SimpleSymbol*& sym, std::map<int, std::string>& texts, std::vector<SimpleSymbol*>& table)
{
    if (sym != nullptr)
    {
        static int val;
        val = (int)sym;
        // low bit set means this is an index not a symbol
        if ((int)sym & 1)
            sym = SymbolName(sym, &table);
        if (sym->visited)
            return;
        sym->visited = true;
        ResolveSymbol(sym->parentClass, texts, typeSymbols);
        for (auto l = sym->syms; l; l = l->next)
        {
            SimpleSymbol *s = (SimpleSymbol*)l->data;
            ResolveSymbol(s, texts, typeSymbols);
            l->data = (void*)s;
        }
        for (auto b = sym->baseClasses; b; b = b->next)
            ResolveSymbol(b->sym, texts, typeSymbols);
        ResolveType(sym->tp, texts, typeSymbols);

        sym->name = texts[(int)sym->name].c_str();
        sym->outputName = texts[(int)sym->outputName].c_str();
        sym->importfile = texts[(int)sym->importfile].c_str();
        sym->namespaceName = texts[(int)sym->namespaceName].c_str();
        sym->msil = texts[(int)sym->msil].c_str();
    }
}
static void ResolveFunction(FunctionData *fd, std::map<int, std::string>& texts)
{
    current = fd;

    ResolveSymbol(fd->name, texts, globalCache);

    ResolveSymbol(fd->variables, texts, fd->variables);
    ResolveSymbol(fd->temporarySymbols, texts, fd->variables);
    for (auto v : fd->imodeList)
    {
        ResolveExpression(v->offset, texts);
        ResolveExpression(v->offset2, texts);
        ResolveExpression(v->offset3, texts);
    }
    for (auto q = fd->instructionList; q; q = q->fwd)
        ResolveInstruction(q, texts);
    lastFunction = current;
    current = nullptr;
}
static void ResolveNames(std::map<int, std::string>& texts)
{
    for (auto&& v : globalCache)
        ResolveSymbol(v, texts, globalCache);
    for (auto&& v : externals)
        ResolveSymbol(v, texts, externals);
    for (auto&& v : typedefs)
        ResolveSymbol(v, texts, typedefs);
    for (auto&& v : typeSymbols)
        ResolveSymbol(v, texts, typeSymbols);
    for (auto&& d : baseData)
    {
        switch (d->type)
        {
        case DT_FUNC:
            ResolveFunction(d->funcData, texts);
            break;
        case DT_AUTOREF:
            current = lastFunction;
            ResolveSymbol(d->symbol.sym, texts, current->variables); // assumes the variables of the last generated function
            current = nullptr;
            break;

        case DT_DEFINITION:
        case DT_SYM:
        case DT_SRREF:
        case DT_PCREF:
        case DT_FUNCREF:
        case DT_VIRTUAL:
        case DT_ENDVIRTUAL:
        case DT_VTT:
        case DT_IMPORTTHUNK:
        case DT_VC1:
            ResolveSymbol(d->symbol.sym, texts, globalCache);
            break;
        }
    }
    for (auto b : browseFiles)
    {
        b->name = texts[(int)b->name].c_str();
    }
    for (auto b : browseInfo)
    {
        b->name = texts[(int)b->name].c_str();

    }

}
bool InputIntermediate()
{
    currentBlock = nullptr;
    texts.clear();
    texts[0] = "";
    textOffset = 1;
    try
    {
        UnstreamHeader();
        UnstreamParams();
        UnstreamXParams();
        UnstreamGlobals();
        UnstreamExternals();
        UnstreamTypes();
        UnstreamBrowse();
        UnstreamMSILProperties();
        UnstreamData();
        ReadText(texts);
        ResolveNames(texts);
        return true;
    }
    catch (std::runtime_error e)
    {
        return false;
    }
}