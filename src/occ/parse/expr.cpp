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

#include "compiler.h"
#include "rtti.h"
#include "PreProcessor.h"

extern COMPILER_PARAMS cparams;
extern ARCH_ASM* chosenAssembler;
extern enum e_kw skim_closepa[], skim_end[];
extern enum e_kw skim_closebr[];
extern enum e_kw skim_semi[];
extern TYPE stdpointer, stdnullpointer, stdchar;
extern TYPE stdint, stdany;
extern TYPE stddouble;
extern TYPE stdvoid, stdfunc;
extern TYPE stdwcharptr;
extern TYPE stdcharptr;
extern TYPE stdfloatimaginary;
extern TYPE stddoubleimaginary;
extern TYPE stdlongdoubleimaginary;
extern TYPE stdbool;
extern TYPE stdunsigned;
extern TYPE stdunsignedlong;
extern TYPE stdlong;
extern TYPE stdunsignedlonglong;
extern TYPE stdlonglong;
extern TYPE stdfloat;
extern TYPE stdlongdouble;
extern TYPE stdchar;
extern TYPE stdwidechar;
extern TYPE stdfloatcomplex;
extern TYPE stddoublecomplex;
extern TYPE stdlongdoublecomplex;
extern TYPE std__func__;
extern TYPE std__string;
extern TYPE stdchar16tptr;
extern TYPE stdchar32tptr;
extern TYPE stdobject;
extern bool setjmp_used;
extern const char* overloadNameTab[];
extern NAMESPACEVALUELIST* localNameSpace;
extern LAMBDA* lambdas;
extern int instantiatingTemplate;
extern int currentErrorLine;
extern int templateNestingCount;
extern NAMESPACEVALUELIST* globalNameSpace;
extern bool hasXCInfo;
extern STRUCTSYM* structSyms;
extern int anonymousNotAlloc;
extern int expandingParams;
extern bool functionCanThrow;
extern SYMBOL* theCurrentFunc;
extern int inGetUserConversion;
extern int tryLevel;
extern bool hasFuncCall;
extern PreProcessor* preProcessor;
extern attributes basisAttribs;
extern bool parsingPreprocessorConstant;
extern bool isCallExit;
int packIndex;

int argument_nesting;

LIST* importThunks;
/* lvaule */
/* handling of const int */
/*--------------------------------------------------------------------------------------------------------------------------------
 */
static EXPRESSION* nodeSizeof(TYPE* tp, EXPRESSION* exp, int flags= 0);
static LEXEME* expression_primary(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags);
static LEXEME* expression_pm(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags);
LEXEME* expression_assign(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags);
static LEXEME* expression_msilfunc(LEXEME* lex, SYMBOL* funcsp, TYPE** tp, EXPRESSION** exp, int flags);

void expr_init(void)
{
    packIndex = -1;
    importThunks = nullptr;
    inGetUserConversion = 0;
}
void thunkForImportTable(EXPRESSION** exp)
{
    SYMBOL* sym;
    if (chosenAssembler->msil)
        return;
    if ((*exp)->type == en_pc)
        sym = (*exp)->v.sp;
    else
        sym = (*exp)->v.func->sp;
    // order is important here as we might get into this function with an already sanitized symbol
    if (sym && sym->attribs.inheritable.linkage2 == lk_import && isfunction(sym->tp) &&
        ((*exp)->type == en_pc || !(*exp)->v.func->ascall))
    {
        LIST* search = importThunks;
        while (search)
        {
            if (((SYMBOL*)search->data)->mainsym == sym)
            {
                *exp = varNode(en_pc, (SYMBOL*)search->data);
                break;
            }
            search = search->next;
        }
        if (!search)
        {
            SYMBOL* newThunk;
            char buf[2048];
            my_sprintf(buf, "@$%s", sym->decoratedName);
            newThunk = makeID(sc_global, &stdpointer, nullptr, litlate(buf));
            newThunk->errname = newThunk->decoratedName = newThunk->name;
            newThunk->mainsym = sym;  // mainsym is the symbol this was derived from
            newThunk->attribs.inheritable.linkage = lk_virtual;
            newThunk->importThunk = true;
            search = (LIST*)(LIST*)Alloc(sizeof(LIST));
            search->next = importThunks;
            search->data = newThunk;
            importThunks = search;
            *exp = varNode(en_pc, (SYMBOL*)search->data);
        }
    }
}
static EXPRESSION* GetUUIDData(SYMBOL* cls)
{
    if (cls && cls->uuidLabel)
    {
        return intNode(en_labcon, cls->uuidLabel);
    }
    else
    {
        EXPRESSION* rv;
        STRING* data = (STRING*)Alloc(sizeof(STRING));
        data->pointers = (SLCHAR**)Alloc(sizeof(SLCHAR*));
        data->size = 1;
        data->strtype = l_astr;
        data->pointers[0] = (SLCHAR*)Alloc(sizeof(SLCHAR));
        data->pointers[0]->count = 16;
        data->pointers[0]->str = (LCHAR*)Alloc(16 * sizeof(LCHAR));
        if (cls && cls->uuid)
        {
            int i;
            for (i = 0; i < 16; i++)
                data->pointers[0]->str[i] = cls->uuid[i];
        }
        rv = stringlit(data);
        if (cls)
            cls->uuidLabel = rv->v.i;
        return rv;
    }
}
static EXPRESSION* GetManagedFuncData(TYPE* tp)
{
    // while managed functions seem to be always stdcall, the caller may be
    // either expecting a stdcall function, or a cdecl function
    // we take care of this by using the 'stdcall' attribute on the function
    // to determine what a caller might expect.
    // this function makes a little thunk table to tell the thunk generator
    // how to set up arguments for the call.  The arguments have to be reversed
    // but we cannot do a naive reversal because arguments of 8 bytes or more have to
    // be copied in order rather than reversed.
    char buf[512], *save = buf;
    int i;
    int sz = 0;
    STRING* data = (STRING*)Alloc(sizeof(STRING));
    SYMBOL* sym;
    SYMLIST* hr;
    if (ispointer(tp))
        tp = basetype(tp)->btp;
    sym = basetype(tp)->sp;
    *save++ = 0;  // space for the number of dwords
    *save++ = 0;
    *save++ = sym->attribs.inheritable.linkage == lk_stdcall;
    hr = basetype(tp)->syms->table[0];
    if (hr && hr->p->tp->type != bt_void)
    {
        sz++;
        hr = hr->next;
        if (hr)
        {
            sz++;
            hr = hr->next;
            while (hr)
            {
                int n = hr->p->tp->size;
                n += 3;
                n /= 4;
                if (n > 32767)
                    diag("GetManagedFuncData: passing too large structure");
                if (n > 0x7f)
                {
                    *save++ = (n >> 8) | 0x80;
                    *save++ = n & 0xff;
                }
                else
                {
                    *save++ = n;
                }
                sz += n;
                hr = hr->next;
            }
        }
    }
    buf[0] = (sz >> 8) | 0x80;
    buf[1] = sz & 0xff;
    data->pointers = (SLCHAR**)Alloc(sizeof(SLCHAR*));
    data->size = 1;
    data->strtype = l_astr;
    data->pointers[0] = (SLCHAR*)Alloc(sizeof(SLCHAR));
    data->pointers[0]->count = save - buf;
    data->pointers[0]->str = (LCHAR*)Alloc((save - buf) * sizeof(SLCHAR));
    for (i = 0; i < save - buf; i++)
        data->pointers[0]->str[i] = buf[i];
    return stringlit(data);
}
void ValidateMSILFuncPtr(TYPE* dest, TYPE* src, EXPRESSION** exp)
{
    bool managedDest = false;
    bool managedSrc = false;
    if ((*exp)->type == en_func && (*exp)->v.func->ascall)
        return;
    // this implementation marshals functions that are put into a pointer variable
    // declared with STDCALL.   If it is not declared with stdcall, then putting it
    // into a structure and passing it to unmanaged code will not work.
    if (isfunction(dest))
    {
        // function arg or assignment to function constant
        managedDest =
            basetype(dest)->sp->attribs.inheritable.linkage2 != lk_unmanaged && chosenAssembler->msil->managed(basetype(dest)->sp);
    }
    else if (isfuncptr(dest))
    {
        // function var
        managedDest = basetype(basetype(dest)->btp)->sp->attribs.inheritable.linkage2 != lk_unmanaged &&
                      chosenAssembler->msil->managed(basetype(basetype(dest)->btp)->sp);
    }
    else
    {
        // unknown
        errortype(ERR_CANNOT_CONVERT_TYPE, src, dest);
    }
    if (isfunction(src))
    {
        // function arg or assignment to function constant
        managedSrc = chosenAssembler->msil->managed(basetype(src)->sp);
    }
    else if (isfuncptr(src))
    {
        // function var
        managedSrc = chosenAssembler->msil->managed(basetype(basetype(src)->btp)->sp);
    }
    else
    {
        // unknown
        errortype(ERR_CANNOT_CONVERT_TYPE, src, dest);
    }
    if (managedDest != managedSrc)
    {
        SYMBOL* sym;
        if (managedSrc)
        {
            sym = gsearch("__OCCMSIL_GetProcThunkToManaged");
            if (sym)
            {
                int n = 0;
                SYMLIST* hr;
                FUNCTIONCALL* functionCall = (FUNCTIONCALL*)(FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
                TYPE* tp1 = src;
                if (ispointer(tp1))
                    tp1 = basetype(tp1)->btp;

                hr = basetype(tp1)->syms->table[0];
                while (hr)
                {
                    int m = hr->p->tp->size;
                    m += 3;
                    m /= 4;
                    n += m;
                    hr = hr->next;
                }
                sym = (SYMBOL*)basetype(sym->tp)->syms->table[0]->p;
                functionCall->sp = sym;
                functionCall->functp = sym->tp;
                functionCall->fcall = varNode(en_pc, sym);
                functionCall->arguments = (INITLIST*)(INITLIST*)Alloc(sizeof(INITLIST));
                functionCall->arguments->tp = &stdpointer;
                functionCall->arguments->exp = *exp;
                functionCall->arguments->next = (INITLIST*)(INITLIST*)Alloc(sizeof(INITLIST));
                functionCall->arguments->next->tp = &stdpointer;
                functionCall->arguments->next->exp = GetManagedFuncData(tp1);
                functionCall->ascall = true;
                *exp = varNode(en_func, nullptr);
                (*exp)->v.func = functionCall;
            }
            else
            {
                diag("ValidateMSILFuncPtr: missing conversion func definition");
            }
        }
        else
        {
            sym = gsearch("__OCCMSIL_GetProcThunkToUnmanaged");
            if (sym)
            {
                FUNCTIONCALL* functionCall = (FUNCTIONCALL*)(FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
                sym = (SYMBOL*)basetype(sym->tp)->syms->table[0]->p;
                functionCall->sp = sym;
                functionCall->functp = sym->tp;
                functionCall->fcall = varNode(en_pc, sym);
                functionCall->arguments = (INITLIST*)(INITLIST*)Alloc(sizeof(INITLIST));
                functionCall->arguments->tp = &stdpointer;
                functionCall->arguments->exp = *exp;
                functionCall->ascall = true;
                *exp = varNode(en_func, nullptr);
                (*exp)->v.func = functionCall;
            }
            else
            {
                diag("ValidateMSILFuncPtr: missing conversion func definition");
            }
        }
    }
}
EXPRESSION* exprNode(enum e_node type, EXPRESSION* left, EXPRESSION* right)
{
    EXPRESSION* rv = (EXPRESSION*)Alloc(sizeof(EXPRESSION));
    rv->type = type;
    rv->left = left;
    rv->right = right;
    return rv;
}
EXPRESSION* varNode(enum e_node type, SYMBOL* sym)
{
    EXPRESSION* rv = (EXPRESSION*)Alloc(sizeof(EXPRESSION));
    rv->type = type;
    rv->v.sp = sym;
    return rv;
}
static bool inreg(EXPRESSION* exp, bool first)
{
    while (castvalue(exp))
        exp = exp->left;
    if (first && lvalue(exp))
    {
        exp = exp->left;
        first = false;
    }
    if (exp->type == en_auto)
    {
        return exp->v.sp->storage_class == sc_register;
    }
    else if (exp->type == en_add || exp->type == en_arrayadd || exp->type == en_arraymul || exp->type == en_lsh ||
             exp->type == en_structadd || exp->type == en_arraylsh)
        return inreg(exp->left, first) || inreg(exp->right, first);
    else
        return false;
}
EXPRESSION* typeNode(TYPE* tp)
{
    EXPRESSION* rv = (EXPRESSION*)Alloc(sizeof(EXPRESSION));
    rv->type = en_type;
    rv->v.tp = tp;
    return rv;
}
EXPRESSION* intNode(enum e_node type, LLONG_TYPE val)
{
    EXPRESSION* rv = (EXPRESSION*)Alloc(sizeof(EXPRESSION));
    rv->type = type;
    rv->v.i = val;
    return rv;
}
void checkauto(TYPE* tp1, int err)
{
    if (isautotype(tp1))
    {
        error(err);
        while (tp1->type == bt_const || tp1->type == bt_volatile || tp1->type == bt_lrqual || tp1->type == bt_rrqual)
        {
            tp1->size = getSize(bt_int);
            tp1 = tp1->btp;
        }
        tp1->type = bt_int;
        tp1->size = getSize(bt_int);
    }
}
static void tagNonConst(SYMBOL* sym, TYPE* tp)
{
    if (sym && tp)
    {
        if (isref(tp))
            tp = basetype(tp)->btp;
        sym->nonConstVariableUsed |= !isconst(tp);
    }
}
static LEXEME* variableName(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    char idname[512];
    FUNCTIONCALL* funcparams = nullptr;
    SYMBOL* sym = nullptr;
    SYMBOL* strSym = nullptr;
    NAMESPACEVALUELIST* nsv = nullptr;
    LEXEME* placeholder = lex;
    if (ismutable)
        *ismutable = false;
    if (cparams.prm_cplusplus || (chosenAssembler->msil && chosenAssembler->msil->allowExtensions))
    {
        lex = id_expression(lex, funcsp, &sym, &strSym, &nsv, nullptr, false, false, idname);
    }
    else
    {
        sym = gsearch(lex->value.s.a);
    }
    if (sym)
    {

        SYMLIST* hr;
        browse_usage(sym, lex->filenum);
        *tp = sym->tp;
        lex = getsym();
        if (sym->attribs.uninheritable.deprecationText)
        {
            deprecateMessage(sym);
        }
        switch (sym->storage_class)
        {
            case sc_member:
            case sc_mutable:
                sym = lambda_capture(sym, cmThis, false);
                break;
            case sc_auto:
            case sc_register:
            case sc_parameter:
                sym = lambda_capture(sym, cmNone, false);
                break;
            case sc_constant:
                if ((flags & _F_AMPERSAND) && sym->parent)
                    sym = lambda_capture(sym, cmNone, false);
                break;
            default:
                break;
        }
        if (sym->templateLevel && !isfunction(sym->tp) && sym->storage_class != sc_type && !ismember(sym) && MATCHKW(lex, lt))
        {
            lex = getsym();
            if (startOfType(lex, false))
            {
                TEMPLATEPARAMLIST* lst = nullptr;
                SYMBOL* sp1 = sym;
                lex = backupsym();
                lex = GetTemplateArguments(lex, funcsp, sp1, &lst);
                sp1 = GetVariableTemplate(sp1, lst);
                if (sp1)
                {
                    sym = sp1;
                    *tp = sym->tp;
                }
                else
                    errorsym(ERR_NO_TEMPLATE_MATCHES, sym);
            }
            else
            {
                lex = backupsym();
            }
        }
        if (/*instantiatingTemplate &&*/ sym->storage_class == sc_templateparam && sym->tp->type == bt_templateparam &&
            sym->tp->templateParam->p->byNonType.val)
        {
            switch (sym->tp->templateParam->p->type)
            {
                case kw_typename:
                case kw_template:
                    lex = prevsym(placeholder);
                    *tp = nullptr;
                    if ((flags & (_F_SIZEOF | _F_PACKABLE)) == (_F_SIZEOF | _F_PACKABLE))
                    {
                        *exp = varNode(en_templateparam, sym);
                        *tp = sym->tp;
                        lex = getsym();
                    }
                    else
                    {
                        lex = expression_func_type_cast(lex, funcsp, tp, exp, flags);
                    }
                    return lex;
                case kw_int:
                    *exp = sym->tp->templateParam->p->byNonType.val;
                    *tp = sym->tp->templateParam->p->byNonType.tp;
                    if (!*exp)
                        *exp = intNode(en_c_i, 0);
                    if ((*tp)->type == bt_templateparam)
                    {
                        TYPE* tp1 = (*tp)->templateParam->p->byClass.val;
                        if (tp1)
                        {
                            *tp = tp1;
                        }
                        else
                        {
                            STRUCTSYM* s = structSyms;
                            SYMBOL* rv = nullptr;
                            while (s && !rv)
                            {
                                if (s->tmpl)
                                    rv = templatesearch((*tp)->templateParam->argsym->name, s->tmpl);
                                s = s->next;
                            }
                            if (rv && rv->tp->templateParam->p->type == kw_typename)
                                *tp = rv->tp->templateParam->p->byClass.val;
                        }
                    }
                    return lex;
                default:
                    break;
            }
        }
        else
        {
            if (sym->tp->type == bt_templateparam)
            {

                if ((sym->storage_class == sc_parameter || sym->tp->templateParam->p->type == kw_int) &&
                    sym->tp->templateParam->p->packed)
                {
                    if (packIndex >= 0)
                    {
                        TEMPLATEPARAMLIST* templateParam = sym->tp->templateParam->p->byPack.pack;
                        int i;
                        for (i = 0; i < packIndex && templateParam; i++)
                            templateParam = templateParam->next;
                        if (templateParam)
                        {
                            if (templateParam->p->type == kw_int && !templateParam->p->packsym)
                            {
                                *tp = templateParam->p->byNonType.tp;
                                *exp = templateParam->p->byNonType.val;
                            }
                            else
                            {
                                sym = templateParam->p->packsym;
                                *tp = sym->tp;
                                *exp = varNode(en_auto, sym);
                            }
                        }
                        else
                        {
                            *exp = intNode(en_packedempty, 0);
                        }
                    }
                    else
                    {
                        sym->packed = true;
                        *exp = varNode(en_auto, sym);
                    }
                }
                else
                {
                    *exp = varNode(en_templateparam, sym);
                }
            }
            else
                switch (sym->storage_class)
                {
                    case sc_mutable:
                        if (ismutable)
                            *ismutable = true;
                    case sc_member:
                        /*
                        if (flags & _F_SIZEOF)
                        {
                            *exp = intNode(en_c_i, 0);
                        }
                        else
                        */
                        if (cparams.prm_cplusplus && (flags & _F_AMPERSAND) && strSym)
                        {
                            *exp = getMemberPtr(sym, strSym, tp, funcsp);
                        }
                        else
                        {
                            *exp = getMemberNode(sym, strSym, tp, funcsp);
                        }
                        break;
                    case sc_type:
                    case sc_typedef:
                        lex = prevsym(placeholder);
                        *tp = nullptr;
                        lex = expression_func_type_cast(lex, funcsp, tp, exp, flags);
                        if (!*exp)
                            *exp = intNode(en_c_i, 0);
                        return lex;
                    case sc_overloads:

                        hr = basetype(sym->tp)->syms->table[0];
                        funcparams = (FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
                        if (cparams.prm_cplusplus && MATCHKW(lex, lt))
                        {
                            lex = GetTemplateArguments(lex, funcsp, sym, &funcparams->templateParams);
                            funcparams->astemplate = true;
                        }
                        if (hr->next || cparams.prm_cplusplus)
                        {
                            funcparams->ascall = MATCHKW(lex, openpa);
                            funcparams->sp = sym;
                        }
                        else
                        {
                            // we only get here for C language, sadly we have to do
                            // argument based lookup in C++...
                            funcparams->sp = hr->p;
                            
                            funcparams->sp->attribs.inheritable.used = true;
                            funcparams->fcall = varNode(en_pc, funcparams->sp);
                            if (!MATCHKW(lex, openpa))
                                funcparams->sp->dumpInlineToFile = funcparams->sp->isInline;
                        }
                        funcparams->functp = funcparams->sp->tp;
                        *tp = funcparams->sp->tp;
                        funcparams->asaddress = !!(flags & _F_AMPERSAND);
                        if (cparams.prm_cplusplus && ismember(basetype(*tp)->sp) && !MATCHKW(lex, openpa))
                        {
                            EXPRESSION* exp1 = (EXPRESSION*)Alloc(sizeof(EXPRESSION));
                            exp1->type = en_memberptr;
                            exp1->left = *exp;
                            exp1->v.sp = funcparams->sp;
                            *exp = exp1;
                            getMemberPtr(sym, strSym, tp, funcsp);
                        }
                        else
                        {
                            *exp = varNode(en_func, nullptr);
                            (*exp)->v.func = funcparams;
                        }
                        break;
                    case sc_catchvar:
                        makeXCTab(funcsp);
                        *exp = varNode(en_auto, funcsp->xc->xctab);
                        *exp = exprNode(en_add, *exp, intNode(en_c_i, XCTAB_INSTANCE_OFS));
                        deref(&stdpointer, exp);
                        break;
                    case sc_enumconstant:
                        *exp = intNode(en_c_i, sym->value.i);
                        break;
                    case sc_constant:
                        *exp = varNode(en_const, sym);
                        break;
                    case sc_auto:
                    case sc_register: /* register variables are treated as
                                       * auto variables in this compiler
                                       * of course the usage restraints of the
                                       * register keyword are enforced elsewhere
                                       */
                        *exp = varNode(en_auto, sym);
                        sym->anyTry |= tryLevel != 0;
                        break;
                    case sc_parameter:
                        //                   tagNonConst(funcsp, sym->tp);
                        if (sym->packed)
                        {
                            if (!(flags & _F_PACKABLE))
                                error(ERR_PACK_SPECIFIER_MUST_BE_USED_IN_ARGUMENT);
                            if (packIndex >= 0)
                            {
                                TYPE* tp1 = sym->tp;
                                TEMPLATEPARAMLIST* templateParam;
                                int i;
                                while (ispointer(tp1) || isref(tp1))
                                    tp1 = basetype(tp1)->btp;
                                tp1 = basetype(tp1);
                                if (tp1->type == bt_templateparam)
                                {
                                    templateParam = tp1->templateParam->p->byPack.pack;
                                    for (i = 0; i < packIndex && templateParam; i++)
                                        templateParam = templateParam->next;
                                    if (templateParam)
                                    {
                                        sym = templateParam->p->packsym;
                                        *tp = sym->tp;
                                        *exp = varNode(en_auto, sym);
                                    }
                                    else
                                    {
                                        *exp = intNode(en_packedempty, 0);
                                    }
                                }
                                else
                                {
                                    SYMLIST* found = nullptr;
                                    HASHTABLE* tables = localNameSpace->valueData->syms;
                                    while (tables && !found)
                                    {
                                        SYMLIST* hr = tables->table[0];
                                        while (hr && !found)
                                        {
                                            if (hr->p == sym)
                                                found = hr;
                                            hr = hr->next;
                                        }
                                        tables = tables->next;
                                    }
                                    if (found)
                                    {
                                        int i;
                                        for (i = 0; found && i < packIndex; i++)
                                            found = found->next;
                                        if (found)
                                        {
                                            sym = found->p;
                                        }
                                        *exp = varNode(en_auto, sym);
                                        *tp = sym->tp;
                                    }
                                    else
                                    {
                                        *exp = intNode(en_packedempty, 0);
                                    }
                                }
                            }
                            else
                            {
                                *exp = varNode(en_auto, sym);
                            }
                        }
                        else
                        {
                            *exp = varNode(en_auto, sym);
                        }
                        /* derefereance parameters which are declared as arrays */
                        {
                            TYPE* tpa = basetype(sym->tp);
                            if (isref(tpa))
                                tpa = basetype(tpa->btp);
                            if (tpa->array)
                                deref(&stdpointer, exp);
                        }
                        sym->anyTry |= tryLevel != 0;
                        break;

                    case sc_localstatic:
                        tagNonConst(funcsp, sym->tp);
                        if (!(flags & _F_SIZEOF))
                            GENREF(sym);
                        if (funcsp && funcsp->isInline)
                        {
                            if (funcsp->promotedToInline || cparams.prm_cplusplus)
                            {
                                funcsp->isInline = funcsp->dumpInlineToFile = funcsp->promotedToInline = false;
                            }
                        }
                        if (sym->attribs.inheritable.linkage3 == lk_threadlocal)
                        {
                            funcsp->nonConstVariableUsed = true;
                            *exp = varNode(en_threadlocal, sym);
                        }
                        else
                        {
                            *exp = varNode(en_global, sym);
                        }
                        sym->attribs.inheritable.used = true;
                        break;
                    case sc_absolute:
                        funcsp->nonConstVariableUsed = true;
                        *exp = varNode(en_absolute, sym);
                        break;
                    case sc_static:
                        sym->attribs.inheritable.used = true;
                    case sc_global:
                    case sc_external:
                        tagNonConst(funcsp, sym->tp);
                        if (strSym)
                        {
                            SYMBOL* tpl = sym;
                            while (tpl)
                            {
                                if (tpl->templateLevel)
                                    break;
                                tpl = tpl->parentClass;
                            }
                            if (tpl && tpl->instantiated)
                            {
                                TemplateDataInstantiate(sym, false, false);
                            }
                        }
                        if (!(flags & _F_SIZEOF))
                            GENREF(sym);
                        if (sym->parentClass && !isExpressionAccessible(nullptr, sym, funcsp, nullptr, false))
                            errorsym(ERR_CANNOT_ACCESS, sym);
                        if (sym->attribs.inheritable.linkage3 == lk_threadlocal)
                            *exp = varNode(en_threadlocal, sym);
                        else
                            *exp = varNode(en_global, sym);
                        if (sym->attribs.inheritable.linkage2 == lk_import)
                        {
                            //                        *exp = exprNode(en_add, *exp, intNode(en_c_i, 2));
                            //                        deref(&stdpointer, exp);
                            deref(&stdpointer, exp);
                        }
                        break;
                    case sc_namespace:
                    case sc_namespacealias:
                        errorsym(ERR_INVALID_USE_OF_NAMESPACE, sym);
                        *exp = intNode(en_c_i, 1);
                        break;
                    default:
                        error(ERR_IDENTIFIER_EXPECTED);
                        *exp = intNode(en_c_i, 1);
                        break;
                }
        }
        sym->tp->used = true;
        if (sym->templateLevel && istype(sym))
        {
            lex = prevsym(placeholder);
            *tp = nullptr;
            lex = expression_func_type_cast(lex, funcsp, tp, exp, flags);
        }
        else
        {
            if (sym->tp->type == bt_templateparam)
            {
                if (*exp && (*exp)->type != en_packedempty && !sym->tp->templateParam->p->packed)
                {
                    if (sym->tp->templateParam->p->type == kw_int)
                        *tp = sym->tp->templateParam->p->byNonType.tp;
                    else
                        *tp = &stdint;
                    if (sym->templateLevel && sym->storage_class != sc_type && !ismember(sym) && !isfunction(sym->tp))
                    {
                        if (!(flags & _F_AMPERSAND))
                            deref(&stdint, exp);
                    }
                    else if (!templateNestingCount)
                    {
                        *exp = intNode(en_c_i, 0);
                    }
                    if (MATCHKW(lex, openpa) || MATCHKW(lex, begin))
                    {
                        lex = prevsym(placeholder);
                        TYPE* tp1 = nullptr;
                        EXPRESSION* exp1;
                        lex = expression_func_type_cast(lex, funcsp, &tp1, &exp1, flags);
                        return lex;
                    }
                }
            }
            else if (sym->tp->type == bt_any)
                deref(&stdint, exp);
            else
            {
                if (isref(*tp))
                {
                    TYPE* tp1 = *tp;
                    deref(*tp, exp);
                    while (isref(tp1))
                        tp1 = basetype(tp1)->btp;
                    *tp = (TYPE*)Alloc(sizeof(TYPE));
                    **tp = *tp1;
                    UpdateRootTypes(*tp);
                }
                if (sym->storage_class != sc_overloads)
                {
                    if (!isstructured(*tp) && basetype(*tp)->type != bt_memberptr && !isfunction(*tp) &&
                        sym->storage_class != sc_constant && sym->storage_class != sc_enumconstant && sym->tp->type != bt_void)
                    {
                        if (!(*tp)->array || (*tp)->vla || (!(*tp)->msil && sym->storage_class == sc_parameter))
                            if ((*tp)->vla)
                                deref(&stdpointer, exp);
                            else
                                deref(*tp, exp);
                        else if ((*tp)->array && inreg(*exp, true))
                            error(ERR_CANNOT_TAKE_ADDRESS_OF_REGISTER);
                    }
                }
            }

            if (lvalue(*exp) && (*exp)->type != en_l_object)
                (*exp)->v.sp = sym;  // catch for constexpr
            (*exp)->pragmas = preProcessor->GetStdPragmas();
            if (isvolatile(*tp))
                (*exp)->isvolatile = true;
            if (isrestrict(*tp))
                (*exp)->isrestrict = true;
            if (isatomic(*tp))
            {
                (*exp)->isatomic = true;
                if (needsAtomicLockFromType(*tp))
                    (*exp)->lockOffset = basetype(*tp)->size;
            }
            if (strSym && funcparams)
                funcparams->novtab = true;
            if (cparams.prm_cplusplus && (sym->storage_class == sc_member || sym->storage_class == sc_mutable))
            {
                qualifyForFunc(funcsp, tp, *ismutable);
            }
        }
    }
    else if (parsingPreprocessorConstant)
    {
        *tp = &stdint;
        *exp = intNode(en_c_i, 0);
        lex = getsym();
    }
    else
    {
        char* name;
        if (strSym && strSym->tp->type == bt_templateselector)
        {
            *exp = exprNode(en_templateselector, nullptr, nullptr);
            (*exp)->v.templateSelector = strSym->tp->sp->templateSelector;
            *tp = &stdany;
            lex = getsym();
            if (MATCHKW(lex, lt))
            {
                TEMPLATEPARAMLIST* current = nullptr;
                lex = GetTemplateArguments(lex, funcsp, nullptr, &current);
            }
            return lex;
        }
        IncGlobalFlag();
        if (ISID(lex))
            name = litlate(lex->value.s.a);
        else
            name = litlate("__unknown");
        sym = (SYMBOL*)Alloc(sizeof(SYMBOL));
        sym->name = name;
        sym->attribs.inheritable.used = true;
        sym->declfile = sym->origdeclfile = lex->file;
        sym->declline = sym->origdeclline = lex->line;
        sym->realdeclline = lex->realline;
        sym->declfilenum = lex->filenum;
        lex = getsym();
        if (MATCHKW(lex, openpa))
        {
            if (cparams.prm_cplusplus)
            {
                sym->storage_class = sc_overloads;
                (*tp) = (TYPE*)Alloc(sizeof(TYPE));
                (*tp)->type = bt_aggregate;
                UpdateRootTypes(*tp);
                (*tp)->sp = sym;
            }
            else
            {
                /* no prototype error will be added later */
                sym->storage_class = sc_external;
                sym->attribs.inheritable.linkage = lk_c;
                sym->tp = (TYPE*)Alloc(sizeof(TYPE));
                sym->tp->type = bt_func;
                sym->tp->size = getSize(bt_pointer);
                sym->tp->syms = CreateHashTable(1);
                sym->tp->sp = sym;
                sym->tp->btp = (TYPE*)Alloc(sizeof(TYPE));
                sym->tp->rootType = sym->tp;
                sym->tp->btp->rootType = sym->tp->btp;
                sym->oldstyle = true;
                sym->tp->btp->type = bt_int;
                sym->tp->btp->size = getSize(bt_int);
                InsertExtern(sym);
                *tp = sym->tp;
            }
            SetLinkerNames(sym, lk_c);
            if (sym->storage_class != sc_overloads)
            {
                InsertSymbol(sym, sym->storage_class, lk_none, false);
                *exp = varNode(sym->storage_class == sc_auto ? en_auto : en_global, sym);
            }
            else
            {
                sym->tp = (TYPE*)Alloc(sizeof(TYPE));
                sym->tp->type = bt_func;
                sym->tp->size = getSize(bt_pointer);
                sym->tp->syms = CreateHashTable(1);
                sym->tp->sp = sym;
                sym->tp->btp = (TYPE*)Alloc(sizeof(TYPE));
                sym->oldstyle = true;
                sym->tp->btp->type = bt_int;
                sym->tp->btp->size = getSize(bt_int);
                sym->tp->rootType = sym->tp;
                sym->tp->btp->rootType = sym->tp->btp;
                funcparams = (FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
                funcparams->sp = sym;
                funcparams->functp = sym->tp;
                funcparams->fcall = intNode(en_c_i, 0);
                *tp = sym->tp;
                *exp = varNode(en_func, nullptr);
                (*exp)->v.func = funcparams;
            }
        }
        else
        {
            if (cparams.prm_cplusplus && atp && isfuncptr(atp))
            {
                SYMBOL* sym;
                sym->storage_class = sc_overloads;
                (*tp) = (TYPE*)Alloc(sizeof(TYPE));
                (*tp)->type = bt_aggregate;
                UpdateRootTypes(*tp);
                (*tp)->sp = sym;
                DecGlobalFlag();
                funcparams = (FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
                funcparams->ascall = true;
                sym = GetOverloadedFunction(tp, &funcparams->fcall, sym, nullptr, atp, true, false, true, flags);
                if (sym)
                {
                    sym->throughClass = sym->throughClass;
                    sym = sym;
                    if (!isExpressionAccessible(nullptr, sym, funcsp, nullptr, false))
                        errorsym(ERR_CANNOT_ACCESS, sym);
                }
                funcparams->sp = sym;
                funcparams->functp = funcparams->sp->tp;
                *exp = varNode(en_func, nullptr);
                (*exp)->v.func = funcparams;
                IncGlobalFlag();
            }
            else
            {
                sym->storage_class = funcsp ? sc_auto : sc_global;
                sym->tp = (TYPE*)Alloc(sizeof(TYPE));
                sym->tp->type = bt_any;
                sym->tp->rootType = sym->tp;
                sym->parentClass = strSym;
                *tp = sym->tp;
                deref(&stdint, exp);
                SetLinkerNames(sym, lk_c);
                if (!nsv && (!strSym || !templateNestingCount ||
                             (!strSym->templateLevel && strSym->tp->type != bt_templateselector &&
                              strSym->tp->type != bt_templatedecltype)))
                {
                    // no error if there are packed templates and we aren't parsing them
                    bool found = false;
                    if (!expandingParams)
                    {
                        SYMBOL* spx = strSym;
                        while (spx && !found)
                        {
                            TEMPLATEPARAMLIST* tpl = spx->templateParams;
                            while (tpl && !found)
                            {
                                if (tpl->p->packed)
                                    found = true;
                                tpl = tpl->next;
                            }
                            spx = spx->parentClass;
                        }
                    }
                    if (!found)
                    {
                        errorstr(ERR_UNDEFINED_IDENTIFIER, name);
                        if (sym->storage_class != sc_overloads &&
                            (localNameSpace->valueData->syms || sym->storage_class != sc_auto))
                            InsertSymbol(sym, sym->storage_class, lk_none, false);
                    }
                }
                if (nsv)
                {
                    errorNotMember(strSym, nsv, sym->name);
                }
                if (sym->storage_class != sc_overloads)
                {
                    *exp = varNode(en_global, sym);
                }
                else
                {
                    funcparams = (FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
                    *exp = varNode(en_func, nullptr);
                    (*exp)->v.func = funcparams;
                }
            }
        }
        DecGlobalFlag();
    }
    if (!*exp)
        *exp = intNode(en_c_i, 0);
    if (sym)
        sym->allocate = true;
    return lex;
}
static LEXEME* expression_member(LEXEME* lex, SYMBOL* funcsp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    TYPE *typein = *tp, *typein2 = isarray(typein) ? typein : nullptr;
    bool points = false;
    const char* tokenName = ".";
    if (ISKW(lex))
        tokenName = lex->kw->name;
    (void)funcsp;
    // find structured version of arithmetic types for msil member matching
    if (chosenAssembler->msil && chosenAssembler->msil->allowExtensions &&
        (isarithmetic(*tp) || (*tp)->type == bt___string || (isarray(*tp) && basetype(*tp)->msil)) &&
        chosenAssembler->msil->find_boxed_type)
    {
        // auto-boxing for msil
        TYPE* tp1 = chosenAssembler->msil->find_boxed_type(basetype(*tp));
        if (tp1)
        {
            while (castvalue(*exp))
                *exp = (*exp)->left;
            if (!lvalue(*exp))
                *exp = msilCreateTemporary(*tp, *exp);
            else
                *exp = (*exp)->left;
            *tp = tp1;
            typein = tp1;
        }
    }
    if (MATCHKW(lex, pointsto))
    {
        TYPE* nesting[100];
        int n = 0;
        points = true;
        do
        {
            if (isstructured(*tp))
            {
                TYPE* x = basetype(*tp);
                int i;
                for (i = 0; i < n; i++)
                    if (nesting[i] == x)
                    {
                        break;
                    }
                nesting[n++] = x;
                if (n >= sizeof(nesting) / sizeof(nesting[0]))
                {
                    break;
                }
            }
        } while ((cparams.prm_cplusplus || chosenAssembler->msil) &&
                 insertOperatorFunc(ovcl_pointsto, pointsto, funcsp, tp, exp, nullptr, nullptr, nullptr, flags));
        typein = *tp;
        if (ispointer(*tp))
        {
            *tp = basetype(*tp);
            *tp = (*tp)->btp;
        }
    }
    *tp = PerformDeferredInitialization(*tp, funcsp);
    if (MATCHKW(lex, dot) || MATCHKW(lex, pointsto))
        lex = getsym();
    if (cparams.prm_cplusplus && MATCHKW(lex, complx))
    {
        // direct destructor or psuedo-destructor
        enum e_lk linkage = lk_none, linkage2 = lk_none, linkage3 = lk_none;
        bool defd = false;
        bool notype = false;
        TYPE* tp1 = nullptr;
        lex = getsym();
        lex = getBasicType(lex, funcsp, &tp1, nullptr, false, sc_auto, &linkage, &linkage2, &linkage3, ac_public, &notype, &defd,
                           nullptr, nullptr, false, true);
        if (!tp1)
        {
            error(ERR_TYPE_NAME_EXPECTED);
        }
        else if (!comparetypes(*tp, tp1, true))
        {
            if (!templateNestingCount)
                error(ERR_DESTRUCTOR_MUST_MATCH_CLASS);
        }
        else if (isstructured(*tp))
        {
            // destructor
            SYMBOL* sp2 = search(overloadNameTab[CI_DESTRUCTOR], (basetype(*tp)->sp)->tp->syms);
            if (sp2)
            {
                callDestructor(basetype(*tp)->sp, nullptr, exp, nullptr, true, false, false);
            }
            if (needkw(&lex, openpa))
                needkw(&lex, closepa);
        }
        else
        {
            // psuedo-destructor, no further activity required.
            if (needkw(&lex, openpa))
                needkw(&lex, closepa);
        }
        *tp = &stdvoid;
    }
    else if (!isstructured(*tp) || (points && !ispointer(typein)))
    {
        if (cparams.prm_cplusplus && ISKW(lex) && (lex->kw->tokenTypes & TT_BASETYPE))
        {
            // possible psuedo destructor with selector
            enum e_lk linkage = lk_none, linkage2 = lk_none, linkage3 = lk_none;
            bool defd = false;
            bool notype = false;
            TYPE* tp1 = nullptr;
            lex = getBasicType(lex, funcsp, &tp1, nullptr, false, sc_auto, &linkage, &linkage2, &linkage3, ac_public, &notype,
                               &defd, nullptr, nullptr, false, true);
            if (!tp1)
            {
                error(ERR_TYPE_NAME_EXPECTED);
            }
            else if (!comparetypes(*tp, tp1, true))
            {
                errortype(ERR_CANNOT_CONVERT_TYPE, tp1, *tp);
            }
            if (!MATCHKW(lex, classsel))
            {
                error(ERR_INVALID_PSUEDO_DESTRUCTOR);
            }
            else
            {
                lex = getsym();
                if (!MATCHKW(lex, complx))
                {
                    error(ERR_INVALID_PSUEDO_DESTRUCTOR);
                }
                else
                {
                    lex = getsym();
                    tp1 = nullptr;
                    lex = getBasicType(lex, funcsp, &tp1, nullptr, false, sc_auto, &linkage, &linkage2, &linkage3, ac_public,
                                       &notype, &defd, nullptr, nullptr, false, true);
                    if (!tp1)
                    {
                        error(ERR_TYPE_NAME_EXPECTED);
                    }
                    else if (!comparetypes(*tp, tp1, true))
                    {
                        error(ERR_DESTRUCTOR_MUST_MATCH_CLASS);
                    }
                    else if (needkw(&lex, openpa))
                        needkw(&lex, closepa);
                }
            }
            *tp = &stdvoid;
        }
        else
        {
            if (points)
                errorstr(ERR_POINTER_TO_STRUCTURE_EXPECTED, tokenName);
            else
                errorstr(ERR_STRUCTURED_TYPE_EXPECTED, tokenName);
            while (ISID(lex) || MATCHKW(lex, kw_operator))
            {
                TYPE* tp = nullptr;
                EXPRESSION* exp = nullptr;
                lex = expression_pm(lex, funcsp, nullptr, &tp, &exp, nullptr, 0);
                if (!MATCHKW(lex, pointsto) && !MATCHKW(lex, dot))
                    break;
                lex = getsym();
            }
        }
    }
    else
    {
        bool isTemplate = false;
        if (MATCHKW(lex, kw_template))
        {
            lex = getsym();
            isTemplate = true;
        }
        if (!ISID(lex) && !MATCHKW(lex, kw_operator))
        {
            error(ERR_IDENTIFIER_EXPECTED);
        }
        else
        {
            SYMBOL* sp2 = nullptr;
            if ((*exp)->type == en_literalclass)
            {
                CONSTEXPRSYM* ces = (CONSTEXPRSYM*)search(lex->value.s.a, (*exp)->v.syms);
                if (ces)
                {
                    lex = getsym();
                    *exp = ces->exp;
                    *tp = ces->sp->tp;
                    return lex;
                }
                else
                {
                    STRUCTSYM l;
                    l.str = basetype(*tp)->sp;
                    addStructureDeclaration(&l);
                    lex = id_expression(lex, funcsp, &sp2, nullptr, nullptr, &isTemplate, false, true, nullptr);
                    dropStructureDeclaration();
                    if (!sp2)
                    {
                        errorNotMember(basetype(*tp)->sp, nullptr, lex->value.s.a);
                        lex = getsym();
                        while (ISID(lex))
                        {
                            lex = getsym();
                            if (!MATCHKW(lex, pointsto) && !MATCHKW(lex, dot))
                                break;
                            lex = getsym();
                        }
                        *exp = intNode(en_c_i, 0);
                        *tp = &stdint;
                        return lex;
                    }
                    lex = getsym();
                    if (sp2->tp->type != bt_aggregate)
                    {
                        error(ERR_CONSTANT_FUNCTION_EXPECTED);
                        *exp = intNode(en_c_i, 0);
                        *tp = &stdint;
                        return lex;
                    }
                    if (MATCHKW(lex, openpa))
                    {
                        FUNCTIONCALL* funcparams = (FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
                        SYMBOL* match;
                        TYPE* tp1 = nullptr;
                        EXPRESSION* exp1 = nullptr;
                        lex = getArgs(lex, funcsp, funcparams, closepa, true, flags);
                        funcparams->thisptr = intNode(en_c_i, 0);
                        funcparams->thistp = (TYPE*)Alloc(sizeof(TYPE));
                        funcparams->thistp->type = bt_pointer;
                        funcparams->thistp->size = getSize(bt_pointer);
                        funcparams->thistp->btp = *tp;
                        funcparams->thistp->rootType = funcparams->thistp;
                        funcparams->ascall = true;
                        match = GetOverloadedFunction(&tp1, &exp1, sp2, funcparams, nullptr, true, false, true, flags);
                        if (match)
                        {
                            funcparams->sp = match;
                            funcparams->functp = match->tp;
                            *exp = substitute_params_for_function(funcparams, (*exp)->v.syms);
                            optimize_for_constants(exp);
                            *tp = basetype(match->tp)->btp;
                            if (!match->constexpression || !IsConstantExpression(*exp, true, false))
                                error(ERR_CONSTANT_FUNCTION_EXPECTED);
                        }
                        else
                        {
                            *exp = intNode(en_c_i, 0);
                            *tp = &stdint;
                        }
                        return lex;
                    }
                    else
                    {
                        needkw(&lex, openpa);
                        *exp = intNode(en_c_i, 0);
                        *tp = &stdint;
                        return lex;
                    }
                }
            }
            if (cparams.prm_cplusplus)
            {
                STRUCTSYM l;
                l.str = basetype(*tp)->sp;
                addStructureDeclaration(&l);
                lex = id_expression(lex, funcsp, &sp2, nullptr, nullptr, &isTemplate, false, true, nullptr);
                dropStructureDeclaration();
            }
            else
            {
                sp2 = search(lex->value.s.a, (basetype(*tp)->sp)->tp->syms);
            }
            if (!sp2)
            {
                errorNotMember(basetype(*tp)->sp, nullptr, lex->value.s.a);
                lex = getsym();
                while (ISID(lex))
                {
                    lex = getsym();
                    if (!MATCHKW(lex, pointsto) && !MATCHKW(lex, dot))
                        break;
                    lex = getsym();
                }
            }
            else
            {
                TYPE* tpb;
                TYPE* basetp = *tp;
                TYPE* typ2 = typein;
                if (sp2->attribs.uninheritable.deprecationText)
                    deprecateMessage(sp2);
                browse_usage(sp2, lex->filenum);
                if (ispointer(typ2))
                    typ2 = basetype(typ2)->btp;
                (*exp)->isatomic = false;
                lex = getsym();
                sp2->attribs.inheritable.used = true;
                *tp = sp2->tp;
                tpb = basetype(*tp);
                if (sp2->storage_class == sc_overloads)
                {
                    FUNCTIONCALL* funcparams = (FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
                    if (cparams.prm_cplusplus && MATCHKW(lex, lt))
                    {
                        SYMLIST* hr1 = basetype(sp2->tp)->syms->table[0];
                        bool isdest = (hr1->p)->isDestructor;
                        while (hr1)
                        {
                            if ((hr1->p)->templateLevel)
                                break;
                            hr1 = hr1->next;
                        }
                        if (hr1)
                        {
                            lex = GetTemplateArguments(lex, funcsp, nullptr, &funcparams->templateParams);
                            funcparams->astemplate = true;
                        }
                        else if (isdest && sp2->parentClass->templateLevel)
                        {
                            TEMPLATEPARAMLIST* lst = nullptr;
                            lex = GetTemplateArguments(lex, funcsp, nullptr, &lst);
                            if (!exactMatchOnTemplateArgs(lst, sp2->parentClass->templateParams->p->bySpecialization.types))
                                error(ERR_DESTRUCTOR_MUST_MATCH_CLASS);
                        }
                        else if (isTemplate)
                        {
                            errorsym(ERR_NOT_A_TEMPLATE, sp2);
                        }
                    }
                    else if (isTemplate)
                    {
                        errorsym(ERR_NOT_A_TEMPLATE, sp2);
                    }
                    funcparams->sp = sp2;
                    funcparams->thisptr = *exp;
                    funcparams->thistp = (TYPE*)Alloc(sizeof(TYPE));
                    funcparams->thistp->size = getSize(bt_pointer);
                    funcparams->thistp->type = bt_pointer;
                    funcparams->thistp->btp = basetp;
                    funcparams->thistp->rootType = funcparams->thistp;

                    if (!points && (*exp)->type != en_l_ref)
                        funcparams->novtab = true;
                    *exp = varNode(en_func, nullptr);
                    (*exp)->v.func = funcparams;
                }
                else
                {
                    SYMBOL* sp3 = sp2->parentClass;
                    if (ismutable && sp2->storage_class == sc_mutable)
                        *ismutable = true;
                    if (sp2->templateLevel)
                    {
                        if (!sp2->instantiated)
                        {
                            TEMPLATEPARAMLIST* lst = nullptr;
                            SYMBOL* sp1 = sp2;
                            if (MATCHKW(lex, lt))
                            {
                                lex = GetTemplateArguments(lex, funcsp, sp2, &lst);
                                if (sp1)
                                {
                                    sp1 = GetClassTemplate(sp1, lst, false);
                                    if (sp1)
                                        sp2 = sp1;
                                    else
                                        errorsym(ERR_NO_TEMPLATE_MATCHES, sp2);
                                }
                            }
                            else if (sp2)
                            {
                                sp1 = sp2;
                            }
                        }
                    }
                    else if (isTemplate)
                    {
                        errorsym(ERR_NOT_A_TEMPLATE, sp2);
                    }
                    if (sp2->storage_class == sc_external || sp2->storage_class == sc_static)
                    {
                        SYMBOL* tpl = sp2;
                        while (tpl)
                        {
                            if (tpl->templateLevel)
                                break;
                            tpl = tpl->parentClass;
                        }
                        if (tpl && tpl->instantiated)
                        {
                            TemplateDataInstantiate(sp2, false, false);
                        }
                    }
                    /*
                    if (sp3 && sp3->mainsym)
                        sp3 = sp3->mainsym;
                    if (sp4 && sp4->mainsym)
                        sp4 = sp4->mainsym;
                        











                    if (sp3 && sp4 && sp3 != sp4 && classRefCount(sp3, sp4) != 1)
                    {
                        errorsym2(ERR_NOT_UNAMBIGUOUS_BASE, sp3, sp4);
                    }
                    */
                    {
                        enum e_ac access = ac_public;
                        SYMBOL* ssp = getStructureDeclaration();
                        if (ssp == basetype(typ2)->sp)
                            access = ac_protected;
                        if (!isAccessible(basetype(typ2)->sp, basetype(typ2)->sp, sp2, funcsp, access, false))
                        {
                            errorsym(ERR_CANNOT_ACCESS, sp2);
                        }
                    }
                    if (sp2->storage_class == sc_constant)
                    {
                        *exp = varNode(en_const, sp2);
                    }
                    else if (sp2->storage_class == sc_enumconstant)
                    {
                        *exp = intNode(en_c_i, sp2->value.i);
                    }
                    else if (sp2->storage_class == sc_static || sp2->storage_class == sc_external)
                    {
                        if (chosenAssembler->msil)
                        {
                            *exp = varNode(en_global, sp2);
                        }
                        else
                        {
                            EXPRESSION* exp2 = varNode(en_global, sp2);
                            *exp = exprNode(en_void, *exp, exp2);
                        }
                    }
                    else
                    {
                        EXPRESSION* offset;
                        if (chosenAssembler->msil && !isarray(sp2->tp))
                            offset = varNode(en_structelem, sp2);  // prepare for the MSIL ldflda instruction
                        else
                            offset = intNode(en_c_i, sp2->offset);
                        if (!typein2 && sp2->parentClass != basetype(typ2)->sp)
                        {
                            *exp = baseClassOffset(sp2->parentClass, basetype(typ2)->sp, *exp);
                        }
                        *exp = exprNode(en_structadd, *exp, offset);
                        if (typein2)
                            deref(typein2, exp);
                        if (sp3)
                        {
                            do
                            {
                                if (basetype(sp3->tp)->type == bt_union)
                                {
                                    offset->unionoffset = true;
                                    break;
                                }
                                if (sp3 != sp2 && ispointer(sp3->tp))
                                    break;
                                sp3 = sp3->parent;
                            } while (sp3);
                        }
                    }
                    if (tpb->hasbits)
                    {
                        (*exp) = exprNode(en_bits, *exp, 0);
                        (*exp)->bits = tpb->bits;
                        (*exp)->startbit = tpb->startbit;
                    }
                    if (sp2->storage_class != sc_constant && sp2->storage_class != sc_enumconstant)
                    {
                        if (isref(*tp))
                        {
                            if (!isstructured(basetype(*tp)->btp))
                                deref(*tp, exp);
                        }
                        deref(*tp, exp);
                    }
                    if (sp2->storage_class != sc_enumconstant)
                    {
                        (*exp)->v.sp = sp2;  // caching the member symbol in the enode for constexpr handling
                    }
                    if (isatomic(basetp))
                    {
                        // note this won't work in C++ because of offset2...
                        (*exp)->isatomic = true;
                        if (needsAtomicLockFromType(*tp))
                            (*exp)->lockOffset = basetp->size - ATOMIC_FLAG_SPACE - sp2->offset;
                    }
                }
                if (!points && ((*exp)->type == en_not_lvalue || (*exp)->type == en_func || (*exp)->type == en_void))
                    if (ISKW(lex) && lex->kw->key >= assign && lex->kw->key <= asxor)
                        error(ERR_LVALUE);
            }
        }
        /* members inherit qualifiers from their parent
         * this mechanism will automatically recurse
         * we don't encode this in the actual type so we don't have to dup
         * types all over the place
         */
        if (points && ispointer(typein))
            typein = basetype(typein)->btp;

        if (isconst(typein) && !isconst(*tp))
        {
            TYPE* p = (TYPE*)Alloc(sizeof(TYPE));
            p->type = bt_const;
            p->btp = *tp;
            p->rootType = (*tp)->rootType;
            p->size = p->btp->size;
            (*tp) = p;
        }
        if (isvolatile(typein) && !isvolatile(*tp))
        {
            TYPE* p = (TYPE*)Alloc(sizeof(TYPE));
            p->type = bt_volatile;
            p->btp = *tp;
            p->rootType = (*tp)->rootType;
            p->size = p->btp->size;
            (*tp) = p;
        }
    }
    return lex;
}
static void LookupSingleAggregate(TYPE* tp, EXPRESSION** exp)
{
    if (tp->type == bt_aggregate)
    {
        SYMLIST* hr = tp->syms->table[0];
        *exp = varNode(en_pc, hr->p);
        if (hr->next)
            errorsym(ERR_OVERLOADED_FUNCTION_AMBIGUOUS, tp->sp);
        //        else // commented out because of std::endl
        //            error(ERR_FUNCTION_CALL_NEEDS_ARGUMENT_LIST);
    }
}
static LEXEME* expression_bracket(LEXEME* lex, SYMBOL* funcsp, TYPE** tp, EXPRESSION** exp, int flags)
{
    TYPE* tp2 = nullptr;
    EXPRESSION* expr2 = nullptr;
    lex = getsym();
    if (cparams.prm_cplusplus && MATCHKW(lex, begin))
    {
        INITLIST* args = nullptr;
        lex = getInitList(lex, funcsp, &args);
        if ((cparams.prm_cplusplus || chosenAssembler->msil) &&
            insertOperatorFunc(ovcl_openbr, openbr, funcsp, tp, exp, nullptr, nullptr, args, flags))
        {
        }
        else
        {
            error(ERR_CANNOT_USE_INIT_LIST_WITH_BUILTIN_BRACKET);
        }
    }
    else
    {
        lex = expression_comma(lex, funcsp, nullptr, &tp2, &expr2, nullptr, flags);
        if (tp2)
        {
            LookupSingleAggregate(tp2, &expr2);
            if ((cparams.prm_cplusplus || chosenAssembler->msil) &&
                insertOperatorFunc(ovcl_openbr, openbr, funcsp, tp, exp, tp2, expr2, nullptr, flags))
            {
            }
            else if (isvoid(*tp) || isvoid(tp2) || (*tp)->type == bt_aggregate || ismsil(*tp) || ismsil(tp2))
                error(ERR_NOT_AN_ALLOWED_TYPE);
            else if (basetype(tp2)->type == bt_memberptr || basetype(*tp)->type == bt_memberptr)
                error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
            else if (basetype(tp2)->scoped || basetype(*tp)->scoped)
                error(ERR_SCOPED_TYPE_MISMATCH);
            else if (isarray(*tp) && (*tp)->msil)
            {
                if (!isint(tp2) && basetype(tp2)->type != bt_enum)
                {
                    if (ispointer(tp2))
                        error(ERR_NONPORTABLE_POINTER_CONVERSION);
                    else
                        error(ERR_NEED_INTEGER_EXPRESSION);
                }
                if ((*exp)->type != en_msil_array_access)
                {
                    EXPRESSION* expr3 = exprNode(en_msil_array_access, nullptr, nullptr);
                    TYPE* btp = *tp;
                    int count = 0;
                    while (isarray(btp) && btp->msil)
                    {
                        count++;
                        btp = btp->btp;
                    }
                    expr3->v.msilArray = (MSIL_ARRAY*)Alloc(sizeof(MSIL_ARRAY) + count - 1 * sizeof(EXPRESSION*));
                    expr3->v.msilArray->max = count;
                    expr3->v.msilArray->count = 0;
                    expr3->v.msilArray->base = (*exp);
                    expr3->v.msilArray->tp = *tp;
                    (*exp) = expr3;
                }
                (*exp)->v.msilArray->indices[(*exp)->v.msilArray->count++] = expr2;
                *tp = basetype(*tp)->btp;
                if (!MATCHKW(lex, closebr))
                {
                    error(ERR_ARRAY_NEED_CLOSEBRACKET);
                    errskim(&lex, skim_closebr);
                }
                skip(&lex, closebr);
                if ((*exp)->v.msilArray->count < (*exp)->v.msilArray->max && !MATCHKW(lex, openbr))
                {
                    error(ERR_ASSIGN_ONLY_MSIL_ARRAY_ELEMENTS);
                }
                *tp = PerformDeferredInitialization(*tp, funcsp);
                return lex;
            }
            else if (ispointer(*tp))
            {
                if (!isint(tp2) && basetype(tp2)->type != bt_enum)
                {
                    if (ispointer(tp2))
                        error(ERR_NONPORTABLE_POINTER_CONVERSION);
                    else
                        error(ERR_NEED_INTEGER_EXPRESSION);
                }
                /* unreference pointer to VLA */
                if (!(*tp)->vla && basetype(basetype(*tp)->btp)->vla)
                    (*tp) = basetype(*tp)->btp;
                if ((*tp)->vla)
                {
                    int soa = getSize(bt_pointer);
                    int sou = getSize(bt_unsigned);
                    EXPRESSION* vlanode = varNode(en_auto, (*tp)->sp);
                    EXPRESSION* exp1 = exprNode(en_add, vlanode, intNode(en_c_i, soa + sou * (1 + (*tp)->vlaindex)));
                    deref(&stdint, &exp1);
                    cast(&stdpointer, &expr2);
                    exp1 = exprNode(en_umul, exp1, expr2);
                    *exp = exprNode(en_add, *exp, exp1);
                    *tp = basetype(*tp)->btp;
                }
                else
                {
                    EXPRESSION* exp1 = nullptr;
                    *tp = basetype(*tp)->btp;
                    if (isstructured(*tp))
                        *tp = PerformDeferredInitialization(*tp, funcsp);
                    cast(&stdint, &expr2);
                    exp1 = exprNode(en_umul, expr2, intNode(en_c_i, (*tp)->size));
                    *exp = exprNode(en_add, *exp, exp1);
                }
                if (!(*tp)->array && !(*tp)->vla)
                    deref(*tp, exp);
            }
            else if (ispointer(tp2))
            {
                if (!isint(*tp))
                {
                    if (ispointer(*tp))
                        error(ERR_NONPORTABLE_POINTER_CONVERSION);
                    else
                        error(ERR_NEED_INTEGER_EXPRESSION);
                }
                if (tp2->vla)
                {
                    int soa = getSize(bt_pointer);
                    int sou = getSize(bt_unsigned);
                    EXPRESSION* vlanode = varNode(en_auto, tp2->sp);
                    expr2 = exprNode(en_add, expr2, intNode(en_c_i, soa + sou * (1 + tp2->vlaindex)));
                    deref(&stdint, exp);
                    *exp = exprNode(en_umul, expr2, *exp);
                    deref(&stdpointer, &vlanode);
                    *exp = exprNode(en_add, *exp, vlanode);
                    *tp = basetype(tp2)->btp;
                }
                else
                {
                    EXPRESSION* exp1 = nullptr;
                    *tp = basetype(tp2)->btp;
                    cast(&stdint, exp);
                    exp1 = exprNode(en_umul, *exp, intNode(en_c_i, (*tp)->size));
                    *exp = exprNode(en_add, expr2, exp1);
                }
                if (!(*tp)->array && !(*tp)->vla)
                    deref(*tp, exp);
            }
            else
            {
                error(ERR_DEREF);
            }
        }
        else
            error(ERR_EXPRESSION_SYNTAX);
    }
    if (!MATCHKW(lex, closebr))
    {
        error(ERR_ARRAY_NEED_CLOSEBRACKET);
        errskim(&lex, skim_closebr);
    }
    skip(&lex, closebr);
    *tp = PerformDeferredInitialization(*tp, funcsp);
    return lex;
}
void checkArgs(FUNCTIONCALL* params, SYMBOL* funcsp)
{
    SYMLIST* hr = basetype(params->functp)->syms->table[0];
    INITLIST* list = params->arguments;
    bool matching = true;
    bool tooshort = false;
    bool toolong = false;
    bool noproto = false;  // params->sp ? params->sp->oldstyle : false;
    bool vararg = false;
    bool hasEllipse = false;
    int argnum = 0;

    if (hr && hr->p->thisPtr)
        hr = hr->next;
    if (!hr)
    {
        matching = false;
        noproto = params->sp->oldstyle;
    }
    if (noproto)
    {
        if (params->sp->hasproto)
            noproto = false;
    }
    if (hr && hr->p->tp->type == bt_void)
    {
        if (list)
            errorsym(ERR_FUNCTION_TAKES_NO_PARAMETERS, params->sp);
    }
    else
        while (hr || list)
        {
            TYPE* dest = nullptr;
            SYMBOL* decl = nullptr;
            if (!hr || hr->p->tp->type != bt_any)
            {
                argnum++;
                if (matching && hr)
                {
                    decl = hr->p;
                    if (!decl->tp)
                        noproto = true;
                    else if (basetype(decl->tp)->type == bt_ellipse)
                    {
                        hasEllipse = true;
                        vararg = chosenAssembler->msil && chosenAssembler->msil->managed(params->sp);
                        params->vararg = vararg;
                        matching = false;
                        decl = nullptr;
                        hr = nullptr;
                        if (!list)
                            break;
                    }
                }
                if (!decl && !hasEllipse && chosenAssembler->msil)
                    toolong = true;
                if (matching)
                {
                    if (!decl)
                        toolong = true;
                    else if (!list)
                        tooshort = true;
                    else
                    {
                        if (decl->attribs.inheritable.zstring)
                        {
                            EXPRESSION* exp = list->exp;
                            if (!exp)
                            {
                                exp = list->exp = intNode(en_c_i, 0);
                            }
                            if (lvalue(exp))
                                exp = exp->left;
                            switch (exp->type)
                            {
                                case en_global:
                                case en_auto:
                                case en_absolute:
                                case en_pc:
                                case en_threadlocal:
                                    if (exp->v.sp->attribs.inheritable.nonstring)
                                    {
                                        error(ERR_NULL_TERMINATED_STRING_REQUIRED);
                                    }
                                    break;
                            }
                        }
                        if (isref(decl->tp))
                        {
                            TYPE* tpb = basetype(basetype(decl->tp)->btp);
                            TYPE* tpd = list->tp;
                            list->byRef = true;
                            if (isref(tpd))
                                tpd = basetype(tpd)->btp;
                            tpd = basetype(tpd);
                            if (isstructured(tpb) && isstructured(tpd))
                            {
                                SYMBOL* base = basetype(tpb)->sp;
                                SYMBOL* derived = basetype(tpd)->sp;
                                if (base != derived && classRefCount(base, derived) != 1)
                                    errortype(ERR_CANNOT_CONVERT_TYPE, list->tp, decl->tp);
                                if (base != derived && !isAccessible(derived, derived, base, funcsp, ac_public, false))
                                    errorsym(ERR_CANNOT_ACCESS, base);
                            }
                            else
                            {
                                goto join;
                            }
                        }
                        else
                        {
                        join:
                            if (!list || !list->tp)
                            {
                            }
                            else if (basetype(decl->tp)->type == bt___string)
                            {
                                if (list->exp->type == en_labcon)
                                    list->exp->type = en_c_string;
                            }
                            else if (!comparetypes(list->tp, decl->tp, false))
                            {
                                if (!chosenAssembler->msil || !isstructured(decl->tp))
                                {
                                    if (basetype(decl->tp)->type == bt___object)
                                    {
                                        if (!isstructured(list->tp) && (!isarray(list->tp) || !basetype(list->tp)->msil))
                                        {
                                            list->exp = exprNode(en_x_object, list->exp, nullptr);
                                        }
                                    }
                                    else if (basetype(decl->tp)->type != bt_memberptr)
                                        errorarg(ERR_TYPE_MISMATCH_IN_ARGUMENT, argnum, decl, params->sp);
                                }
                                else if (isstructured(list->tp))
                                {
                                    SYMBOL* base = basetype(decl->tp)->sp;
                                    SYMBOL* derived = basetype(list->tp)->sp;
                                    if (base != derived && classRefCount(base, derived) != 1)
                                        errortype(ERR_CANNOT_CONVERT_TYPE, list->tp, decl->tp);
                                    if (base != derived && !isAccessible(derived, derived, base, funcsp, ac_public, false))
                                        errorsym(ERR_CANNOT_ACCESS, base);
                                }
                                else if (!isconstzero(list->tp, list->exp))
                                    errorarg(ERR_TYPE_MISMATCH_IN_ARGUMENT, argnum, decl, params->sp);
                            }
                            else if (assignDiscardsConst(decl->tp, list->tp))
                            {
                                if (cparams.prm_cplusplus)
                                    errortype(ERR_CANNOT_CONVERT_TYPE, list->tp, decl->tp);
                                else
                                    error(ERR_SUSPICIOUS_POINTER_CONVERSION);
                            }
                            else if (ispointer(decl->tp))
                            {
                                if (!ispointer(list->tp) && (!isarithmeticconst(list->exp) || !isconstzero(decl->tp, list->exp)))
                                {
                                    if (!isfunction(list->tp) ||
                                        (!isvoidptr(decl->tp) &&
                                         (!isfuncptr(decl->tp) ||
                                          (cparams.prm_cplusplus && !comparetypes(basetype(decl->tp)->btp, list->tp, true)))))
                                        errorarg(ERR_TYPE_MISMATCH_IN_ARGUMENT, argnum, decl, params->sp);
                                }
                                else if (!comparetypes(decl->tp, list->tp, true))
                                {
                                    if (!isconstzero(list->tp, list->exp))
                                    {
                                        if (!isvoidptr(decl->tp) && !isvoidptr(list->tp))
                                        {
                                            if (cparams.prm_cplusplus)
                                            {
                                                TYPE* tpb = basetype(decl->tp)->btp;
                                                TYPE* tpd = basetype(list->tp)->btp;
                                                if (isstructured(tpb) && isstructured(tpd))
                                                {
                                                    SYMBOL* base = basetype(tpb)->sp;
                                                    SYMBOL* derived = basetype(tpd)->sp;
                                                    if (base != derived && classRefCount(base, derived) != 1)
                                                        errortype(ERR_CANNOT_CONVERT_TYPE, list->tp, decl->tp);
                                                    if (isAccessible(derived, derived, base, funcsp, ac_public, false))
                                                        errorsym(ERR_CANNOT_ACCESS, base);
                                                }
                                                else
                                                {
                                                    errortype(ERR_CANNOT_CONVERT_TYPE, list->tp, decl->tp);
                                                }
                                            }
                                            else
                                            {
                                                if (!matchingCharTypes(list->tp, decl->tp))
                                                    error(ERR_SUSPICIOUS_POINTER_CONVERSION);
                                            }
                                        }
                                    }
                                }
                            }
                            else if (ispointer(list->tp))
                            {
                                if (!ispointer(decl->tp))
                                    errorarg(ERR_TYPE_MISMATCH_IN_ARGUMENT, argnum, decl, params->sp);
                            }
                            else if (isarithmetic(list->tp) && isarithmetic(decl->tp))
                            {
                                if (basetype(decl->tp)->type != basetype(list->tp)->type && basetype(decl->tp)->type > bt_int)
                                {
                                    cast(decl->tp, &list->exp);
                                }
                            }
                        }
                        dest = decl->tp;
                    }
                }
                else if (list->tp->vla)
                {
                    // cast to a regular pointer if there is no declared param
                    TYPE* tpx = (TYPE*)(TYPE*)Alloc(sizeof(TYPE));
                    tpx->type = bt_pointer;
                    tpx->size = getSize(bt_pointer);
                    tpx->btp = list->tp;
                    while (tpx->btp->vla)
                        tpx->btp = tpx->btp->btp;
                    list->tp = tpx;
                }
                else if (isint(list->tp))
                {
                    if (basetype(list->tp)->type <= bt_int)
                        dest = &stdint;
                    else if (!(chosenAssembler->msil))
                        cast(list->tp, &list->exp);
                }
                else if (isfloat(list->tp))
                {
                    if (basetype(list->tp)->type < bt_double)
                        dest = &stddouble;
                    else if (!(chosenAssembler->msil))
                        cast(list->tp, &list->exp);
                }
                if (dest && list && list->tp && basetype(dest)->type != bt_memberptr && !comparetypes(dest, list->tp, true))
                {
                    if (!chosenAssembler->msil || (!isstructured(dest) && (!isarray(dest) || !basetype(dest)->msil)))
                        cast(basetype(dest), &list->exp);
                    list->tp = dest;
                }
                else if (dest && list && basetype(dest)->type == bt_enum)
                {
                    // this needs to be revisited to get proper typing in C++
                    cast(&stdint, &list->exp);
                    list->tp = &stdint;
                }
            }
            if (list)
            {
                bool ivararg = vararg;
                if (list->exp->type == en_auto && list->exp->v.sp->va_typeof)
                    ivararg = false;
                list->vararg = ivararg;
                if (hr)
                {
                    TYPE* tp = hr->p->tp;
                    while (tp && tp->type != bt_pointer)
                    {
                        if (tp->type == bt_va_list)
                        {
                            list->valist = true;
                            break;
                        }
                        tp = tp->btp;
                    }
                }
                list = list->next;
            }
            if (hr)
                hr = hr->next;
        }
    if (noproto)
        errorsym(ERR_CALL_FUNCTION_NO_PROTO, params->sp);
    else if (toolong)
        errorsym(ERR_PARAMETER_LIST_TOO_LONG, params->sp);
    else if (tooshort)
        errorsym(ERR_PARAMETER_LIST_TOO_SHORT, params->sp);
}
static LEXEME* getInitInternal(LEXEME* lex, SYMBOL* funcsp, INITLIST** lptr, enum e_kw finish, bool allowNesting, bool allowPack,
                               bool toErr, int flags)
{
    *lptr = nullptr;
    lex = getsym(); /* past ( */
    while (!MATCHKW(lex, finish))
    {
        INITLIST* p = (INITLIST*)Alloc(sizeof(INITLIST));
        if (MATCHKW(lex, begin))
        {
            lex = getInitInternal(lex, funcsp, &p->nested, end, true, false, false, flags);
            *lptr = p;
            lptr = &(*lptr)->next;
            if (!allowNesting)
                error(ERR_CANNOT_USE_INIT_LIST);
        }
        else
        {
            LEXEME* start = lex;
            lex = expression_assign(lex, funcsp, nullptr, &p->tp, &p->exp, nullptr,
                                    _F_PACKABLE | (finish == closepa ? _F_INARGS : 0) | (flags & _F_SIZEOF));
            if (p->tp && isvoid(p->tp) && finish != closepa)
                error(ERR_NOT_AN_ALLOWED_TYPE);
            optimize_for_constants(&p->exp);
            if (finish != closepa)
                assignmentUsages(p->exp, false);
            if (p->tp)
            {
                if (p->exp && p->exp->type == en_func && p->exp->v.func->sp->parentClass && !p->exp->v.func->ascall &&
                    !p->exp->v.func->asaddress)
                {
                    SYMLIST* hr = basetype(p->exp->v.func->functp)->syms->table[0];
                    while (hr)
                    {
                        SYMBOL* sym = hr->p;
                        if (sym->storage_class == sc_member || sym->storage_class == sc_mutable)
                        {
                            error(ERR_NO_IMPLICIT_MEMBER_FUNCTION_ADDRESS);
                            break;
                        }
                        hr = hr->next;
                    }
                }
                if (allowPack && cparams.prm_cplusplus && MATCHKW(lex, ellipse))
                {
                    // lose p
                    lex = getsym();
                    if (templateNestingCount)
                    {
                        *lptr = p;
                        lptr = &(*lptr)->next;
                    }
                    else if (p->exp && p->exp->type != en_packedempty)
                    {
                        if (!isstructured(p->tp) && !p->tp->templateParam)
                            checkPackedExpression(p->exp);
                        // this is going to presume that the expression involved
                        // is not too long to be cached by the LEXEME mechanism.
                        lptr = expandPackedInitList(lptr, funcsp, start, p->exp);
                    }
                }
                else
                {
                    if (toErr && argument_nesting <= 1)
                        checkUnpackedExpression(p->exp);
                    *lptr = p;
                    lptr = &(*lptr)->next;
                }
            }
            else
            {
                lex = backupsym();
                error(ERR_IDENTIFIER_EXPECTED);
                errskim(&lex, finish == closepa ? skim_closepa : skim_end);
                return lex;
            }
        }
        if (!MATCHKW(lex, comma))
        {
            break;
        }
        lex = getsym();
        if (MATCHKW(lex, finish))
            break;
    }
    if (!needkw(&lex, finish))
    {
        errskim(&lex, finish == closepa ? skim_closepa : skim_end);
        skip(&lex, finish);
    }
    return lex;
}
LEXEME* getInitList(LEXEME* lex, SYMBOL* funcsp, INITLIST** owner)
{
    return getInitInternal(lex, funcsp, owner, end, false, true, true, 0);
}
LEXEME* getArgs(LEXEME* lex, SYMBOL* funcsp, FUNCTIONCALL* funcparams, enum e_kw finish, bool allowPack, int flags)
{
    LEXEME* rv;
    argument_nesting++;
    rv = getInitInternal(lex, funcsp, &funcparams->arguments, finish, true, allowPack, argument_nesting == 1, flags);
    argument_nesting--;
    return rv;
}
LEXEME* getMemberInitializers(LEXEME* lex, SYMBOL* funcsp, FUNCTIONCALL* funcparams, enum e_kw finish, bool allowPack)
{
    LEXEME* rv;
    argument_nesting++;
    rv = getInitInternal(lex, funcsp, &funcparams->arguments, finish, true, allowPack, false, 0);
    argument_nesting--;
    return rv;
}
static int simpleDerivation(EXPRESSION* exp)
{
    int rv = 0;
    while (castvalue(exp))
        exp = exp->left;
    if (exp->left)
        rv |= simpleDerivation(exp->left);
    if (exp->right)
        rv |= simpleDerivation(exp->right);
    switch (exp->type)
    {
        case en_func:
        case en_thisref:
        case en_stmt:
            rv |= 1;
            break;
        case en_labcon:
        case en_global:
        case en_auto:
        case en_absolute:
        case en_pc:
        case en_threadlocal:
            rv |= 2;
            break;
        default:
            if (lvalue(exp))
                rv |= 1;
            break;
    }
    return rv;
}
EXPRESSION* DerivedToBase(TYPE* tpn, TYPE* tpo, EXPRESSION* exp, int flags)
{
    if (isref(tpn))
        tpn = basetype(tpn)->btp;
    if (isref(tpo))
        tpo = basetype(tpo)->btp;
    if (isstructured(tpn) && isstructured(tpo))
    {
        SYMBOL* spn = basetype(tpn)->sp;
        SYMBOL* spo = basetype(tpo)->sp;
        if (spo != spn && !sameTemplate(tpn, tpo))
        {
            int n = classRefCount(spn, spo);
            if (n == 1)
            {
                // derived to base
                EXPRESSION q, *v = &q;
                memset(&q, 0, sizeof(q));
                v->type = en_c_i;
                v = baseClassOffset(spn, spo, v);

                optimize_for_constants(&v);
                if (v->type != en_c_i && (flags & _F_NOVIRTUALBASE))
                    return nullptr;

                v = baseClassOffset(spn, spo, exp);
                if ((flags & _F_VALIDPOINTER) || v->type != en_c_i || simpleDerivation(exp) == 2)
                {
                    optimize_for_constants(&v);
                }
                else
                {
                    EXPRESSION* varsp = anonymousVar(sc_auto, &stdpointer);
                    EXPRESSION* var = exprNode(en_l_p, varsp, nullptr);
                    EXPRESSION* asn = exprNode(en_assign, var, exp);
                    EXPRESSION* left = exprNode(en_add, var, v);
                    EXPRESSION* right = var;
                    v = exprNode(en_cond, var, exprNode(en_void, left, right));
                    v = exprNode(en_void, asn, v);
                }
                return v;
            }
        }
    }
    return exp;
}
static bool cloneTempExpr(EXPRESSION** expr, SYMBOL** found, SYMBOL** replace);
bool cloneTempStmt(STATEMENT** block, SYMBOL** found, SYMBOL** replace)
{
    bool rv = false;
    while (*block != nullptr)
    {
        if (found)
        {
            STATEMENT* b = (STATEMENT*)Alloc(sizeof(STATEMENT));
            *b = **block;
            *block = b;
        }
        switch ((*block)->type)
        {
            case st__genword:
                break;
            case st_try:
            case st_catch:
            case st___try:
            case st___catch:
            case st___finally:
            case st___fault:
                rv |= cloneTempStmt(&(*block)->lower, found, replace);
                rv |= cloneTempStmt(&(*block)->blockTail, found, replace);
                break;
            case st_return:
            case st_expr:
            case st_declare:
                rv |= cloneTempExpr(&(*block)->select, found, replace);
                break;
            case st_goto:
            case st_label:
                break;
            case st_select:
            case st_notselect:
                rv |= cloneTempExpr(&(*block)->select, found, replace);
                break;
            case st_switch:
                rv |= cloneTempExpr(&(*block)->select, found, replace);
                rv |= cloneTempStmt(&(*block)->lower, found, replace);
                break;
            case st_block:
                rv |= cloneTempStmt(&(*block)->lower, found, replace);
                rv |= cloneTempStmt(&(*block)->blockTail, found, replace);
                break;
            case st_passthrough:
                break;
            case st_datapassthrough:
                break;
            case st_nop:
                break;
            case st_line:
            case st_varstart:
            case st_dbgblock:
                break;
            default:
                diag("Invalid block type in cloneTempStmt");
                break;
        }
        block = &(*block)->next;
    }
    return rv;
}
#define CLONED_SYMS 20
static bool cloneTempExpr(EXPRESSION** expr, SYMBOL** found, SYMBOL** replace)
{
    bool rv = false;
    if (!*expr)
        return rv;
    if (found)
    {
        EXPRESSION* newval = (EXPRESSION*)Alloc(sizeof(EXPRESSION));
        *newval = **expr;
        *expr = newval;
    }
    if ((*expr)->type == en_thisref)
    {
        rv = cloneTempExpr(&(*expr)->left, found, replace);
    }
    else if ((*expr)->type == en_stmt)
    {
        rv = cloneTempStmt(&(*expr)->v.stmt, found, replace);
    }
    else if ((*expr)->type == en_func)
    {
        if ((*expr)->v.func->sp->isConstructor || (*expr)->v.func->sp->isDestructor)
        {
            INITLIST** args;
            if (found)
            {
                FUNCTIONCALL* newval = (FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
                *newval = *(*expr)->v.func;
                (*expr)->v.func = newval;
            }
            args = &(*expr)->v.func->arguments;
            while (*args)
            {
                if (found)
                {
                    INITLIST* newval = (INITLIST*)Alloc(sizeof(INITLIST));
                    *newval = **args;
                    *args = newval;
                }
                rv |= cloneTempExpr(&(*args)->exp, found, replace);
                args = &(*args)->next;
            }
            rv |= cloneTempExpr(&(*expr)->v.func->thisptr, found, replace);
        }
    }
    else
    {
        rv |= cloneTempExpr(&(*expr)->left, found, replace);
        rv |= cloneTempExpr(&(*expr)->right, found, replace);
        if ((*expr)->type == en_auto)
        {
            SYMBOL* sym = (*expr)->v.sp;
            if (sym->anonymous && !sym->stackblock)
            {
                if (found)
                {
                    int i = 0;
                    EXPRESSION* newexp = (EXPRESSION*)Alloc(sizeof(EXPRESSION));
                    *newexp = **expr;
                    *expr = newexp;
                    for (i = 0; i < CLONED_SYMS && found[i]; i++)
                    {
                        if (found[i] == sym)
                            break;
                    }
                    if (i >= CLONED_SYMS)
                    {
                        diag("cloneTempExpr - too many syms");
                    }
                    else
                    {
                        if (!found[i])
                        {
                            found[i] = sym;
                            replace[i] = clonesym(sym);
                            if (theCurrentFunc)
                                replace[i]->value.i = theCurrentFunc->value.i;
                            replace[i]->name = AnonymousName();
                        }
                        (*expr)->v.sp = replace[i];
                    }
                }
                rv = true;
            }
        }
    }
    return rv;
}
void CreateInitializerList(TYPE* initializerListTemplate, TYPE* initializerListType, INITLIST** lptr, bool operands, bool asref)
{
    (void)operands;
    INITLIST** initial = lptr;
    EXPRESSION *rv = nullptr, **pos = &rv;
    int count = 0, i;
    INITLIST* searchx = *lptr;
    TYPE* tp = (TYPE*)Alloc(sizeof(TYPE));
    EXPRESSION *data, *initList;
    SYMBOL *start, *end;
    EXPRESSION* dest;
    start = search("_M_start", basetype(initializerListTemplate)->syms);
    end = search("_M_end", basetype(initializerListTemplate)->syms);
    if (!start || !end)
        Utils::fatal("Invalid definition of initializer-list");
    if (!(*initial)->nested && comparetypes((*initial)->tp, initializerListTemplate, true))
    {
        *initial = (INITLIST*)Alloc(sizeof(INITLIST));
        if (asref)
        {
            (*initial)->tp = (TYPE*)Alloc(sizeof(TYPE));
            (*initial)->tp->size = getSize(bt_pointer);
            (*initial)->tp->type = bt_lref;
            (*initial)->tp->btp = initializerListTemplate;
            (*initial)->tp->rootType = (*initial)->tp;
            (*initial)->exp = searchx->exp;
        }
        else
        {
            (*initial)->tp = initializerListTemplate;
            (*initial)->exp = exprNode(en_stackblock, searchx->exp, nullptr);
            (*initial)->exp->size = basetype(initializerListTemplate)->size;
        }
    }
    else
    {
        tp->type = bt_pointer;
        tp->array = true;
        while (searchx)
            count++, searchx = searchx->next;
        tp->btp = initializerListType;
        tp->rootType = tp;
        tp->size = count * (initializerListType->size);
        tp->esize = intNode(en_c_i, count);
        data = anonymousVar(sc_auto, tp);
        if (isstructured(initializerListType))
        {
            EXPRESSION* exp = data;
            EXPRESSION* elms = intNode(en_c_i, count);
            callDestructor(initializerListType->sp, nullptr, &exp, elms, true, false, false);
            initInsert(&data->v.sp->dest, tp, exp, 0, false);
        }
        for (i = 0; i < count; i++, lptr = &(*lptr)->next)
        {
            EXPRESSION* node;
            dest = exprNode(en_add, data, intNode(en_c_i, i * (initializerListType->size)));
            if (isstructured(initializerListType))
            {
                TYPE* ctype = initializerListType;
                EXPRESSION* cdest = dest;
                FUNCTIONCALL* params = (FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
                INITLIST* arg = (INITLIST*)Alloc(sizeof(INITLIST));
                params->arguments = arg;
                *arg = (*lptr)->nested ? *(*lptr)->nested : **lptr;
                if (!(*lptr)->nested)
                {
                    arg->next = nullptr;
                }

                callConstructor(&ctype, &cdest, params, false, nullptr, true, false, false, false, _F_INITLIST, false);
                node = cdest;
            }
            else
            {
                EXPRESSION* src = (*lptr)->exp;
                deref(initializerListType, &dest);
                node = exprNode(en_assign, dest, src);
            }
            if (rv)
            {
                *pos = exprNode(en_void, *pos, node);
                pos = &(*pos)->right;
            }
            else
            {
                rv = node;
            }
        }
        initList = anonymousVar(sc_auto, initializerListTemplate);
        dest = exprNode(en_add, initList, intNode(en_c_i, start->offset));
        deref(&stdpointer, &dest);
        dest = exprNode(en_assign, dest, data);
        if (rv)
        {
            *pos = exprNode(en_void, *pos, dest);
            pos = &(*pos)->right;
        }
        else
        {
            rv = dest;
        }
        dest = exprNode(en_add, initList, intNode(en_c_i, end->offset));
        deref(&stdpointer, &dest);
        dest = exprNode(en_assign, dest, exprNode(en_add, data, intNode(en_c_i, tp->size)));
        if (rv)
        {
            *pos = exprNode(en_void, *pos, dest);
            pos = &(*pos)->right;
        }
        else
        {
            rv = dest;
        }
        *initial = (INITLIST*)Alloc(sizeof(INITLIST));
        if (asref)
        {
            (*initial)->tp = (TYPE*)Alloc(sizeof(TYPE));
            (*initial)->tp->size = getSize(bt_pointer);
            (*initial)->tp->type = bt_lref;
            (*initial)->tp->btp = initializerListTemplate;
            (*initial)->tp->rootType = (*initial)->tp;
            (*initial)->exp = exprNode(en_void, rv, initList);
        }
        else
        {
            (*initial)->tp = initializerListTemplate;
            (*initial)->exp = exprNode(en_stackblock, exprNode(en_void, rv, initList), nullptr);
            (*initial)->exp->size = basetype(initializerListTemplate)->size;
        }
    }
}
void AdjustParams(SYMBOL* func, SYMLIST* hr, INITLIST** lptr, bool operands, bool implicit)
{
    (void)operands;
    if (func->storage_class == sc_overloads)
        return;
    if (hr && hr->p->thisPtr)
        hr = hr->next;
    while (hr && (*lptr || hr->p->init != nullptr ||
                  (hr->p->deferredCompile != nullptr && (!templateNestingCount || instantiatingTemplate))))
    {
        SYMBOL* sym = hr->p;
        INITLIST* p;

        if (sym->deferredCompile && !sym->init)
        {
            LEXEME* lex;
            STRUCTSYM l, n;
            TYPE* tp2;
            int count = 0;
            int tns = PushTemplateNamespace(func);
            l.str = func;
            addStructureDeclaration(&l);
            count++;

            if (func->templateParams)
            {
                n.tmpl = func->templateParams;
                addTemplateDeclaration(&n);
                count++;
            }
            sym->tp = PerformDeferredInitialization(sym->tp, nullptr);
            lex = SetAlternateLex(sym->deferredCompile);

            tp2 = sym->tp;
            if (isref(tp2))
                tp2 = basetype(tp2)->btp;
            if (isstructured(tp2))
            {
                SYMBOL* sym2;
                anonymousNotAlloc++;
                sym2 = anonymousVar(sc_auto, tp2)->v.sp;
                anonymousNotAlloc--;
                sym2->stackblock = !isref(sym->tp);
                lex = initialize(lex, theCurrentFunc, sym2, sc_auto, false, 0); /* also reserves space */
                sym->init = sym2->init;
                if (sym->init->exp->type == en_thisref)
                {
                    EXPRESSION** expr = &sym->init->exp->left->v.func->thisptr;
                    if ((*expr)->type == en_add && isconstzero(&stdint, (*expr)->right))
                        sym->init->exp->v.t.thisptr = (*expr) = (*expr)->left;
                }
            }
            else
            {
                lex = initialize(lex, theCurrentFunc, sym, sc_member, false, 0);
            }
            SetAlternateLex(nullptr);
            sym->deferredCompile = nullptr;
            while (count--)
            {
                dropStructureDeclaration();
            }
            PopTemplateNamespace(tns);
        }
        if (!*lptr)
        {
            EXPRESSION* q = sym->init->exp;
            optimize_for_constants(&q);
            *lptr = (INITLIST*)Alloc(sizeof(INITLIST));
            (*lptr)->exp = q;
            (*lptr)->tp = sym->tp;
            if (cloneTempExpr(&(*lptr)->exp, nullptr, nullptr))
            {
                int i;
                SYMBOL *ths[CLONED_SYMS], *newval[CLONED_SYMS];
                memset(ths, 0, sizeof(ths));
                memset(newval, 0, sizeof(newval));
                cloneTempExpr(&(*lptr)->exp, &ths[0], &newval[0]);
                for (i = 0; i < CLONED_SYMS && ths[i]; i++)
                    if (ths[i] && ths[i]->dest)
                    {
                        EXPRESSION* exp = ths[i]->dest->exp;
                        cloneTempExpr(&exp, &ths[0], &newval[0]);
                        (*lptr)->dest = exp;
                    }
            }
            if (isstructured(sym->tp))
            {
                hr = hr->next;
                lptr = &(*lptr)->next;
                continue;
            }
        }
        p = *lptr;
        if (p && p->exp && (p->exp->type == en_pc || p->exp->type == en_func))
        {
            if (chosenAssembler->msil)
            {
                ValidateMSILFuncPtr(func->tp, p->tp, &p->exp);
            }
            thunkForImportTable(&p->exp);
        }
        if (cparams.prm_cplusplus)
        {
            bool done = false;
            if (!done && !p->tp)
            {
                // initlist
                INITLIST* pinit = p->nested;
                if (isstructured(sym->tp) || (isref(sym->tp) && isstructured(basetype(sym->tp)->btp)))
                {
                    TYPE* stype = sym->tp;
                    SYMBOL* sp;
                    EXPRESSION* thisptr;
                    if (isref(stype))
                        stype = basetype(stype)->btp;
                    thisptr = anonymousVar(theCurrentFunc ? sc_auto : sc_localstatic, stype);
                    sp = thisptr->v.sp;
                    if (!theCurrentFunc)
                    {
                        sp->label = nextLabel++;
                        insertInitSym(sp);
                    }
                    if (stype->sp->trivialCons)
                    {
                        INITIALIZER *init = nullptr, **it = &init;
                        SYMLIST* hr = stype->syms->table[0];
                        while (pinit && hr)
                        {
                            SYMBOL* shr = hr->p;
                            if (ismemberdata(shr))
                            {
                                initInsert(it, pinit->tp, pinit->exp, shr->offset, false);
                                it = &(*it)->next;
                                pinit = pinit->next;
                            }
                            hr = hr->next;
                        }
                        p->exp = convertInitToExpression(stype, nullptr, nullptr, theCurrentFunc, init, thisptr, false);
                        if (!isref(sym->tp))
                            sp->stackblock = true;
                        done = true;
                    }
                    else
                    {
                        TYPE* ctype = sp->tp;
                        EXPRESSION* dexp = thisptr;
                        p->exp = thisptr;
                        callConstructorParam(&ctype, &p->exp, pinit->tp, pinit->exp, true, true, implicit, false);
                        if (!isref(sym->tp))
                        {
                            sp->stackblock = true;
                        }
                        else
                        {
                            callDestructor(stype->sp, nullptr, &dexp, nullptr, true, false, false);
                            if (dexp)
                                p->dest = dexp;
                        }
                        done = true;
                    }
                    p->tp = sym->tp;
                }
                else if (ispointer(sym->tp))
                {
                    INITIALIZER *init = nullptr, **it = &init;
                    EXPRESSION* thisptr = anonymousVar(theCurrentFunc ? sc_auto : sc_localstatic, sym->tp);
                    SYMBOL* sp = thisptr->v.sp;
                    int n = 0;
                    TYPE* btp = sym->tp;
                    while (isarray(btp))
                        btp = basetype(btp)->btp;
                    if (!theCurrentFunc)
                    {
                        sp->label = nextLabel++;
                        insertInitSym(sp);
                    }
                    if (!isarray(sym->tp))
                    {
                        TYPE* gtype = (TYPE*)Alloc(sizeof(TYPE));
                        INITLIST* xx = pinit;
                        *gtype = *sym->tp;
                        UpdateRootTypes(gtype);
                        gtype->array = true;
                        gtype->esize = intNode(en_c_i, n);
                        while (xx)
                        {
                            n++;
                            xx = xx->next;
                        }
                        sym->tp = gtype;
                        sym->tp->size = btp->size * n;
                    }
                    n = 0;
                    while (pinit)
                    {
                        initInsert(it, pinit->tp, pinit->exp, n, false);
                        it = &(*it)->next;
                        n += btp->size;
                        pinit = pinit->next;
                    }
                    p->exp = convertInitToExpression(sym->tp, nullptr, nullptr, theCurrentFunc, init, thisptr, false);
                    p->tp = sym->tp;
                    done = true;
                }
                else
                {
                    // defer to below processing
                    if (pinit)
                    {
                        p->exp = pinit->exp;
                        p->tp = pinit->tp;
                    }
                    else
                    {
                        p->exp = intNode(en_c_i, 0);
                        p->tp = &stdint;
                    }
                }
            }
            if (!done && p->exp)
            {
                if (isstructured(sym->tp))
                {
                    bool sameType = false;
                    EXPRESSION* temp = p->exp;
                    TYPE* tpx = p->tp;
                    sym->tp = PerformDeferredInitialization(sym->tp, nullptr);
                    if (isref(tpx))
                        tpx = basetype(tpx)->btp;
                    // result of a nested constructor
                    if (temp->type == en_thisref)
                    {
                        temp = p->exp->left;
                    }
                    // use constructor or conversion function and push on stack ( no destructor)
                    if (temp->type == en_func && basetype(temp->v.func->sp->tp)->btp &&
                        !isref(basetype(temp->v.func->sp->tp)->btp) &&
                        ((sameType = comparetypes(sym->tp, tpx, true)) ||
                         classRefCount(basetype(sym->tp)->sp, basetype(tpx)->sp) == 1))
                    {
                        SYMBOL* esp;
                        EXPRESSION* consexp;
                        // copy constructor...
                        TYPE* ctype = sym->tp;
                        EXPRESSION* paramexp;
                        consexp = anonymousVar(sc_auto, sym->tp);  // sc_parameter to push it...
                        esp = consexp->v.sp;
                        esp->stackblock = true;
                        consexp = varNode(en_auto, esp);
                        paramexp = temp->v.func->returnEXP ? temp->v.func->returnEXP : p->exp;
                        paramexp = DerivedToBase(sym->tp, tpx, paramexp, _F_VALIDPOINTER);
                        callConstructorParam(&ctype, &consexp, sym->tp, paramexp, true, true, implicit, false);
                        if (paramexp != p->exp)
                            p->exp = exprNode(en_void, p->exp, consexp);
                        else
                            p->exp = consexp;
                    }
                    else
                    {
                        TYPE* ctype = sym->tp;
                        EXPRESSION* consexp = anonymousVar(sc_auto, sym->tp);  // sc_parameter to push it...
                        SYMBOL* esp = consexp->v.sp;
                        esp->stackblock = true;
                        callConstructorParam(&ctype, &consexp, p->tp, p->exp, true, true, implicit, false);
                        p->exp = consexp;
                    }
                    p->tp = sym->tp;
                }
                else if (isref(sym->tp))
                {
                    if (isstructured(basetype(sym->tp)->btp))
                    {
                        TYPE* tpx = p->tp;
                        if (isref(tpx))
                            tpx = basetype(tpx)->btp;
                        if ((!isconst(basetype(sym->tp)->btp) && !isconst(sym->tp) &&
                             (sym->tp->type != bt_rref ||
                              (!func->templateLevel && (!func->parentClass || !func->parentClass->templateLevel) /*forward*/)) &&
                             isconst(tpx)) ||
                            (!comparetypes(sym->tp, tpx, true) && !sameTemplate(sym->tp, tpx) &&
                             !classRefCount(basetype(basetype(sym->tp)->btp)->sp, basetype(tpx)->sp)))
                        {
                            // make temp via constructor or conversion function
                            EXPRESSION* consexp = anonymousVar(sc_auto, basetype(sym->tp)->btp);  // sc_parameter to push it...
                            SYMBOL* esp = consexp->v.sp;
                            TYPE* ctype = basetype(sym->tp)->btp;
                            EXPRESSION* paramexp = p->exp;
                            p->exp = consexp;
                            callConstructorParam(&ctype, &p->exp, basetype(p->tp), paramexp, true, true, false, false);
                            if (p->exp->type == en_func)
                            {
                                SYMBOL* spx = p->exp->v.func->sp;
                                TYPE* tpx = basetype(spx->tp);
                                TYPE *tpx1, *tpx2;
                                if (spx->castoperator)
                                {
                                    tpx1 = spx->parentClass->tp;
                                    tpx2 = tpx->btp;
                                    if (isref(tpx2))
                                        tpx2 = basetype(tpx2)->btp;
                                }
                                else
                                {
                                    tpx1 = ((SYMBOL*)tpx->syms->table[0]->next->p)->tp;
                                    tpx2 = spx->parentClass->tp;
                                }
                                esp->tp = tpx1;  // guaranteed to be a structured type or reference to one
                                p->exp->v.func->thisptr = DerivedToBase(tpx1, p->tp, p->exp->v.func->thisptr, _F_VALIDPOINTER);
                                if (isstructured(tpx2) || (isref(tpx2) && isstructured(basetype(tpx2)->btp)))
                                    p->exp = DerivedToBase(sym->tp, tpx2, p->exp, 0);
                                else
                                    cast(sym->tp, &p->exp);
                            }
                        }
                        else
                        {
                            if (!comparetypes(sym->tp, p->tp, true))
                                p->exp = DerivedToBase(sym->tp, p->tp, p->exp, 0);
                        }
                    }
                    else if (basetype(basetype(sym->tp)->btp)->type == bt_memberptr)
                    {
                        TYPE* tp2 = basetype(sym->tp)->btp;
                        if (p->exp->type == en_memberptr)
                        {
                            int lbl = dumpMemberPtr(p->exp->v.sp, tp2, true);
                            p->exp = intNode(en_labcon, lbl);
                        }
                        else if (isconstzero(p->tp, p->exp) || p->exp->type == en_nullptr)
                        {
                            EXPRESSION* dest = createTemporary(tp2, nullptr);
                            p->exp = exprNode(en_blockclear, dest, nullptr);
                            p->exp->size = tp2->size;
                            p->exp = exprNode(en_void, p->exp, dest);
                        }
                        else if (p->exp->type == en_func && p->exp->v.func->returnSP)
                        {
                            int lbl = dumpMemberPtr(p->exp->v.sp, tp2, true);
                            p->exp = intNode(en_labcon, lbl);
                            /*
                            EXPRESSION *dest = createTemporary(tp2, nullptr);
                            p->exp->v.func->returnSP->allocate = false;
                            p->exp->v.func->returnEXP = dest;
                            p->exp = exprNode(en_void, p->exp, dest) ;
                            p->exp = exprNode(en_stackblock, p->exp, nullptr);
                            p->exp->size = tp2->size;
                            */
                        }
                        else if (p->exp->type == en_pc)
                        {
                            int lbl = dumpMemberPtr(p->exp->v.sp, tp2, true);
                            p->exp = intNode(en_labcon, lbl);
                        }
                        p->tp = sym->tp;
                    }
                    else if (comparetypes(sym->tp, p->tp, true))
                    {
                        if (isarithmeticconst(p->exp) ||
                            (basetype(sym->tp)->type != bt_rref && !isconst(basetype(sym->tp)->btp) && isconst(p->tp)))
                        {
                            // make numeric temp and perform cast
                            p->exp = createTemporary(sym->tp, p->exp);
                        }
                        else
                        {
                            // pass address
                            EXPRESSION* exp = p->exp;
                            while (castvalue(exp) || exp->type == en_not_lvalue)
                                exp = exp->left;
                            if (exp->type != en_l_ref)
                            {
                                if (!isref(sym->tp) || !isfunction(basetype(sym->tp)->btp))
                                {
                                    if (!lvalue(exp))
                                    {
                                        TYPE* tp1 = basetype(sym->tp)->btp;
                                        // make numeric temp and perform cast
                                        if (isarray(tp1))
                                        {
                                            TYPE tp2 = {};
                                            tp2.type = bt_lref;
                                            tp2.size = getSize(bt_pointer);
                                            tp2.btp = &stdpointer;
                                            tp2.rootType = &tp2;
                                            exp = createTemporary(&tp2, exp);
                                        }
                                        else
                                        {
                                            exp = createTemporary(sym->tp, exp);
                                        }
                                    }
                                    else if (exp->type == en_lvalue)
                                    {
                                        exp = createTemporary(sym->tp, exp);
                                    }
                                    else
                                    {
                                        exp = exp->left;  // take address
                                    }
                                }
                                p->exp = exp;
                            }
                            else if (ispointer(p->tp) && isstructured(basetype(p->tp)->btp))
                            {
                                // make numeric temp and perform cast
                                p->exp = createTemporary(sym->tp, exp);
                            }
                        }
                    }
                    else if (isstructured(p->tp))
                    {
                        // arithmetic or pointer
                        TYPE* etp = basetype(sym->tp)->btp;
                        if (cppCast(p->tp, &etp, &p->exp))
                            p->tp = etp;
                        p->exp = createTemporary(sym->tp, p->exp);
                    }
                    else
                    {
                        // make numeric temp and perform cast
                        p->exp = createTemporary(sym->tp, p->exp);
                    }
                    p->tp = sym->tp;
                }
                else if (isstructured(p->tp))
                {
                    if (sym->tp->type == bt_ellipse)
                    {
                        p->exp = exprNode(en_stackblock, p->exp, nullptr);
                        p->exp->size = p->tp->size;
                    }
                    else
                    {
                        // arithmetic or pointer
                        TYPE* etp = sym->tp;
                        if (cppCast(p->tp, &etp, &p->exp))
                            p->tp = etp;
                    }
                }
                else if (isvoidptr(sym->tp) && p->tp->type == bt_aggregate)
                {
                    LookupSingleAggregate(p->tp, &p->exp);
                }
                else if (ispointer(sym->tp) && ispointer(p->tp))
                {
                    // handle base class conversion
                    TYPE* tpb = basetype(sym->tp)->btp;
                    TYPE* tpd = basetype(p->tp)->btp;
                    if (cparams.prm_cplusplus && !isconst(basetype(sym->tp)->btp) && basetype(p->tp)->stringconst)
                        error(ERR_INVALID_CHARACTER_STRING_CONVERSION);
                    if (!comparetypes(basetype(tpb), basetype(tpd), true))
                    {
                        if (isstructured(tpb) && isstructured(tpd))
                        {
                            p->exp = DerivedToBase(tpb, tpd, p->exp, 0);
                        }
                        p->tp = sym->tp;
                    }
                }
                else if (basetype(sym->tp)->type == bt_memberptr)
                {
                    if (p->exp->type == en_memberptr)
                    {
                        int lbl = dumpMemberPtr(p->exp->v.sp, sym->tp, true);
                        p->exp = intNode(en_labcon, lbl);
                        p->exp = exprNode(en_stackblock, p->exp, nullptr);
                        p->exp->size = sym->tp->size;
                    }
                    else if (isconstzero(p->tp, p->exp) || p->exp->type == en_nullptr)
                    {
                        EXPRESSION* dest = createTemporary(sym->tp, nullptr);
                        p->exp = exprNode(en_blockclear, dest, nullptr);
                        p->exp->size = sym->tp->size;
                        p->exp = exprNode(en_void, p->exp, dest);
                        p->exp = exprNode(en_stackblock, p->exp, nullptr);
                        p->exp->size = sym->tp->size;
                    }
                    else if (p->exp->type == en_func && p->exp->v.func->returnSP)
                    {
                        EXPRESSION* dest = anonymousVar(sc_auto, sym->tp);
                        SYMBOL* esp = dest->v.sp;
                        int lbl = dumpMemberPtr(p->exp->v.sp, sym->tp, true);
                        esp->stackblock = true;
                        p->exp = intNode(en_labcon, lbl);
                        p->exp = exprNode(en_stackblock, p->exp, nullptr);
                        p->exp->size = sym->tp->size;
                        /*
                        EXPRESSION *dest = createTemporary(sym->tp, nullptr);
                        p->exp->v.func->returnSP->allocate = false;
                        p->exp->v.func->returnEXP = dest;
                        p->exp = exprNode(en_void, p->exp, dest) ;
                        p->exp = exprNode(en_stackblock, p->exp, nullptr);
                        p->exp->size = sym->tp->size;
                        */
                    }
                    else if (p->exp->type == en_pc)
                    {
                        int lbl = dumpMemberPtr(p->exp->v.sp, sym->tp, true);
                        p->exp = intNode(en_labcon, lbl);
                        p->exp = exprNode(en_stackblock, p->exp, nullptr);
                        p->exp->size = sym->tp->size;
                    }
                    else
                    {
                        p->exp = exprNode(en_stackblock, p->exp, nullptr);
                        p->exp->size = sym->tp->size;
                    }
                    p->tp = sym->tp;
                }
            }
        }
        else if (chosenAssembler->msil)
        {
            if (isref(sym->tp))
            {
                if (comparetypes(sym->tp, p->tp, true))
                {
                    if (isarithmeticconst(p->exp) ||
                        (basetype(sym->tp)->type != bt_rref && !isconst(basetype(sym->tp)->btp) && isconst(p->tp)))
                    {
                        // make numeric temp and perform cast
                        p->exp = createTemporary(sym->tp, p->exp);
                    }
                    else
                    {
                        // pass address
                        EXPRESSION* exp = p->exp;
                        while (castvalue(exp) || exp->type == en_not_lvalue)
                            exp = exp->left;
                        if (exp->type != en_l_ref)
                        {
                            if (!isref(sym->tp) || !isfunction(basetype(sym->tp)->btp))
                            {
                                if (!lvalue(exp))
                                {
                                    // make numeric temp and perform cast
                                    exp = createTemporary(sym->tp, exp);
                                }
                                else
                                {
                                    exp = exp->left;  // take address
                                }
                            }
                            p->exp = exp;
                        }
                        else if (ispointer(p->tp) && isstructured(basetype(p->tp)->btp))
                        {
                            // make numeric temp and perform cast
                            p->exp = createTemporary(sym->tp, exp);
                        }
                    }
                }
                else
                {
                    // make numeric temp and perform cast
                    p->exp = createTemporary(sym->tp, p->exp);
                }
                p->tp = sym->tp;
            }
            else
            {
                if (basetype(sym->tp)->type == bt___string)
                {
                    if ((basetype(p->tp)->type == bt___string) || (p->exp->type == en_labcon && p->exp->string))
                    {
                        if (p->exp->type == en_labcon)
                            p->exp->type = en_c_string;
                    }
                    else if ((isarray(p->tp) || ispointer(p->tp)) && !basetype(p->tp)->msil &&
                             basetype(basetype(p->tp)->btp)->type == bt_char)
                    {
                        // make a 'string' object and initialize it with the string
                        TYPE* ctype = chosenAssembler->msil->find_boxed_type(basetype(sym->tp));
                        EXPRESSION *exp1, *exp2;
                        exp1 = exp2 = anonymousVar(sc_auto, &std__string);
                        callConstructorParam(&ctype, &exp2, p->tp, p->exp, true, true, false, false);
                        exp2 = exprNode(en_l_string, exp2, nullptr);
                        p->exp = exp2;
                        p->tp = &std__string;
                    }
                }
                else if (basetype(sym->tp)->type == bt___object)
                {
                    if (basetype(p->tp)->type != bt___object && !isstructured(p->tp) && (!isarray(p->tp) || !basetype(p->tp)->msil))
                    {
                        if ((isarray(p->tp) || ispointer(p->tp)) && !basetype(p->tp)->msil &&
                            basetype(basetype(p->tp)->btp)->type == bt_char)
                        {
                            // make a 'string' object and initialize it with the string
                            TYPE* ctype = chosenAssembler->msil->find_boxed_type(&std__string);
                            EXPRESSION *exp1, *exp2;
                            exp1 = exp2 = anonymousVar(sc_auto, &std__string);
                            callConstructorParam(&ctype, &exp2, p->tp, p->exp, true, true, false, false);
                            exp2 = exprNode(en_l_string, exp2, nullptr);
                            p->exp = exp2;
                            p->tp = &std__string;
                        }
                        else
                            p->exp = exprNode(en_x_object, p->exp, nullptr);
                    }
                }
                else if (ismsil(p->tp))
                    ;  // error
                // legacy c language support
                else if (p && p->tp && isstructured(p->tp) && (!basetype(p->tp)->sp->msil || !isconstzero(p->tp, p->exp)))
                {
                    p->exp = exprNode(en_stackblock, p->exp, nullptr);
                    p->exp->size = p->tp->size;
                }
            }
        }
        else
        {
            // legacy c language support
            if (p && p->tp)
            {
                if (isstructured(p->tp))
                {
                    p->exp = exprNode(en_stackblock, p->exp, nullptr);
                    p->exp->size = p->tp->size;
                }
                else if (isfloat(sym->tp) || isimaginary(sym->tp) || iscomplex(sym->tp))
                {
                    cast(sym->tp, &p->exp);
                    optimize_for_constants(&p->exp);
                }
            }
        }
        hr = hr->next;
        lptr = &(*lptr)->next;
    }
    while (*lptr)  // take care of elliptical arguments and arguments without a prototype
    {
        INITLIST* p = *lptr;
        if (!p->tp || !p->exp)
        {
            p->tp = &stdint;
            p->exp = intNode(en_c_i, 0);
        }
        if (func->msil && p->exp->type == en_labcon && p->exp->string)
        {
            p->exp->type = en_c_string;
            p->tp = &(std__string);
        }
        if (isstructured(p->tp))
        {
            p->exp = exprNode(en_stackblock, p->exp, nullptr);
            p->exp->size = p->tp->size;
        }
        else if (p->tp->type == bt_float)
        {
            cast(&stddouble, &p->exp);
        }
        lptr = &(*lptr)->next;
    }
}
LEXEME* expression_arguments(LEXEME* lex, SYMBOL* funcsp, TYPE** tp, EXPRESSION** exp, int flags)
{
    TYPE* tp_cpp = *tp;
    EXPRESSION* exp_cpp = *exp;
    FUNCTIONCALL* funcparams;
    INITLIST** lptr;
    EXPRESSION* exp_in = *exp;
    bool operands = false;
    bool hasThisPtr = false;
    TYPE* initializerListType = nullptr;
    TYPE* initializerListTemplate = nullptr;
    bool initializerRef = false;
    bool addedThisPointer = false;
    bool memberPtr = false;
    if (exp_in->type != en_func || isfuncptr(*tp) || isstructured(*tp))
    {
        TYPE* tpx = *tp;
        SYMBOL* sym;
        if (exp_in->type == en_templateselector)
        {
            TEMPLATESELECTOR* tsl = exp_in->v.templateSelector;
            SYMBOL* ts = tsl->next->sp;
            SYMBOL* sp = ts;
            TEMPLATESELECTOR* find = tsl->next->next;
            if (!sp->instantiated && tsl->next->isTemplate)
            {
                TEMPLATEPARAMLIST* current = tsl->next->templateParams;
                sp = GetClassTemplate(ts, current, true);
            }
            if (sp && sp->tp->type == bt_templateselector)
            {
                TYPE* tp = sp->tp;
                tp = SynthesizeType(tp, nullptr, false);
                if (tp && isstructured(tp))
                    sp = basetype(tp)->sp;
            }
            sym = nullptr;
            if (sp)
            {
                sp = basetype(PerformDeferredInitialization(sp->tp, nullptr))->sp;
                while (find && sp)
                {
                    SYMBOL* spo = sp;
                    if (!isstructured(spo->tp))
                        break;

                    sp = search(find->name, spo->tp->syms);
                    if (!sp)
                    {
                        sp = classdata(find->name, spo, nullptr, false, false);
                        if (sp == (SYMBOL*)-1)
                            sp = nullptr;
                    }
                    find = find->next;
                }
                if (!find)
                    sym = sp;
            }
        }
        else
        {
            if (ispointer(tpx))
                tpx = basetype(tpx)->btp;
            sym = basetype(tpx)->sp;
        }
        funcparams = (FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
        if (sym)
        {
            funcparams->sp = sym;
            funcparams->functp = sym->tp;
            funcparams->fcall = *exp;
            *exp = varNode(en_func, nullptr);
            (*exp)->v.func = funcparams;
        }
        else if (!templateNestingCount)
            error(ERR_CALL_OF_NONFUNCTION);
    }
    else
    {
        SYMBOL* ss = getStructureDeclaration();
        funcparams = exp_in->v.func;
        hasThisPtr = funcparams->thisptr != nullptr;
        if (basetype(*tp)->sp)
            funcparams->sp = basetype(*tp)->sp;
        if (ss)
        {
            funcparams->functp = ss->tp;
        }
    }

    if ((!templateNestingCount || instantiatingTemplate) && funcparams->sp && funcparams->sp->name[0] == '_' &&
        parseBuiltInTypelistFunc(&lex, funcsp, funcparams->sp, tp, exp))
        return lex;

    if (lex)
    {
        lex = getArgs(lex, funcsp, funcparams, closepa, true, flags);
    }

    if (funcparams->astemplate && argument_nesting)
    {
        // if we hit a packed template param here, then this is going to be a candidate
        // for some other function's packed expression
        TEMPLATEPARAMLIST* tl = funcparams->templateParams;
        while (tl)
        {
            if (tl->p->packed)
            {
                return lex;
            }
            tl = tl->next;
        }
    }
    if (*tp)
        getFunctionSP(tp);
    if ((*exp)->type == en_funcret)
    {
        (*exp)->v.func = funcparams;
        *exp = exprNode(en_funcret, *exp, nullptr);
        return lex;
    }
    if (chosenAssembler->msil && funcparams->sp)
    {
        if (funcparams->sp->storage_class == sc_overloads)
        {
            // note at this pointer the arglist does NOT have the this pointer,
            // it will be added after we select a member function that needs it.
            funcparams->ascall = true;
            SYMBOL* sym =
                GetOverloadedFunction(tp, &funcparams->fcall, funcparams->sp, funcparams, nullptr, true, false, true, flags);
            if (sym)
            {
                sym->attribs.inheritable.used = true;
                funcparams->sp = sym;
                *tp = sym->tp;
            }
        }
    }
    else if ((cparams.prm_cplusplus || chosenAssembler->msil) && funcparams->sp)
    {
        SYMBOL* sym = nullptr;
        // add this ptr
        if (!funcparams->thisptr && funcparams->sp->parentClass && !isfuncptr(funcparams->sp->tp))
        {
            TYPE *tp = (TYPE*)Alloc(sizeof(TYPE)), *tpx;
            funcparams->thisptr = getMemberBase(funcparams->sp, nullptr, funcsp, false);
            funcparams->thistp = tp;
            tp->type = bt_pointer;
            tp->size = getSize(bt_pointer);
            tpx = tp;
            if (funcsp)
            {
                if (isconst(funcsp->tp))
                {
                    tpx = tpx->btp = (TYPE*)Alloc(sizeof(TYPE));
                    tpx->size = basetype(funcparams->sp->parentClass->tp)->size;
                    tpx->type = bt_const;
                }
                if (isvolatile(funcsp->tp))
                {
                    tpx = tpx->btp = (TYPE*)Alloc(sizeof(TYPE));
                    tpx->size = basetype(funcparams->sp->parentClass->tp)->size;
                    tpx->type = bt_volatile;
                }
            }
            tpx->btp = funcparams->sp->parentClass->tp;
            UpdateRootTypes(tp);
            addedThisPointer = true;
        }
        // we may get here with the overload resolution already done, e.g.
        // for operator or cast function calls...
        if (funcparams->sp->storage_class == sc_overloads)
        {
            TYPE* tp1;
            // note at this pointer the arglist does NOT have the this pointer,
            // it will be added after we select a member function that needs it.
            funcparams->ascall = true;
            sym = GetOverloadedFunction(tp, &funcparams->fcall, funcparams->sp, funcparams, nullptr, true, false, true, flags);
            if (isfunction(*tp))
            {
                if (isstructured(basetype(*tp)->btp))
                {
                    // get rid of any invalid sizing
                    TYPE* tpx = basetype(basetype(*tp)->btp);
                    tpx->size = tpx->sp->tp->size;
                }
            }
            tp1 = *tp;
            while (tp1->btp && tp1->type != bt_bit)
                tp1 = tp1->btp;
            if (sym)
            {
                sym->attribs.inheritable.used = true;
                if (sym->decoratedName[0] == '@' && lex)
                    browse_usage(sym, lex->filenum);
                if (funcparams->astemplate && sym->templateLevel && !sym->specialized)
                {
                    TEMPLATEPARAMLIST* tpln = funcparams->templateParams;
                    TEMPLATEPARAMLIST* tplo = sym->parentTemplate->templateParams->next;
                    while (tpln && tplo)
                    {
                        tpln = tpln->next;
                        tplo = tplo->next;
                    }
                    if (tpln)
                        errorsym(ERR_TOO_MANY_ARGS_PASSED_TO_TEMPLATE, sym);
                }
                sym->throughClass = funcparams->sp->throughClass;
                funcparams->sp = sym;
                if (ismember(sym))
                {
                    if (funcparams->noobject)
                    {
                        if (!funcsp->parentClass || classRefCount(sym->parentClass, funcsp->parentClass) == 0)
                        {
                            errorsym(ERR_USE_DOT_OR_POINTSTO_TO_CALL, sym);
                        }
                        else if (classRefCount(sym->parentClass, funcsp->parentClass) != 1)
                        {
                            errorsym2(ERR_NOT_UNAMBIGUOUS_BASE, sym->parentClass, funcsp->parentClass);
                        }
                        else if (funcsp->storage_class == sc_member || funcsp->storage_class == sc_virtual)
                        {
                            TYPE** cur;
                            funcparams->thisptr = varNode(en_auto, (SYMBOL*)basetype(funcsp->tp)->syms->table[0]->p);
                            deref(&stdpointer, &funcparams->thisptr);
                            funcparams->thisptr =
                                DerivedToBase(sym->parentClass->tp, basetype(funcparams->thisptr->left->v.sp->tp)->btp,
                                              funcparams->thisptr, _F_VALIDPOINTER);
                            funcparams->thistp = (TYPE*)Alloc(sizeof(TYPE));
                            cur = &funcparams->thistp->btp;
                            funcparams->thistp->type = bt_pointer;
                            funcparams->thistp->size = getSize(bt_pointer);
                            if (isconst(sym->tp))
                            {
                                (*cur) = (TYPE*)Alloc(sizeof(TYPE));
                                (*cur)->type = bt_const;
                                (*cur)->size = sym->parentClass->tp->size;
                                cur = &(*cur)->btp;
                            }
                            if (isvolatile(sym->tp))
                            {
                                (*cur) = (TYPE*)Alloc(sizeof(TYPE));
                                (*cur)->type = bt_volatile;
                                (*cur)->size = sym->parentClass->tp->size;
                                cur = &(*cur)->btp;
                            }
                            *cur = sym->parentClass->tp;
                            UpdateRootTypes(funcparams->thistp->btp);
                            cppCast(((SYMBOL*)basetype(funcsp->tp)->syms->table[0]->p)->tp, &funcparams->thistp,
                                    &funcparams->thisptr);
                        }
                    }
                }
            }
        }
        else
        {
            operands = !ismember(funcparams->sp) && funcparams->thisptr && !addedThisPointer;
            if (!isExpressionAccessible(funcparams->thistp ? basetype(basetype(funcparams->thistp)->btp)->sp : nullptr,
                                        funcparams->sp, funcsp, funcparams->thisptr, false))
                if (!isExpressionAccessible(funcparams->thistp ? basetype(basetype(funcparams->thistp)->btp)->sp : nullptr,
                                            funcparams->sp, funcsp, funcparams->thisptr, false))
                    errorsym(ERR_CANNOT_ACCESS, funcparams->sp);
        }
        if (sym)
        {
            bool test;
            *tp = sym->tp;
            if (hasThisPtr)
            {
                test = isExpressionAccessible(basetype(basetype(funcparams->thistp)->btp)->sp, sym, funcsp, funcparams->thisptr,
                                              false);
            }
            else
            {
                test = isExpressionAccessible(funcparams->thistp ? basetype(basetype(funcparams->thistp)->btp)->sp : nullptr, sym,
                                              funcsp, funcparams->thisptr, false);
            }
            if (!test)
            {
                errorsym(ERR_CANNOT_ACCESS, sym);
            }
            if (ismember(funcparams->sp))
            {
                if (funcparams->thistp && isconst(basetype(funcparams->thistp)->btp))
                    if (!isconst(*tp))
                        errorsym(ERR_NON_CONST_FUNCTION_CALLED_FOR_CONST_OBJECT, funcparams->sp);
            }
        }
    }
    if (basetype(*tp)->type == bt_memberptr)
    {
        memberPtr = true;
        *tp = basetype(*tp)->btp;
    }
    if (!isfunction(*tp))
    {
        // might be operator ()
        if (cparams.prm_cplusplus)
        {
            if (insertOperatorParams(funcsp, &tp_cpp, &exp_cpp, funcparams, flags))
            {
                hasThisPtr = funcparams->thisptr != nullptr;
                *tp = tp_cpp;
                *exp = exp_cpp;
            }
        }
    }
    {
        HASHTABLE* temp = basetype(*tp)->syms;
        if (temp)
        {
            SYMLIST* hr = temp->table[0];

            if (funcparams->sp && !ismember(funcparams->sp) && !memberPtr)
            {
                if (operands)
                {
                    INITLIST* al = (INITLIST*)Alloc(sizeof(INITLIST));
                    al->exp = funcparams->thisptr;
                    al->tp = funcparams->thistp;
                    al->next = funcparams->arguments;
                    funcparams->arguments = al;
                }
                funcparams->thisptr = nullptr;
            }
            else
            {
                if (!getStructureDeclaration() && !ispointer(tp_cpp) && !hasThisPtr)
                    errorsym(ERR_ACCESS_MEMBER_NO_OBJECT, funcparams->sp);
                operands = false;
            }
            lptr = &funcparams->arguments;
            if (funcparams->sp)
            {
                if (funcparams->sp->parentClass == nullptr && funcparams->sp->parentNameSpace == nullptr)
                {
                    if (!strcmp(funcparams->sp->name, "setjmp"))
                        setjmp_used = true;
                    if (!strcmp(funcparams->sp->name, "exit"))
                        isCallExit = true;
                }
                TYPE* tp1 = funcparams->sp->tp;
                if (ispointer(tp1))
                    tp1 = basetype(tp1)->btp;
                if (isfunction(tp1))
                {
                    hr = basetype(tp1)->syms->table[0];
                    if (hr)
                    {
                        if (hr->p->thisPtr)
                            hr = hr->next;
                        if (!hr->next || (hr->next->p)->init)
                        {
                            TYPE* tp = hr->p->tp;
                            if (isref(tp))
                            {
                                initializerRef = true;
                                tp = basetype(tp)->btp;
                            }
                            if (isstructured(tp))
                            {
                                SYMBOL* sym = (basetype(tp)->sp);
                                if (sym->parentNameSpace && !strcmp(sym->parentNameSpace->name, "std"))
                                {
                                    if (!strcmp(sym->name, "initializer_list") && sym->templateLevel)
                                    {
                                        initializerListTemplate = sym->tp;
                                        initializerListType = sym->templateParams->next->p->byClass.val;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (!(flags & _F_SIZEOF))
            {
                if (initializerListType)
                {
                    CreateInitializerList(initializerListTemplate, initializerListType, lptr, operands, initializerRef);
                    if (hr->next)
                        AdjustParams(funcparams->sp, hr->next, &(*lptr)->next, operands, true);
                }
                else
                {
                    AdjustParams(funcparams->sp, hr, lptr, operands, true);
                }
            }
            if (funcparams->sp->xcMode != xc_unspecified && funcparams->sp->xcMode != xc_none)
                hasFuncCall = true;
            CheckCalledException(funcparams->sp, funcparams->thisptr);
            if (cparams.prm_cplusplus)
            {
                lptr = &funcparams->arguments;
                while (*lptr)
                {
                    if (isstructured((*lptr)->tp) && (*lptr)->exp)
                    {
                        EXPRESSION* exp = (*lptr)->exp;
                        if (exp->type == en_not_lvalue)
                            exp = exp->left;
                        if (exp->type == en_auto)
                        {
                            if (!funcparams->callLab)
                                funcparams->callLab = -1;
                        }
                        else if (exp->type == en_thisref)
                        {
                            if (!funcparams->callLab)
                                funcparams->callLab = -1;
                        }
                    }
                    lptr = &(*lptr)->next;
                }
            }
            if (isfunction(*tp))
            {
                if (funcparams->thisptr && !memberPtr)
                {
                    SYMBOL* base = funcparams->sp->parentClass;
                    SYMBOL* derived = basetype(basetype(funcparams->thistp)->btp)->sp;
                    if (base != derived)
                    {
                        funcparams->thisptr = DerivedToBase(base->tp, derived->tp, funcparams->thisptr, _F_VALIDPOINTER);
                    }
                }
                if (isstructured(basetype(*tp)->btp) || basetype(basetype(*tp)->btp)->type == bt_memberptr)
                {
                    if (!(flags & _F_SIZEOF))
                    {
                        funcparams->returnEXP = anonymousVar(sc_auto, basetype(*tp)->btp);
                        funcparams->returnSP = funcparams->returnEXP->v.sp;
                    }
                }
                funcparams->ascall = true;
                funcparams->functp = *tp;
                {
                    TYPE** tp1;
                    *tp = basetype(*tp)->btp;
                    if (isref(*tp))
                    {
                        TYPE* tp1 = *tp;
                        *tp = (TYPE*)Alloc(sizeof(TYPE));
                        **tp = *(tp1->btp);
                        UpdateRootTypes(*tp);
                        if ((*tp)->type == bt_rref)
                        {
                            (*tp)->rref = true;
                            (*tp)->lref = false;
                        }
                        else
                        {
                            (*tp)->lref = true;
                            (*tp)->rref = false;
                        }
                    }
                    else if (ispointer(*tp) && (*tp)->array)
                    {
                        (*tp)->lref = true;
                        (*tp)->rref = false;
                    }
                    tp1 = tp;
                    while (ispointer(*tp1) || basetype(*tp1)->type == bt_memberptr)
                        tp1 = &basetype(*tp1)->btp;
                    while ((*tp1)->btp)
                        tp1 = &(*tp1)->btp;
                    if (isstructured(*tp1))
                    {
                        *tp1 = (*tp1)->sp->tp;
                    }
                }
                if (!(flags & _F_SIZEOF))
                {
                    checkArgs(funcparams, funcsp);
                }
                if (funcparams->returnSP)
                {
                    SYMBOL* sym = basetype(funcparams->returnSP->tp)->sp;
                    if (sym->templateLevel && sym->templateParams && !sym->instantiated)
                    {
                        if (!allTemplateArgsSpecified(sym, sym->templateParams))
                            sym = GetClassTemplate(sym, sym->templateParams->next, false);
                        if (sym && allTemplateArgsSpecified(sym, sym->templateParams))
                            funcparams->returnSP->tp = TemplateClassInstantiate(sym, sym->templateParams, false, sc_global)->tp;
                    }
                }
                if (!funcparams->novtab && funcparams->sp && funcparams->sp->storage_class == sc_virtual)
                {
                    exp_in = funcparams->thisptr;
                    deref(&stdpointer, &exp_in);
                    exp_in = exprNode(en_add, exp_in, intNode(en_c_i, funcparams->sp->vtaboffset));
                    deref(&stdpointer, &exp_in);
                    funcparams->fcall = exp_in;
                }
                else
                {
                    exp_in = varNode(en_func, nullptr);
                    exp_in->v.func = funcparams;
                    if (exp_in && cparams.prm_cplusplus && funcparams->returnEXP)
                    {
                        if (!basetype(funcparams->returnSP->tp)->sp->trivialCons)
                        {
                            EXPRESSION* expx;
                            exp_in = exprNode(en_thisref, exp_in, nullptr);
                            exp_in->v.t.thisptr = funcparams->returnEXP;
                            exp_in->v.t.tp = funcparams->returnSP->tp;

                            expx = funcparams->returnEXP;
                            callDestructor(basetype(funcparams->returnSP->tp)->sp, nullptr, &expx, nullptr, true, false, true);
                            initInsert(&funcparams->returnSP->dest, funcparams->returnSP->tp, expx, 0, true);
                        }
                    }
                    if (exp_in)
                    {
                        TYPE* tp = basetype(funcparams->sp->tp)->btp;
                        if ((flags & _F_AMPERSAND) && isarithmetic(tp))
                        {
                            EXPRESSION* rv = anonymousVar(sc_auto, tp);
                            deref(tp, &rv);
                            exp_in = exprNode(en_void, exprNode(en_assign, rv, exp_in), rv);
                            errortype(ERR_CREATE_TEMPORARY, tp, tp);
                        }
                    }
                    if (exp_in)
                        *exp = exp_in;
                }
                if (funcparams->sp && isfunction(funcparams->sp->tp) && isref(basetype(funcparams->sp->tp)->btp))
                {
                    TYPE** tp1;
                    TYPE* tp2 = (TYPE*)Alloc(sizeof(TYPE));
                    deref(basetype(basetype(funcparams->sp->tp)->btp)->btp, exp);
                    tp1 = &basetype(funcparams->sp->tp)->btp;
                    *tp2 = *basetype(*tp1)->btp;
                    UpdateRootTypes(tp2);
                    *tp = tp2;
                    if (basetype(*tp1)->type == bt_rref)
                    {
                        (*tp)->rref = true;
                        (*tp)->lref = false;
                    }
                    else
                    {
                        (*tp)->lref = true;
                        (*tp)->rref = false;
                    }
                    while (isref(*tp))
                        *tp = basetype(*tp)->btp;
                }
            }
            else if (templateNestingCount && !instantiatingTemplate && (*tp)->type == bt_aggregate)
            {
                *exp = exprNode(en_funcret, *exp, nullptr);
                *tp = &stdvoid;
            }
            else
            {
                if (!templateNestingCount)
                    error(ERR_CALL_OF_NONFUNCTION);
                *tp = &stdvoid;
            }
        }
        else
        {
            *tp = &stdint;
            if (!templateNestingCount)
                error(ERR_CALL_OF_NONFUNCTION);
        }
    }
    return lex;
}
static LEXEME* expression_alloca(LEXEME* lex, SYMBOL* funcsp, TYPE** tp, EXPRESSION** exp, int flags)
{
    lex = getsym();
    if (needkw(&lex, openpa))
    {
        lex = expression_comma(lex, funcsp, nullptr, tp, exp, nullptr, flags);
        if (*tp)
        {
            ResolveTemplateVariable(tp, exp, &stdint, nullptr);
            if (!isint(*tp))
                error(ERR_NEED_INTEGER_EXPRESSION);
            optimize_for_constants(exp);

            funcsp->allocaUsed = true;
            *exp = exprNode(en_alloca, *exp, 0);
            needkw(&lex, closepa);
            *tp = &stdpointer;
        }
        else
        {
            error(ERR_EXPRESSION_SYNTAX);
            *tp = nullptr;
        }
    }
    else
    {
        errskim(&lex, skim_closepa);
        skip(&lex, closepa);
        *tp = nullptr;
    }
    return lex;
}
static LEXEME* expression_offsetof(LEXEME* lex, SYMBOL* funcsp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    *exp = intNode(en_c_i, 0);
    lex = getsym();
    if (needkw(&lex, openpa))
    {
        // this is naive, not checking the actual type of the sym when these are present
        if (MATCHKW(lex, kw_struct) || MATCHKW(lex, kw_class))
            lex = getsym();
        if (ISID(lex))
        {
            char name[512];
            SYMBOL* sym;
            HASHTABLE* table;
            SYMBOL* strSym;
            NAMESPACEVALUELIST* nsv;
            strcpy(name, lex->value.s.a);
            lex = tagsearch(lex, name, &sym, &table, &strSym, &nsv, sc_global);
            if (!sym)  // might be a typedef
            {

                sym = namespacesearch(name, globalNameSpace, false, false);
                if (sym->tp->type == bt_typedef)
                {
                    sym = basetype(sym->tp->btp)->sp;
                }
                else
                {
                    sym = nullptr;
                }
            }
            if (sym && isstructured(sym->tp))
            {
                if (needkw(&lex, comma))
                {
                    *tp = sym->tp;
                    do
                    {
                        lex = expression_member(lex, funcsp, tp, exp, ismutable, flags | _F_AMPERSAND);
                        if (lvalue(*exp))
                            *exp = (*exp)->left;
                    } while (MATCHKW(lex, dot));
                }
            }
            else
            {
                error(ERR_CLASS_TYPE_EXPECTED);
            }
        }
        else
        {
            error(ERR_IDENTIFIER_EXPECTED);
        }
        needkw(&lex, closepa);
    }
    *tp = &stdunsigned;

    return lex;
}
static LEXEME* expression_msilfunc(LEXEME* lex, SYMBOL* funcsp, TYPE** tp, EXPRESSION** exp, int flags)
{
    enum e_kw kw = lex->kw->key;
    lex = getsym();
    if (MATCHKW(lex, openpa))
    {
        FUNCTIONCALL funcparams;
        memset(&funcparams, 0, sizeof(funcparams));
        lex = getArgs(lex, funcsp, &funcparams, closepa, true, flags);
        int n = 0;
        INITLIST* args = funcparams.arguments;
        for (n = 0; args; args = args->next)
            n++;
        if (n > 3)
            errorstr(ERR_PARAMETER_LIST_TOO_LONG, "__cpblk/__initblk");
        else if (n < 3)
            errorstr(ERR_PARAMETER_LIST_TOO_SHORT, "__cpblk/__initblk");
        switch (kw)
        {
            case kw__cpblk:
                if (!ispointer(funcparams.arguments->tp))
                {
                    *exp = intNode(en_c_i, 0);
                    errorstr2(ERR_TYPE_MISMATCH_IN_ARGUMENT, "__dest", "__cpblk");
                }
                else if (!ispointer(funcparams.arguments->next->tp))
                {
                    *exp = intNode(en_c_i, 0);
                    errorstr2(ERR_TYPE_MISMATCH_IN_ARGUMENT, "__src", "__cpblk");
                }
                else if (!isint(funcparams.arguments->next->next->tp))
                {
                    *exp = intNode(en_c_i, 0);
                    errorstr2(ERR_TYPE_MISMATCH_IN_ARGUMENT, "__size", "__cpblk");
                }
                else
                {
                    *exp = exprNode(en_void, funcparams.arguments->exp, funcparams.arguments->next->exp);
                    *exp = exprNode(en__cpblk, *exp, funcparams.arguments->next->next->exp);
                }
                break;
            case kw__initblk:
                if (!ispointer(funcparams.arguments->tp))
                {
                    *exp = intNode(en_c_i, 0);
                    errorstr2(ERR_TYPE_MISMATCH_IN_ARGUMENT, "__dest", "__initblk");
                }
                else if (!isint(funcparams.arguments->next->tp))
                {
                    *exp = intNode(en_c_i, 0);
                    errorstr2(ERR_TYPE_MISMATCH_IN_ARGUMENT, "__value", "__initblk");
                }
                else if (!isint(funcparams.arguments->next->next->tp))
                {
                    *exp = intNode(en_c_i, 0);
                    errorstr2(ERR_TYPE_MISMATCH_IN_ARGUMENT, "__size", "__initblk");
                }
                else
                {
                    *exp = exprNode(en_void, funcparams.arguments->exp, funcparams.arguments->next->exp);
                    *exp = exprNode(en__initblk, *exp, funcparams.arguments->next->next->exp);
                }
                break;
            default:
                *exp = intNode(en_c_i, 0);
                break;
        }
        *tp = &stdpointer;
    }
    else
    {
        errskim(&lex, skim_closepa);
        skip(&lex, closepa);
        *tp = nullptr;
    }
    return lex;
}
static LEXEME* expression_string(LEXEME* lex, SYMBOL* funcsp, TYPE** tp, EXPRESSION** exp)
{
    int elems = 0;
    STRING* data;
    (void)funcsp;

    IncGlobalFlag();
    lex = concatStringsInternal(lex, &data, &elems);
    *exp = stringlit(data);
    DecGlobalFlag();
    if (data->suffix)
    {
        enum e_bt tpb = (*tp)->type;
        SYMBOL* sym;
        char name[512];
        switch (data->strtype)
        {
            default:
            case l_astr:
                tpb = stdcharptr.btp->type;
                break;
            case l_wstr:
                tpb = stdwcharptr.btp->type;
                break;
            case l_msilstr:
                tpb = std__string.type;
                break;
            case l_ustr:
                tpb = stdchar16tptr.btp->type;
                break;
            case l_Ustr:
                tpb = stdchar32tptr.btp->type;
                break;
        }
        my_sprintf(name, "%s@%s", overloadNameTab[CI_LIT], data->suffix);
        sym = LookupSym(name);
        if (sym)
        {
            SYMLIST *hr = sym->tp->syms->table[0], *hr1;
            SYMBOL *sym1, *sym2;
            while (hr)
            {
                sym1 = hr->p;
                hr1 = sym1->tp->syms->table[0];
                sym2 = (SYMBOL*)hr1->p;
                if (hr1->next && ispointer(sym2->tp))
                    if (isconst(sym2->tp->btp) && basetype(sym2->tp->btp)->type == tpb)
                        break;
                hr = hr->next;
            }
            if (hr)
            {
                FUNCTIONCALL* f = (FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
                f->sp = sym1;
                f->functp = sym1->tp;
                f->fcall = varNode(en_pc, sym1);
                f->arguments = (INITLIST*)Alloc(sizeof(INITLIST));
                f->arguments->tp = sym2->tp;
                f->arguments->exp = *exp;
                f->arguments->next = (INITLIST*)Alloc(sizeof(INITLIST));
                f->arguments->next->tp = &stdunsigned;
                f->arguments->next->exp = intNode(en_c_i, elems);
                *exp = intNode(en_func, 0);
                (*exp)->v.func = f;
                *tp = sym1->tp;
                expression_arguments(nullptr, funcsp, tp, exp, false);
                return lex;
            }
        }
        errorstr(ERR_COULD_NOT_FIND_A_MATCH_FOR_LITERAL_SUFFIX, data->suffix);
    }
    *tp = (TYPE*)Alloc(sizeof(TYPE));
    if (data->strtype == l_msilstr)
        **tp = std__string;
    else
    {
        (*tp)->type = bt_pointer;
        (*tp)->array = true;
        (*tp)->stringconst = true;
        (*tp)->rootType = (*tp);
        (*tp)->esize = intNode(en_c_i, elems + 1);
        switch (data->strtype)
        {
            default:
            case l_astr:
                (*tp)->btp = stdcharptr.btp;
                break;
            case l_wstr:
                (*tp)->btp = stdwcharptr.btp;
                break;
            case l_ustr:
                (*tp)->btp = stdchar16tptr.btp;
                break;
            case l_Ustr:
                (*tp)->btp = stdchar32tptr.btp;
                break;
        }
    }
    (*tp)->rootType = (*tp);
    if ((*tp)->type == bt___string)
        (*tp)->size = 1;
    else
        (*tp)->size = (elems + 1) * (*tp)->btp->size;
    return lex;
}
static LEXEME* expression_generic(LEXEME* lex, SYMBOL* funcsp, TYPE** tp, EXPRESSION** exp, int flags)
{
    lex = getsym();
    if (!needkw(&lex, openpa))
    {
        errskim(&lex, skim_closepa);
        skip(&lex, closepa);
        *exp = intNode(en_c_i, 0);
        *tp = &stdint;
    }
    else
    {
        TYPE* selectType = nullptr;
        EXPRESSION* throwawayExpression = nullptr;
        lex = expression_assign(lex, funcsp, nullptr, &selectType, &throwawayExpression, nullptr, flags);
        if (MATCHKW(lex, comma))
        {
            bool dflt = false;
            struct genericHold
            {
                struct genericHold* next;
                TYPE* selector;
                TYPE* type;
                EXPRESSION* exp;
            };
            struct genericHold *list = nullptr, **pos = &list;
            struct genericHold* selectedGeneric = nullptr;
            while (MATCHKW(lex, comma))
            {
                struct genericHold *next = (struct genericHold*)Alloc(sizeof(struct genericHold)), *scan;
                lex = getsym();
                if (MATCHKW(lex, kw_default))
                {
                    lex = getsym();
                    if (dflt)
                    {
                        error(ERR_GENERIC_TOO_MANY_DEFAULTS);
                    }
                    dflt = true;
                    next->selector = nullptr;
                }
                else
                {
                    lex = get_type_id(lex, &next->selector, funcsp, sc_cast, false, true);
                    if (!next->selector)
                    {
                        error(ERR_GENERIC_MISSING_TYPE);
                        break;
                    }
                }
                if (MATCHKW(lex, colon))
                {
                    lex = getsym();
                    lex = expression_assign(lex, funcsp, nullptr, &next->type, &next->exp, nullptr, flags);
                    if (!next->type)
                    {
                        error(ERR_GENERIC_MISSING_EXPRESSION);
                        break;
                    }
                    scan = list;
                    while (scan)
                    {
                        if (scan->selector && next->selector && comparetypes(next->selector, scan->selector, true))
                        {
                            if (isconst(next->selector) == isconst(scan->selector) &&
                                isvolatile(next->selector) == isvolatile(scan->selector) &&
                                isrestrict(next->selector) == isrestrict(scan->selector) &&
                                next->selector->alignment == scan->selector->alignment)
                            {
                                error(ERR_DUPLICATE_TYPE_IN_GENERIC);
                            }
                        }
                        scan = scan->next;
                    }
                    if (!selectedGeneric && !next->selector)
                        selectedGeneric = next;
                    else
                    {
                        if (selectType && next->selector && comparetypes(next->selector, selectType, true))
                        {
                            if (isconst(next->selector) == isconst(selectType) &&
                                isvolatile(next->selector) == isvolatile(selectType) &&
                                isrestrict(next->selector) == isrestrict(selectType) &&
                                next->selector->alignment == selectType->alignment)
                            {
                                if (selectedGeneric && selectedGeneric->selector)
                                    error(ERR_DUPLICATE_TYPE_IN_GENERIC);
                                selectedGeneric = next;
                            }
                        }
                    }
                    *pos = next;
                    pos = &(*pos)->next;
                }
                else
                {
                    error(ERR_GENERIC_MISSING_EXPRESSION);
                    break;
                }
            }
            if (selectedGeneric)
            {
                *tp = selectedGeneric->type;
                *exp = selectedGeneric->exp;
            }
            else
            {
                error(ERR_GENERIC_NOTHING_SELECTED);
                *tp = &stdint;
                *exp = intNode(en_c_i, 0);
            }
            if (!needkw(&lex, closepa))
            {
                errskim(&lex, skim_closepa);
                skip(&lex, closepa);
            }
        }
        else
        {
            errskim(&lex, skim_closepa);
            skip(&lex, closepa);
        }
    }
    return lex;
}
static bool getSuffixedChar(LEXEME* lex, SYMBOL* funcsp, TYPE** tp, EXPRESSION** exp)
{
    char name[512];
    enum e_bt tpb = (*tp)->type;
    SYMBOL* sym;
    my_sprintf(name, "%s@%s", overloadNameTab[CI_LIT], lex->suffix);
    sym = LookupSym(name);
    if (sym)
    {
        SYMLIST *hr = sym->tp->syms->table[0], *hr1;
        SYMBOL *sym1, *sym2;
        while (hr)
        {
            sym1 = hr->p;
            hr1 = sym1->tp->syms->table[0];
            sym2 = (SYMBOL*)hr1->p;
            if (!hr1->next && sym2->tp->type == tpb)
                break;
            hr = hr->next;
        }
        if (hr)
        {
            FUNCTIONCALL* f = (FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
            f->sp = sym1;
            f->functp = sym1->tp;
            f->fcall = varNode(en_pc, sym1);
            f->arguments = (INITLIST*)Alloc(sizeof(INITLIST));
            f->arguments->tp = *tp;
            f->arguments->exp = *exp;
            *exp = intNode(en_func, 0);
            (*exp)->v.func = f;
            *tp = sym1->tp;
            expression_arguments(nullptr, funcsp, tp, exp, false);
            return true;
        }
    }
    errorstr(ERR_COULD_NOT_FIND_A_MATCH_FOR_LITERAL_SUFFIX, lex->suffix);
    return false;
}
static bool getSuffixedNumber(LEXEME* lex, SYMBOL* funcsp, TYPE** tp, EXPRESSION** exp)
{
    char name[512];
    enum e_bt tpb;
    SYMBOL* sym;
    if (lex->type == l_ull)
        tpb = bt_unsigned_long_long;
    else
        tpb = bt_long_double;
    my_sprintf(name, "%s@%s", overloadNameTab[CI_LIT], lex->suffix);
    sym = LookupSym(name);
    if (sym)
    {
        // look for parameter of type unsigned long long or long double
        SYMLIST *hr = sym->tp->syms->table[0], *hr1;
        SYMBOL *sym1, *sym2;
        while (hr)
        {
            sym1 = hr->p;
            hr1 = sym1->tp->syms->table[0];
            sym2 = (SYMBOL*)hr1->p;
            if (!hr1->next && sym2->tp->type == tpb)
                break;
            hr = hr->next;
        }
        if (hr)
        {
            FUNCTIONCALL* f = (FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
            f->sp = sym1;
            f->functp = sym1->tp;
            f->fcall = varNode(en_pc, sym1);
            f->arguments = (INITLIST*)Alloc(sizeof(INITLIST));
            if (lex->type == l_ull)
            {
                f->arguments->tp = &stdunsignedlonglong;
                f->arguments->exp = intNode(en_c_ull, lex->value.i);
            }
            else
            {
                f->arguments->tp = &stdlongdouble;
                f->arguments->exp = intNode(en_c_ld, 0);
                f->arguments->exp->v.f = lex->value.f;
            }
            *exp = intNode(en_func, 0);
            (*exp)->v.func = f;
            *tp = sym1->tp;
            expression_arguments(nullptr, funcsp, tp, exp, false);
            return true;
        }
        else
        {
            // not found, look for parameter of type const char *
            hr = sym->tp->syms->table[0];
            while (hr)
            {
                TYPE* tpx;
                sym1 = hr->p;
                hr1 = sym1->tp->syms->table[0];
                sym2 = (SYMBOL*)hr1->p;
                tpx = sym2->tp;
                if (!hr1->next && ispointer(tpx))
                {
                    tpx = basetype(tpx)->btp;
                    if (isconst(tpx) && basetype(tpx)->type == bt_char)
                        break;
                }
                hr = hr->next;
            }
            if (hr)
            {
                FUNCTIONCALL* f = (FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
                STRING* data;
                int i;
                f->sp = sym1;
                f->functp = sym1->tp;
                f->fcall = varNode(en_pc, sym1);
                f->arguments = (INITLIST*)Alloc(sizeof(INITLIST));
                f->arguments->tp = &stdcharptr;
                f->arguments->tp->size = (strlen(lex->litaslit) + 1) * f->arguments->tp->btp->size;
                IncGlobalFlag();
                data = (STRING*)Alloc(sizeof(STRING));
                data->strtype = l_astr;
                data->size = 1;
                data->pointers = (SLCHAR**)Alloc(sizeof(SLCHAR*));
                data->pointers[0] = (SLCHAR*)Alloc(sizeof(SLCHAR));
                data->pointers[0]->count = strlen(lex->litaslit);
                data->pointers[0]->str = (LCHAR*)Alloc(sizeof(LCHAR) * (data->pointers[0]->count + 1));
                for (i = 0; i < data->pointers[0]->count; i++)
                    data->pointers[0]->str[i] = lex->litaslit[i];
                f->arguments->exp = stringlit(data);
                DecGlobalFlag();
                *exp = intNode(en_func, 0);
                (*exp)->v.func = f;
                *tp = sym1->tp;
                expression_arguments(nullptr, funcsp, tp, exp, false);
                return true;
            }
        }
    }
    errorstr(ERR_COULD_NOT_FIND_A_MATCH_FOR_LITERAL_SUFFIX, lex->suffix);
    return false;
}
static LEXEME* expression_atomic_func(LEXEME* lex, SYMBOL* funcsp, TYPE** tp, EXPRESSION** exp, int flags)
{
    enum e_kw kw = KW(lex);
    lex = getsym();
    if (needkw(&lex, openpa))
    {
        if (kw == kw_atomic_kill_dependency)
        {
            lex = expression_assign(lex, funcsp, nullptr, tp, exp, nullptr, flags);
            if (!*tp)
                error(ERR_EXPRESSION_SYNTAX);
            needkw(&lex, closepa);
        }
        else if (kw == kw_atomic_var_init)
        {
            lex = expression_assign(lex, funcsp, nullptr, tp, exp, nullptr, flags);
            if (!*tp)
                error(ERR_EXPRESSION_SYNTAX);
            if (MATCHKW(lex, comma))  // atomic_init
            {
                TYPE* tp1;
                EXPRESSION* exp1;
                lex = getsym();
                lex = expression_assign(lex, funcsp, nullptr, &tp1, &exp1, nullptr, flags);
                if (*tp && tp1)
                {
                    ATOMICDATA* d;
                    d = (ATOMICDATA*)Alloc(sizeof(ATOMICDATA));
                    if (!ispointer(tp1))
                    {
                        error(ERR_DEREF);
                        d->tp = *tp = &stdint;
                    }
                    else
                    {
                        TYPE* tp2 = *tp;
                        d->tp = *tp = basetype(tp1)->btp;
                        tp1 = tp2;
                    }
                    if (!comparetypes(*tp, tp1, false))
                    {
                        error(ERR_INCOMPATIBLE_TYPE_CONVERSION);
                    }
                    d->address = exp1;
                    d->value = *exp;
                    d->atomicOp = ao_init;
                    *exp = exprNode(en_atomic, nullptr, nullptr);
                    (*exp)->v.ad = d;
                }
                else
                {
                    error(ERR_EXPRESSION_SYNTAX);
                }
            }
            else  // ATOMIC_VAR_INIT
            {
                if (*exp)
                    (*exp)->atomicinit = true;
            }
            needkw(&lex, closepa);
        }
        else
        {
            TYPE *tpf = nullptr, *tpf1;
            ATOMICDATA* d;
            d = (ATOMICDATA*)Alloc(sizeof(ATOMICDATA));
            switch (kw)
            {
                case kw_atomic_flag_test_set:
                    lex = expression_assign(lex, funcsp, nullptr, &tpf, &d->flg, nullptr, flags);
                    if (tpf)
                    {
                        if (!ispointer(tpf))
                        {
                            error(ERR_DEREF);
                        }
                        deref(&stdint, &d->flg);
                        d->tp = *tp = &stdint;
                    }
                    if (needkw(&lex, comma))
                    {
                        lex = expression_assign(lex, funcsp, nullptr, &tpf, &d->memoryOrder1, nullptr, flags);
                    }
                    else
                    {
                        tpf = &stdint;
                        d->memoryOrder1 = intNode(en_c_i, mo_seq_cst);
                    }
                    d->atomicOp = ao_flag_set_test;
                    if (!d->memoryOrder2)
                        d->memoryOrder2 = d->memoryOrder1;
                    break;
                case kw_atomic_flag_clear:
                    lex = expression_assign(lex, funcsp, nullptr, &tpf, &d->flg, nullptr, flags);
                    if (tpf)
                    {
                        if (!ispointer(tpf))
                        {
                            error(ERR_DEREF);
                        }
                        deref(&stdint, &d->flg);
                        d->tp = *tp = &stdint;
                    }
                    if (needkw(&lex, comma))
                    {
                        lex = expression_assign(lex, funcsp, nullptr, &tpf, &d->memoryOrder1, nullptr, flags);
                    }
                    else
                    {
                        tpf = &stdint;
                        d->memoryOrder1 = intNode(en_c_i, mo_seq_cst);
                    }
                    d->atomicOp = ao_flag_clear;
                    if (!d->memoryOrder2)
                        d->memoryOrder2 = d->memoryOrder1;
                    *tp = &stdvoid;
                    break;
                case kw_atomic_fence:
                    lex = expression_assign(lex, funcsp, nullptr, &tpf, &d->memoryOrder1, nullptr, flags);
                    if (!d->memoryOrder2)
                        d->memoryOrder2 = d->memoryOrder1;
                    d->memoryOrder1 = exprNode(en_add, d->memoryOrder1, intNode(en_c_i, 0x80));
                    d->atomicOp = ao_fence;
                    *tp = &stdvoid;
                    break;
                case kw_atomic_load:
                    lex = expression_assign(lex, funcsp, nullptr, &tpf, &d->address, nullptr, flags);
                    if (tpf)
                    {
                        if (!ispointer(tpf))
                        {
                            error(ERR_DEREF);
                            d->tp = *tp = &stdint;
                        }
                        else
                        {
                            d->tp = *tp = basetype(tpf)->btp;
                        }
                    }
                    if (needkw(&lex, comma))
                    {
                        lex = expression_assign(lex, funcsp, nullptr, &tpf, &d->memoryOrder1, nullptr, flags);
                    }
                    else
                    {
                        tpf = &stdint;
                        d->memoryOrder1 = intNode(en_c_i, mo_seq_cst);
                    }
                    d->atomicOp = ao_load;
                    if (!d->memoryOrder2)
                        d->memoryOrder2 = d->memoryOrder1;
                    break;
                case kw_atomic_store:
                    lex = expression_assign(lex, funcsp, nullptr, &tpf, &d->address, nullptr, flags);
                    if (tpf)
                    {
                        if (!ispointer(tpf))
                        {
                            error(ERR_DEREF);
                            d->tp = *tp = &stdint;
                        }
                        else
                        {
                            d->tp = *tp = basetype(tpf)->btp;
                        }
                    }
                    if (needkw(&lex, comma))
                    {
                        lex = expression_assign(lex, funcsp, nullptr, &tpf, &d->value, nullptr, flags);
                        if (!comparetypes(tpf, *tp, false))
                        {
                            error(ERR_INCOMPATIBLE_TYPE_CONVERSION);
                        }
                    }
                    else
                    {
                        *tp = &stdint;
                        d->value = intNode(en_c_i, 0);
                    }
                    if (needkw(&lex, comma))
                    {
                        lex = expression_assign(lex, funcsp, nullptr, &tpf, &d->memoryOrder1, nullptr, flags);
                    }
                    else
                    {
                        tpf = &stdint;
                        d->memoryOrder1 = intNode(en_c_i, mo_seq_cst);
                    }
                    d->atomicOp = ao_store;
                    if (!d->memoryOrder2)
                        d->memoryOrder2 = d->memoryOrder1;
                    d->memoryOrder1 = exprNode(en_add, d->memoryOrder1, intNode(en_c_i, 0x80));
                    break;
                case kw_atomic_modify:
                    lex = expression_assign(lex, funcsp, nullptr, &tpf, &d->address, nullptr, flags);
                    if (tpf)
                    {
                        if (!ispointer(tpf))
                        {
                            error(ERR_DEREF);
                            d->tp = *tp = &stdint;
                        }
                        else
                        {
                            d->tp = *tp = basetype(tpf)->btp;
                        }
                    }
                    if (needkw(&lex, comma))
                    {
                        switch (KW(lex))
                        {
                            default:
                                d->third = intNode(en_c_i, asplus);
                                break;
                            case asplus:
                            case asminus:
                            case asor:
                            case asand:
                            case asxor:
                                if (isstructured(*tp))
                                    error(ERR_ILL_STRUCTURE_OPERATION);
                            case assign:
                                d->third = intNode(en_c_i, KW(lex));
                                break;
                        }
                        lex = getsym();
                    }

                    if (needkw(&lex, comma))
                    {
                        lex = expression_assign(lex, funcsp, nullptr, &tpf, &d->value, nullptr, flags);
                        if (!comparetypes(tpf, *tp, false))
                        {
                            error(ERR_INCOMPATIBLE_TYPE_CONVERSION);
                        }
                    }
                    else
                    {
                        *tp = &stdint;
                        d->value = intNode(en_c_i, 0);
                    }
                    if (needkw(&lex, comma))
                    {
                        lex = expression_assign(lex, funcsp, nullptr, &tpf, &d->memoryOrder1, nullptr, flags);
                    }
                    else
                    {
                        tpf = &stdint;
                        d->memoryOrder1 = intNode(en_c_i, mo_seq_cst);
                    }
                    d->atomicOp = ao_modify;
                    if (!d->memoryOrder2)
                        d->memoryOrder2 = d->memoryOrder1;
                    break;
                case kw_atomic_cmpswp:
                    lex = expression_assign(lex, funcsp, nullptr, &tpf, &d->address, nullptr, flags);
                    if (tpf)
                    {
                        if (!ispointer(tpf))
                        {
                            error(ERR_DEREF);
                            d->tp = *tp = &stdint;
                        }
                        else
                        {
                            d->tp = *tp = basetype(tpf)->btp;
                        }
                    }
                    if (needkw(&lex, comma))
                    {
                        lex = expression_assign(lex, funcsp, nullptr, &tpf1, &d->third, nullptr, flags);
                        if (!comparetypes(tpf, tpf1, false))
                        {
                            error(ERR_INCOMPATIBLE_TYPE_CONVERSION);
                        }
                    }
                    else
                    {
                        *tp = &stdint;
                        d->third = intNode(en_c_i, 0);
                    }

                    if (needkw(&lex, comma))
                    {
                        lex = expression_assign(lex, funcsp, nullptr, &tpf, &d->value, nullptr, flags);
                        if (!comparetypes(tpf, *tp, false))
                        {
                            error(ERR_INCOMPATIBLE_TYPE_CONVERSION);
                        }
                        if (!isstructured(*tp))
                            cast(*tp, &d->value);
                    }
                    else
                    {
                        *tp = &stdint;
                        d->value = intNode(en_c_i, 0);
                    }
                    if (needkw(&lex, comma))
                    {
                        lex = expression_assign(lex, funcsp, nullptr, &tpf, &d->memoryOrder1, nullptr, flags);
                    }
                    else
                    {
                        tpf = &stdint;
                        d->memoryOrder1 = intNode(en_c_i, mo_seq_cst);
                    }
                    if (needkw(&lex, comma))
                    {
                        lex = expression_assign(lex, funcsp, nullptr, &tpf1, &d->memoryOrder2, nullptr, flags);
                    }
                    else
                    {
                        tpf1 = &stdint;
                        d->memoryOrder2 = intNode(en_c_i, mo_seq_cst);
                    }
                    d->atomicOp = ao_cmpswp;
                    if (!d->memoryOrder2)
                        d->memoryOrder2 = d->memoryOrder1;
                    *tp = &stdint;
                    break;
                default:
                    break;
            }
            if (tpf && !isint(tpf))
            {
                error(ERR_NEED_INTEGER_TYPE);
                d->memoryOrder1 = *exp = intNode(en_c_i, mo_relaxed);
            }
            optimize_for_constants(&d->memoryOrder1);
            if (!needkw(&lex, closepa))
            {
                errskim(&lex, skim_closepa);
                skip(&lex, closepa);
            }
            *exp = exprNode(en_atomic, nullptr, nullptr);
            (*exp)->v.ad = d;
        }
    }
    else
    {
        *tp = &stdint;
        *exp = intNode(en_c_i, mo_relaxed);
        errskim(&lex, skim_closepa);
        skip(&lex, closepa);
    }
    return lex;
}
static LEXEME* expression___typeid(LEXEME* lex, SYMBOL* funcsp, TYPE** tp, EXPRESSION** exp)
{
    lex = getsym();
    if (needkw(&lex, openpa))
    {
        lex = expression_comma(lex, funcsp, nullptr, tp, exp, nullptr, _F_SIZEOF);
        if (!*tp)
        {
            error(ERR_TYPE_NAME_EXPECTED);
            *exp = intNode(en_c_i, 0);
        }
        else
        {
            int id;
            TYPE* tp1 = *tp;
            if (isref(tp1))
                *tp = basetype(tp1)->btp;  // DAL fixed
            switch (tp1->type)
            {

                case bt_bool:
                case bt_char:
                    id = -1;
                    break;
                case bt_short:
                    id = -2;
                    break;
                case bt_int:
                    id = -3;
                    break;
                case bt_long:
                    id = -4;
                    break;
                case bt_long_long:
                    id = -5;
                    break;
                case bt_unsigned_char:
                    id = 1;
                    break;
                case bt_unsigned_short:
                    id = 2;
                    break;
                case bt_unsigned:
                    id = 3;
                    break;
                case bt_unsigned_long:
                    id = 4;
                    break;
                case bt_unsigned_long_long:
                    id = 5;
                    break;
                case bt_float:
                    id = 7;
                    break;
                case bt_double:
                    id = 8;
                    break;
                case bt_long_double:
                    id = 10;
                    break;
                case bt_float_imaginary:
                    id = 15;
                    break;
                case bt_double_imaginary:
                    id = 16;
                    break;
                case bt_long_double_imaginary:
                    id = 17;
                    break;
                case bt_float_complex:
                    id = 20;
                    break;
                case bt_double_complex:
                    id = 21;
                    break;
                case bt_long_double_complex:
                    id = 22;
                    break;
                default:
                    id = 100000;
                    break;
            }
            *exp = intNode(en_c_i, id);
        }
        *tp = &stdint;
        needkw(&lex, closepa);
    }
    return lex;
}

static LEXEME* expression_primary(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    switch (lex ? lex->type : l_none)
    {
        case l_id:
            lex = variableName(lex, funcsp, atp, tp, exp, ismutable, flags);
            break;
        case l_kw:
            switch (KW(lex))
            {
                SYMBOL* sym;
                case openbr:
                    if (cparams.prm_cplusplus)
                        lex = expression_lambda(lex, funcsp, atp, tp, exp, flags);
                    break;
                case classsel:
                case kw_operator:
                case kw_decltype:
                    lex = variableName(lex, funcsp, atp, tp, exp, ismutable, flags);
                    break;
                case kw_nullptr:
                    *exp = intNode(en_nullptr, 0);
                    *tp = &stdnullpointer;
                    lex = getsym();
                    break;
                case kw_this:
                    if (lambdas)
                    {
                        lambda_capture(nullptr, cmThis, false);
                        if (lambdas->captureThis)
                        {
                            SYMBOL* ths = search("$this", lambdas->cls->tp->syms);
                            if (ths)
                            {
                                TYPE* t1 = (TYPE*)Alloc(sizeof(TYPE));
                                t1->type = bt_pointer;
                                t1->size = getSize(bt_pointer);
                                t1->btp = basetype(lambdas->lthis->tp)->btp;
                                t1->rootType = t1;
                                *tp = t1;
                                *exp = varNode(en_auto, (SYMBOL*)basetype(funcsp->tp)->syms->table[0]->p);  // this ptr
                                deref(&stdpointer, exp);
                                *exp = exprNode(en_add, *exp, intNode(en_c_i, ths->offset));
                                deref(&stdpointer, exp);
                            }
                            else
                            {
                                diag("expression_primary: missing lambda this");
                            }
                        }
                        else
                        {
                            *exp = intNode(en_c_i, 0);
                            *tp = &stdint;
                        }
                    }
                    else if (getStructureDeclaration() && funcsp && funcsp->parentClass)
                    {
                        getThisType(funcsp, tp);
                        *exp = varNode(en_auto, (SYMBOL*)basetype(funcsp->tp)->syms->table[0]->p);  // this ptr
                        deref(&stdpointer, exp);
                    }
                    else
                    {
                        *exp = intNode(en_c_i, 0);
                        *tp = &stdint;
                        error(ERR_THIS_MEMBER_FUNC);
                    }
                    lex = getsym();
                    break;
                case kw___I:
                    *exp = intNode(en_c_i, 0);
                    (*exp)->type = en_c_fi;
                    (*exp)->v.f = (LLONG_TYPE)1;
                    *tp = &stdfloatimaginary;
                    (*exp)->pragmas = preProcessor->GetStdPragmas();
                    lex = getsym();
                    break;
                case kw___offsetof:
                    lex = expression_offsetof(lex, funcsp, tp, exp, ismutable, flags);
                    break;
                case kw_true:
                    lex = getsym();
                    *exp = intNode(en_c_i, 1);
                    (*exp)->type = en_c_bool;
                    *tp = &stdbool;
                    break;
                case kw_false:
                    lex = getsym();
                    *exp = intNode(en_c_i, 0);
                    (*exp)->type = en_c_bool;
                    *tp = &stdbool;
                    return lex;
                case kw_alloca:
                    lex = expression_alloca(lex, funcsp, tp, exp, flags);
                    return lex;
                case kw__initblk:
                case kw__cpblk:
                    lex = expression_msilfunc(lex, funcsp, tp, exp, flags);
                    break;
                case openpa:
                    lex = getsym();
                    lex = expression_comma(lex, funcsp, nullptr, tp, exp, ismutable, flags & ~(_F_INTEMPLATEPARAMS | _F_SELECTOR));
                    if (!*tp)
                        error(ERR_EXPRESSION_SYNTAX);
                    needkw(&lex, closepa);
                    break;
                case kw___func__:
                    *tp = &std__func__;
                    if (!funcsp->__func__label)
                    {
                        LCHAR buf[256], *q = buf;
                        const char* p = funcsp->name;
                        STRING* string;
                        IncGlobalFlag();
                        string = (STRING*)Alloc(sizeof(STRING));
                        string->strtype = l_astr;
                        string->size = 1;
                        string->pointers = (SLCHAR**)Alloc(sizeof(SLCHAR*));
                        while (*p)
                            *q++ = *p++;
                        *q = 0;
                        string->pointers[0] = (SLCHAR*)Alloc(sizeof(SLCHAR));
                        string->pointers[0]->str = wlitlate(buf);
                        string->pointers[0]->count = q - buf;
                        *exp = stringlit(string);
                        funcsp->__func__label = string->label;
                        DecGlobalFlag();
                    }
                    else
                        *exp = intNode(en_labcon, funcsp->__func__label);
                    lex = getsym();
                    break;
                case kw__uuidof:
                    lex = getsym();
                    needkw(&lex, openpa);
                    if (ISID(lex))
                    {
                        sym = tsearch(lex->value.s.a);
                        lex = getsym();
                    }
                    else
                    {
                        TYPE* tp1;
                        EXPRESSION* exp1;
                        lex = expression_no_comma(lex, funcsp, nullptr, &tp1, &exp1, nullptr, 0);
                        if (tp1 && isstructured(tp1))
                        {
                            sym = basetype(tp1)->sp;
                        }
                        else
                        {
                            sym = nullptr;
                        }
                    }
                    needkw(&lex, closepa);
                    *exp = GetUUIDData(sym);
                    *tp = &stdpointer;
                    break;
                case kw___va_typeof__:
                    lex = getsym();
                    if (MATCHKW(lex, openpa))
                    {
                        lex = getsym();
                        if (startOfType(lex, false))
                        {
                            SYMBOL* sym;
                            lex = get_type_id(lex, tp, funcsp, sc_cast, false, true);
                            (*tp)->used = true;
                            needkw(&lex, closepa);
                            // don't enter in table, this is purely so we can cache the type info
                            sym = makeID(sc_auto, *tp, nullptr, AnonymousName());
                            sym->va_typeof = true;
                            *exp = varNode(en_auto, sym);
                            break;
                        }
                    }
                    error(ERR_TYPE_NAME_EXPECTED);
                    *exp = intNode(en_c_i, 0);
                    *tp = &stdint;
                    break;
                case kw___typeid:
                    lex = expression___typeid(lex, funcsp, tp, exp);
                    break;
                case kw_D0:
                case kw_D1:
                case kw_D2:
                case kw_D3:
                case kw_D4:
                case kw_D5:
                case kw_D6:
                case kw_D7:
                case kw_A0:
                case kw_A1:
                case kw_A2:
                case kw_A3:
                case kw_A4:
                case kw_A5:
                case kw_A6:
                case kw_A7:
                    *exp = intNode(en_c_i, 0);
                    *tp = &stdint;
                    lex = getsym();
                    break;
                case kw__NAN:
                    *exp = intNode(en_c_i, 0);
                    (*exp)->type = en_c_f;
                    (*exp)->v.f.SetNaN();
                    lex = getsym();
                    *tp = &stdfloat;
                    break;
                case kw__INF:
                    *exp = intNode(en_c_i, 0);
                    (*exp)->type = en_c_f;
                    (*exp)->v.f.SetInfinity(0);
                    lex = getsym();
                    *tp = &stdfloat;
                    break;
                case kw_generic:
                    lex = expression_generic(lex, funcsp, tp, exp, flags);
                    break;
                case kw_atomic_flag_test_set:
                case kw_atomic_flag_clear:
                case kw_atomic_fence:
                case kw_atomic_load:
                case kw_atomic_store:
                case kw_atomic_modify:
                case kw_atomic_cmpswp:
                case kw_atomic_kill_dependency:
                case kw_atomic_var_init:
                    lex = expression_atomic_func(lex, funcsp, tp, exp, flags);
                    break;
                case kw_typename:
                    lex = getsym();
                    *tp = nullptr;
                    lex = expression_func_type_cast(lex, funcsp, tp, exp, flags);
                    break;
                default:
                    /*					errorstr(ERR_UNEXPECTED_KEYWORD, lex->kw->name); */
                    *tp = nullptr;
                    *exp = intNode(en_c_i, 0);
                    lex = getsym();
                    break;
            }
            break;
        case l_i:
            *exp = intNode(en_c_i, lex->value.i);
            *tp = &stdint;
            lex = getsym();
            break;
        case l_ui:
            *exp = intNode(en_c_ui, lex->value.i);
            (*exp)->type = en_c_ui;
            *tp = &stdunsigned;
            lex = getsym();
            break;
        case l_l:
            *exp = intNode(en_c_l, lex->value.i);
            (*exp)->type = en_c_l;
            *tp = &stdlong;
            lex = getsym();
            break;
        case l_ul:
            *exp = intNode(en_c_ul, lex->value.i);
            (*exp)->type = en_c_ul;
            *tp = &stdunsignedlong;
            lex = getsym();
            break;
        case l_ll:
            *exp = intNode(en_c_ll, lex->value.i);
            (*exp)->type = en_c_ll;
            *tp = &stdlonglong;
            lex = getsym();
            break;
        case l_ull:
            if (!lex->suffix || !getSuffixedNumber(lex, funcsp, tp, exp))
            {
                *exp = intNode(en_c_ull, lex->value.i);
                (*exp)->type = en_c_ull;
                *tp = &stdunsignedlonglong;
            }
            lex = getsym();
            break;
        case l_f:
            *exp = intNode(en_c_f, 0);
            (*exp)->v.f = lex->value.f;
            (*exp)->pragmas = preProcessor->GetStdPragmas();
            *tp = &stdfloat;
            lex = getsym();
            break;
        case l_d:
            *exp = intNode(en_c_d, 0);
            (*exp)->v.f = lex->value.f;
            (*exp)->pragmas = preProcessor->GetStdPragmas();
            *tp = &stddouble;
            lex = getsym();
            break;
        case l_ld:
            if (!lex->suffix || !getSuffixedNumber(lex, funcsp, tp, exp))
            {
                *exp = intNode(en_c_ld, 0);
                (*exp)->v.f = lex->value.f;
                (*exp)->pragmas = preProcessor->GetStdPragmas();
                *tp = &stdlongdouble;
            }
            lex = getsym();
            break;
        case l_astr:
        case l_wstr:
        case l_ustr:
        case l_Ustr:
        case l_msilstr:
            lex = expression_string(lex, funcsp, tp, exp);
            break;
        case l_wchr:
            *exp = intNode(en_c_wc, lex->value.i);
            (*exp)->type = en_c_wc;
            *tp = &stdwidechar;
            if (lex->suffix)
                getSuffixedChar(lex, funcsp, tp, exp);
            lex = getsym();
            break;
        case l_achr:
            *exp = intNode(en_c_c, lex->value.i);
            (*exp)->type = en_c_c;
            *tp = &stdchar;
            if (lex->suffix)
                getSuffixedChar(lex, funcsp, tp, exp);
            lex = getsym();
            break;
        case l_uchr:
            *exp = intNode(en_c_u16, lex->value.i);
            (*exp)->type = en_c_u16;
            *tp = stdchar16tptr.btp;
            if (lex->suffix)
                getSuffixedChar(lex, funcsp, tp, exp);
            lex = getsym();
            break;
        case l_Uchr:
            *exp = intNode(en_c_u32, lex->value.i);
            (*exp)->type = en_c_u32;
            *tp = stdchar32tptr.btp;
            if (lex->suffix)
                getSuffixedChar(lex, funcsp, tp, exp);
            lex = getsym();
            break;
        default:
            *tp = nullptr;
            *exp = intNode(en_c_i, 0);
            break;
    }
    return lex;
}
static EXPRESSION* nodeSizeof(TYPE* tp, EXPRESSION* exp, int flags)
{
    EXPRESSION *exp_in = exp;
    tp = PerformDeferredInitialization(basetype(tp), nullptr);
    if (isstructured(tp))
        tp = basetype(tp)->sp->tp;
    if (isref(tp))
        tp = basetype(tp)->btp;
    tp = basetype(tp);
    if (exp)
    {
        while (castvalue(exp))
            exp = exp->left;
        if (lvalue(exp) && exp->left->type == en_bits)
            error(ERR_SIZEOF_BITFIELD);
    }
    if (cparams.prm_cplusplus && tp->type == bt_enum && !tp->fixed)
        error(ERR_SIZEOF_UNFIXED_ENUMERATION);
    if (isfunction(tp))
        error(ERR_SIZEOF_NO_FUNCTION);
    if (cparams.prm_cplusplus && tp->size == 0 && !templateNestingCount)
        errortype(ERR_UNSIZED_TYPE, tp, tp); /* second will be ignored in this case */
    /* this tosses exp...  sizeof expressions don't get evaluated at run time */
    /* unless they are size of a vla... */
    if (tp->vla)
    {
        exp = tp->esize;
        tp = basetype(tp)->btp;

        while (tp->type == bt_pointer)
        {
            exp = exprNode(en_mul, exp, tp->esize);
            tp = basetype(tp)->btp;
        }
        exp = exprNode(en_mul, exp, intNode(en_c_i, tp->size));
    }
    else
    {
        exp = nullptr;
        if (isstructured(tp))
        {
            if (basetype(tp)->size == 0)
                errorsym(ERR_UNSIZED_TYPE,basetype(tp)->sp);
            if (basetype(tp)->syms)
            {
                SYMLIST* hr = basetype(tp)->syms->table[0];
                SYMBOL* cache = nullptr;
                TYPE* tpx;
                while (hr)
                {
                    SYMBOL* sym = hr->p;
                    if (ismemberdata(sym))
                        cache = sym;
                    hr = hr->next;
                }
                if (cache)
                {
                    tpx = basetype(cache->tp);
                    if (tpx->size == 0) /* if the last element of a structure is unsized */
                                        /* sizeof doesn't add the size of the padding element */
                        exp = intNode(en_c_i, cache->offset);
                }
            }
        }
    }
    if (!exp)
    {
        // array which is an argument has different sizeof requirements
        if ((flags & _F_SIZEOF) && tp->type == bt_pointer && tp->array && exp_in && exp_in->type == en_l_p && exp_in->left->type == en_auto && exp_in->left->v.sp->storage_class == sc_parameter)
            exp = intNode(en_c_i, getSize(bt_pointer));
        else
            exp = intNode(en_c_i, tp->size);
    }
    return exp;
}
static int widelen(LCHAR* s)
{
    int rv = 0;
    while (*s++)
        rv++;
    return rv;
}
static LEXEME* expression_sizeof(LEXEME* lex, SYMBOL* funcsp, TYPE** tp, EXPRESSION** exp)
{
    int paren = false;
    *exp = nullptr;
    lex = getsym();
    if (cparams.prm_cplusplus && MATCHKW(lex, ellipse))   {
        lex = getsym();
        if (MATCHKW(lex, openpa))
        {
            paren = true;
            lex = getsym();
        }
        if (!ISID(lex))
        {
            error(ERR_SIZEOFELLIPSE_NEEDS_TEMPLATE_PACK);
            errskim(&lex, skim_closepa);
        }
        else
        {
            EXPRESSION* exp1 = nullptr;
            lex = variableName(lex, funcsp, nullptr, tp, &exp1, nullptr, _F_PACKABLE | _F_SIZEOF);
            if (!exp1 || !exp1->v.sp->tp->templateParam->p->packed)
            {
                //                error(ERR_SIZEOFELLIPSE_NEEDS_TEMPLATE_PACK);
                *tp = &stdunsigned;
                *exp = intNode(en_c_i, 1);
            }
            else if (templateNestingCount)
            {
                *exp = intNode(en_sizeofellipse, 0);
                (*exp)->v.templateParam = (*tp)->templateParam;
                *tp = &stdunsigned;
            }
            else if (!(*tp)->templateParam->p->packed)
            {
                *tp = &stdunsigned;
                *exp = intNode(en_c_i, 1);
            }
            else
            {
                int n = 0;
                TEMPLATEPARAMLIST* templateParam = (*tp)->templateParam->p->byPack.pack;
                while (templateParam)
                {
                    n++;
                    templateParam = templateParam->next;
                }
                *tp = &stdunsigned;
                *exp = intNode(en_c_i, n);
            }
        }
        if (paren)
            needkw(&lex, closepa);
    }
    else
    {
        if (MATCHKW(lex, openpa))
        {
            paren = true;
            lex = getsym();
        }
        if (!paren || !startOfType(lex, false))
        {
            if (paren)
            {
                lex = expression_comma(lex, funcsp, nullptr, tp, exp, nullptr, _F_SIZEOF);
                needkw(&lex, closepa);
            }
            else
            {
                lex = expression_unary(lex, funcsp, nullptr, tp, exp, nullptr, _F_SIZEOF);
            }
            ResolveTemplateVariable(tp, exp, &stdint, nullptr);
            if (!*tp)
            {
                *exp = intNode(en_c_i, 1);
                error(ERR_EXPRESSION_SYNTAX);
            }
            else
            {
                *exp = nodeSizeof(*tp, *exp, _F_SIZEOF);
            }
        }
        else
        {
            LEXEME* prev = lex;
            lex = get_type_id(lex, tp, funcsp, sc_cast, cparams.prm_cplusplus, true);
            if (cparams.prm_cplusplus && MATCHKW(lex, openpa))
            {
                lex = prevsym(prev);
                lex = expression_func_type_cast(lex, funcsp, tp, exp, 0);
            }
            if (paren)
                needkw(&lex, closepa);
            if (MATCHKW(lex, ellipse))
            {
                lex = getsym();
                if ((*tp)->type == bt_templateparam)
                {
                    if (!(*tp)->templateParam->p->packed)
                    {
                        error(ERR_PACK_SPECIFIER_REQUIRES_PACKED_TEMPLATE_PARAMETER);
                    }
                    else if (packIndex != -1)
                    {
                        TEMPLATEPARAMLIST* tpl = (*tp)->templateParam->p->byPack.pack;
                        int i;
                        for (i = 0; tpl && i < packIndex; i++)
                            tpl = tpl->next;
                        if (tpl)
                            *tp = tpl->p->byClass.val;
                    }
                }
                else
                {
                    error(ERR_PACK_SPECIFIER_NOT_ALLOWED_HERE);
                }
            }
            if (!*tp)
            {
                *exp = intNode(en_c_i, 1);
            }
            else
            {
                checkauto(*tp, ERR_AUTO_NOT_ALLOWED);
                *exp = nodeSizeof(*tp, *exp, _F_SIZEOF);
            }
        }

        *tp = &stdint; /* other compilers use sizeof as a signed value so we do too... */
    }
    return lex;
}
static LEXEME* expression_alignof(LEXEME* lex, SYMBOL* funcsp, TYPE** tp, EXPRESSION** exp)
{
    lex = getsym();
    if (needkw(&lex, openpa))
    {
        lex = get_type_id(lex, tp, funcsp, sc_cast, false, true);
        needkw(&lex, closepa);
        if (MATCHKW(lex, ellipse))
        {
            lex = getsym();
            if ((*tp)->type == bt_templateparam)
            {
                if (!(*tp)->templateParam->p->packed)
                {
                    error(ERR_PACK_SPECIFIER_REQUIRES_PACKED_TEMPLATE_PARAMETER);
                }
                else if (packIndex != -1)
                {
                    TEMPLATEPARAMLIST* tpl = (*tp)->templateParam->p->byPack.pack;
                    int i;
                    for (i = 0; tpl && i < packIndex; i++)
                        tpl = tpl->next;
                    if (tpl)
                        *tp = tpl->p->byClass.val;
                }
            }
            else
            {
                error(ERR_PACK_SPECIFIER_NOT_ALLOWED_HERE);
            }
        }
        if (!*tp)
        {
            *exp = intNode(en_c_i, 1);
        }
        else
        {
            TYPE* itp = *tp;
            checkauto(itp, ERR_AUTO_NOT_ALLOWED);
            if (isref(itp))
                itp = (basetype(itp)->btp);
            while (itp->array)
                itp = itp->btp;
            *exp = intNode(en_c_i, getAlign(sc_global, *tp));
        }
    }
    *tp = &stdint;
    return lex;
}
static LEXEME* expression_ampersand(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, int flags)
{
    lex = getsym();
    lex = expression_cast(lex, funcsp, atp, tp, exp, nullptr, (flags) | _F_AMPERSAND);
    if (*tp)
    {
        TYPE *btp, *tp1;
        EXPRESSION *exp1 = *exp, *symRef;
        while (exp1->type == en_void && exp1->right)
            exp1 = exp1->right;
        if (exp1->type == en_void)
            exp1 = exp1->left;
        symRef = chosenAssembler->msil ? GetSymRef(exp1) : nullptr;
        btp = basetype(*tp);
        LookupSingleAggregate(btp, &exp1);  // DAL
        if ((cparams.prm_cplusplus || chosenAssembler->msil) &&
            insertOperatorFunc(ovcl_unary_any, and_unary, funcsp, tp, exp, nullptr, nullptr, nullptr, flags))
        {
            return lex;
        }
        else if (isvoid(*tp) || ismsil(*tp))
            error(ERR_NOT_AN_ALLOWED_TYPE);
        else if (btp->hasbits)
            error(ERR_CANNOT_TAKE_ADDRESS_OF_BIT_FIELD);
        else if (btp->msil)
            error(ERR_MANAGED_OBJECT_NO_ADDRESS);
        else if (symRef && symRef->v.sp->attribs.inheritable.linkage2 == lk_property)
            errorsym(ERR_CANNOT_TAKE_ADDRESS_OF_PROPERTY, symRef->v.sp);
        else if (inreg(*exp, true))
            error(ERR_CANNOT_TAKE_ADDRESS_OF_REGISTER);
        else if ((!ispointer(btp) || !(btp)->array) && !isstructured(btp) && !isfunction(btp) && btp->type != bt_aggregate &&
                 (exp1)->type != en_memberptr)
        {
            if ((exp1)->type != en_const && exp1->type != en_assign)
                if (!lvalue(exp1))
                    if (cparams.prm_ansi || !castvalue(exp1))
                        error(ERR_MUST_TAKE_ADDRESS_OF_MEMORY_LOCATION);
        }
        else
            switch ((exp1)->type)
            {
                case en_pc:
                case en_auto:
                case en_global:
                case en_absolute:
                case en_threadlocal:
                {
                    SYMBOL* sym = exp1->v.sp;
                    if (sym->isConstructor || sym->isDestructor)
                        error(ERR_CANNOT_TAKE_ADDRESS_OF_CONSTRUCTOR_OR_DESTRUCTOR);
                    break;
                }
                default:
                    break;
            }
        if ((*exp)->type == en_const)
        {
            /* if a variable propagated silently to an inline constant
             * this will restore it as a static variable in the const section
             * when we take its address
             * but the constant will still be used inline when possible
             */
            SYMBOL* sym = (*exp)->v.sp;
            if (!sym->indecltable)
            {
                if (!sym->label)
                {
                    TYPE *tp, *tpb, *tpn, **tpnp = &tpn;
                    bool done = false;
                    SYMBOL* spold = sym;
                    sym->label = nextLabel++;
                    IncGlobalFlag();
                    sym = clonesym(sym);
                    spold->indecltable = true;
                    tp = sym->tp;
                    tpb = basetype(tp);
                    do
                    {
                        *tpnp = (TYPE*)Alloc(sizeof(TYPE));
                        **tpnp = *tp;
                        UpdateRootTypes(*tpnp);
                        tpnp = &(*tpnp)->btp;
                        if (tp != tpb)
                            tp = tp->btp;
                        else
                            done = true;
                    } while (!done);
                    sym->tp = tpn;
                    sym->storage_class = sc_static;
                    insertInitSym(sym);
                    DecGlobalFlag();
                }
                else
                {
                    IncGlobalFlag();
                    insertInitSym(sym);
                    DecGlobalFlag();
                }
                if (!sym->parent)
                    sym->parent = funcsp;  // this promotion of a global to local is necessary to not make it linkable
                *exp = varNode(en_global, sym);
            }
            else
            {
                *exp = varNode(en_global, sym);
            }
            tp1 = (TYPE*)Alloc(sizeof(TYPE));
            tp1->type = bt_pointer;
            tp1->size = getSize(bt_pointer);
            tp1->btp = *tp;
            tp1->rootType = tp1;
            *tp = tp1;
        }
        else if (!isfunction(*tp) && (*tp)->type != bt_aggregate)
        {
            EXPRESSION *expasn = nullptr, **exp2;
            while (castvalue(exp1))
                exp1 = (exp1)->left;
            if (exp1->type == en_assign)
            {
                expasn = exp1;
                exp1 = exp1->left;
                while (castvalue(exp1))
                    exp1 = (exp1)->left;
            }
            if (!lvalue(exp1))
            {
                if (!btp->array && !btp->vla && !isstructured(btp) && basetype(btp)->type != bt_memberptr &&
                    basetype(btp)->type != bt_templateparam)
                    error(ERR_LVALUE);
            }
            else if (!isstructured(btp) && exp1->type != en_l_ref)
                exp1 = (exp1)->left;

            switch ((exp1)->type)
            {
                case en_pc:
                case en_auto:
                case en_global:
                case en_absolute:
                case en_threadlocal:
                    (exp1)->v.sp->addressTaken = true;
                    break;
                default:
                    break;
            }
            exp2 = exp;
            while ((*exp2)->type == en_void && (*exp2)->right)
                exp2 = &(*exp2)->right;
            if ((*exp2)->type == en_void)
                exp2 = &(*exp2)->left;
            if (basetype(btp)->type != bt_memberptr)
            {
                tp1 = (TYPE*)Alloc(sizeof(TYPE));
                tp1->type = bt_pointer;
                tp1->size = getSize(bt_pointer);
                tp1->btp = *tp;
                tp1->rootType = tp1;
                *tp = tp1;
                if (expasn)
                    *exp2 = exprNode(en_void, expasn, exp1);
                else
                    *exp2 = exp1;
            }
        }
    }
    return lex;
}
static LEXEME* expression_deref(LEXEME* lex, SYMBOL* funcsp, TYPE** tp, EXPRESSION** exp, int flags)
{
    lex = getsym();
    lex = expression_cast(lex, funcsp, nullptr, tp, exp, nullptr, flags);
    if ((cparams.prm_cplusplus || chosenAssembler->msil) &&
        insertOperatorFunc(ovcl_unary_pointer, star_unary, funcsp, tp, exp, nullptr, nullptr, nullptr, flags))
    {
        return lex;
    }
    if (*tp && (isvoid(*tp) || (*tp)->type == bt_aggregate || ismsil(*tp)))
    {
        error(ERR_NOT_AN_ALLOWED_TYPE);
    }
    else if (*tp && basetype(*tp)->type == bt_memberptr)
        error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
    if (*tp && !isfunction(*tp) && !isfuncptr(*tp))
    {
        TYPE *btp = basetype(*tp), *btp2, *btp3;
        ;
        if (!ispointer(*tp))
        {
            if (!templateNestingCount || instantiatingTemplate)
                error(ERR_DEREF);
            deref(&stdpointer, exp);
        }
        else
        {
            btp3 = btp->btp;
            btp2 = basetype(btp->btp);
            if (btp2->type == bt_void)
            {
                error(ERR_DEREF);
                deref(&stdpointer, exp);
            }
            else if (isstructured(btp2))
            {
                *tp = btp3;
                *tp = PerformDeferredInitialization(*tp, funcsp);
            }
            else
            {
                if (!isfunction(btp2))
                {
                    *tp = btp3;
                    deref(*tp, exp);
                    if (isvolatile(btp3))
                        (*exp)->isvolatile = true;
                    if (isrestrict(btp3))
                        (*exp)->isrestrict = true;
                    if (isatomic(btp3))
                    {
                        (*exp)->isatomic = true;
                        if (needsAtomicLockFromType(btp3))
                            (*exp)->lockOffset = basetype(btp3)->size;
                    }
                }
            }
        }
    }
    tagNonConst(funcsp, *tp);
    return lex;
}
static LEXEME* expression_postfix(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    TYPE* oldType;
    bool done = false;
    bool localMutable = false;
    if (KWTYPE(lex, TT_POINTERQUAL | TT_LINKAGE | TT_BASETYPE | TT_STORAGE_CLASS))
    {
        lex = expression_func_type_cast(lex, funcsp, tp, exp, flags);
    }
    else
        switch (KW(lex))
        {
            case kw_dynamic_cast:
                oldType = nullptr;
                lex = GetCastInfo(lex, funcsp, tp, &oldType, exp, (flags & _F_PACKABLE));
                if (*tp && !doDynamicCast(tp, oldType, exp, funcsp))
                    if (!typeHasTemplateArg(*tp))
                        errortype(ERR_CANNOT_CAST_TYPE, oldType, *tp);
                if (isref(*tp))
                    *tp = basetype(*tp)->btp;
                break;
            case kw_static_cast:
                oldType = nullptr;
                lex = GetCastInfo(lex, funcsp, tp, &oldType, exp, (flags & _F_PACKABLE));
                if (*tp && !doStaticCast(tp, oldType, exp, funcsp, true))
                    if (!typeHasTemplateArg(*tp))
                        errortype(ERR_CANNOT_CAST_TYPE, oldType, *tp);
                if (isref(*tp))
                    *tp = basetype(*tp)->btp;
                break;
            case kw_const_cast:
                oldType = nullptr;
                lex = GetCastInfo(lex, funcsp, tp, &oldType, exp, (flags & _F_PACKABLE));
                if (*tp && !doConstCast(tp, oldType, exp, funcsp))
                    if (!typeHasTemplateArg(*tp))
                        errortype(ERR_CANNOT_CAST_TYPE, oldType, *tp);
                if (isref(*tp))
                    *tp = basetype(*tp)->btp;
                break;
            case kw_reinterpret_cast:
                oldType = nullptr;
                lex = GetCastInfo(lex, funcsp, tp, &oldType, exp, (flags & _F_PACKABLE));
                if (*tp && !doReinterpretCast(tp, oldType, exp, funcsp, true))
                    if (!typeHasTemplateArg(*tp))
                        errortype(ERR_CANNOT_CAST_TYPE, oldType, *tp);
                if (isref(*tp))
                    *tp = basetype(*tp)->btp;
                break;
            case kw_typeid:
                lex = expression_typeid(lex, funcsp, tp, exp, (flags & _F_PACKABLE));
                break;
            default:
                lex = expression_primary(lex, funcsp, atp, tp, exp, &localMutable, flags);
                if (ismutable)
                    *ismutable = localMutable;
                break;
        }
    if (!*tp)
        return lex;
    while (!done && !parsingPreprocessorConstant && lex)
    {
        enum e_kw kw;
        switch (KW(lex))
        {
            case openbr:
                lex = expression_bracket(lex, funcsp, tp, exp, flags);
                break;
            case openpa:
                lex = expression_arguments(lex, funcsp, tp, exp, flags);
                break;
            case pointsto:
            case dot:
                lex = expression_member(lex, funcsp, tp, exp, ismutable, flags);
                break;
            case autoinc:
            case autodec:

                kw = KW(lex);
                lex = getsym();
                if ((cparams.prm_cplusplus || chosenAssembler->msil) &&
                    insertOperatorFunc(ovcl_unary_postfix, kw, funcsp, tp, exp, nullptr, nullptr, nullptr, flags))
                {
                }
                else
                {
                    castToArithmetic(false, tp, exp, kw, nullptr, true);
                    if (isconstraw(*tp, true) && !localMutable)
                        error(ERR_CANNOT_MODIFY_CONST_OBJECT);
                    else if (isstructured(*tp))
                        error(ERR_ILL_STRUCTURE_OPERATION);
                    else if (!lvalue(*exp) && basetype(*tp)->type != bt_templateparam)
                    {
                        error(ERR_LVALUE);
                    }
                    else
                    {
                        EXPRESSION *exp3 = nullptr, *exp1 = nullptr;
                        if ((*exp)->left->type == en_func || (*exp)->left->type == en_thisref)
                        {
                            EXPRESSION* exp2 = anonymousVar(sc_auto, *tp);
                            deref(&stdpointer, &exp2);
                            exp3 = exprNode(en_assign, exp2, (*exp)->left);
                            deref(*tp, &exp2);
                            *exp = exp2;
                        }
                        if (basetype(*tp)->type == bt_pointer)
                        {
                            TYPE* btp = basetype(*tp)->btp;
                            if (basetype(btp)->type == bt_void)
                            {
                                if (cparams.prm_cplusplus)
                                    error(ERR_ARITHMETIC_WITH_VOID_STAR);
                                exp1 = nodeSizeof(&stdchar, *exp);
                            }
                            else
                            {
                                exp1 = nodeSizeof(btp, *exp);
                            }
                        }
                        else
                        {
                            if (isvoid(*tp) || (*tp)->type == bt_aggregate || ismsil(*tp))
                                error(ERR_NOT_AN_ALLOWED_TYPE);
                            if (basetype(*tp)->scoped)
                                error(ERR_SCOPED_TYPE_MISMATCH);
                            if (basetype(*tp)->type == bt_memberptr)
                                error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                            exp1 = intNode(en_c_i, 1);
                        }
                        if (basetype(*tp)->type == bt_bool)
                        {
                            /* autoinc of a bool sets it true.  autodec not allowed in C++
                             * these aren't spelled out in the C99 standard, we are
                             * doing the normal thing here...
                             */
                            if (kw == autodec)
                            {
                                if (cparams.prm_cplusplus)
                                    error(ERR_CANNOT_USE_bool_HERE);
                                *exp = exprNode(en_assign, *exp, intNode(en_c_bool, 0));
                            }
                            else
                            {
                                *exp = exprNode(en_assign, *exp, intNode(en_c_bool, 1));
                            }
                        }
                        else
                        {
                            cast(*tp, &exp1);
                            *exp = exprNode(kw == autoinc ? en_autoinc : en_autodec, *exp, exp1);
                        }
                        if (exp3)
                            *exp = exprNode(en_void, exp3, *exp);
                        while (lvalue(exp1))
                            exp1 = exp1->left;
                        if (exp1->type == en_auto)
                            exp1->v.sp->altered = true;
                    }
                }
                break;
            default:
                done = true;
                break;
        }
    }
    /*	if (isfunction(*tp))
            error(ERR_FUNCTION_ARGUMENTS_EXPECTED);
    */
    return lex;
}
LEXEME* expression_unary(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    bool localMutable = false;
    enum e_kw kw = KW(lex);
    /* note some of the math ops are speced to do integer promotions
     * if being stored, the proposed place to store them is not known, so e.g.
     * a ~ on a unsigned char would promote to int to be evaluated,
     * the int would be further sign
     * extended to a long long though through the assignment promotion
     */
    switch (kw)
    {
        case plus:
            lex = getsym();
            lex = expression_cast(lex, funcsp, atp, tp, exp, nullptr, flags);
            if (*tp)
            {
                if ((cparams.prm_cplusplus || chosenAssembler->msil) &&
                    insertOperatorFunc(ovcl_unary_numeric, plus_unary, funcsp, tp, exp, nullptr, nullptr, nullptr, flags))
                {
                }
                else
                {
                    LookupSingleAggregate(*tp, exp);
                    castToArithmetic(false, tp, exp, kw, nullptr, true);
                    if (isstructured(*tp))
                        error(ERR_ILL_STRUCTURE_OPERATION);
                    else if (isvoid(*tp) || ismsil(*tp))
                        error(ERR_NOT_AN_ALLOWED_TYPE);
                    else if (basetype(*tp)->type == bt_memberptr)
                        error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                    else if (basetype(*tp)->scoped)
                        error(ERR_SCOPED_TYPE_MISMATCH);
                    else if (ispointer(*tp))
                        error(ERR_ILL_POINTER_OPERATION);
                    else if (atp && basetype(atp)->type < bt_int)
                    {
                        cast(atp, exp);
                        *tp = atp;
                    }
                    else if (basetype(*tp)->type < bt_int)
                    {
                        cast(&stdint, exp);
                        *tp = &stdint;
                    }
                }
            }
            break;
        case minus:
            lex = getsym();
            lex = expression_cast(lex, funcsp, atp, tp, exp, nullptr, flags);
            if (!*tp)
            {
                error(ERR_EXPRESSION_SYNTAX);
                *tp = &stdint;
            }
            LookupSingleAggregate(*tp, exp);
            if (*tp)
            {
                if ((cparams.prm_cplusplus || chosenAssembler->msil) &&
                    insertOperatorFunc(ovcl_unary_numeric, minus_unary, funcsp, tp, exp, nullptr, nullptr, nullptr, flags))
                {
                }
                else
                {
                    castToArithmetic(false, tp, exp, kw, nullptr, true);
                    *exp = exprNode(en_uminus, *exp, nullptr);
                    if (isstructured(*tp))
                        error(ERR_ILL_STRUCTURE_OPERATION);
                    else if (isvoid(*tp) || ismsil(*tp))
                        error(ERR_NOT_AN_ALLOWED_TYPE);
                    else if (basetype(*tp)->type == bt_memberptr)
                        error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                    else if (basetype(*tp)->scoped)
                        error(ERR_SCOPED_TYPE_MISMATCH);
                    else if (ispointer(*tp))
                        error(ERR_ILL_POINTER_OPERATION);
                    else if (atp && basetype(atp)->type < bt_int)
                    {
                        cast(atp, exp);
                        *tp = atp;
                    }
                    else if (basetype(*tp)->type < bt_int)
                    {
                        cast(&stdint, exp);
                        *tp = &stdint;
                    }
                }
            }
            break;
        case star:
            lex = expression_deref(lex, funcsp, tp, exp, flags);
            break;
        case andx:
            lex = expression_ampersand(lex, funcsp, atp, tp, exp, flags);
            break;
        case notx:
            lex = getsym();
            lex = expression_cast(lex, funcsp, atp, tp, exp, nullptr, flags);
            if (!*tp)
            {
                error(ERR_EXPRESSION_SYNTAX);
                *tp = &stdint;
            }
            LookupSingleAggregate(*tp, exp);
            if (*tp)
            {
                if ((cparams.prm_cplusplus || chosenAssembler->msil) &&
                    insertOperatorFunc(ovcl_unary_numericptr, notx, funcsp, tp, exp, nullptr, nullptr, nullptr, flags))
                {
                }
                else
                {
                    castToArithmetic(false, tp, exp, kw, nullptr, false);
                    if (isstructured(*tp))
                        error(ERR_ILL_STRUCTURE_OPERATION);
                    else if (isvoid(*tp) || ismsil(*tp))
                        error(ERR_NOT_AN_ALLOWED_TYPE);
                    else if (basetype(*tp)->scoped)
                        error(ERR_SCOPED_TYPE_MISMATCH);
                    /*
                else
                    if (basetype(*tp)->type < bt_int)
                    {
                        cast(&stdint, exp);
                        *tp = &stdint;
                    }
                    */
                    if ((*tp)->type == bt_memberptr)
                    {
                        *exp = exprNode(en_mp_as_bool, *exp, nullptr);
                        (*exp)->size = (*tp)->size;
                        *exp = exprNode(en_not, *exp, nullptr);
                    }
                    else
                    {
                        *exp = exprNode(en_not, *exp, nullptr);
                    }
                }
                if (cparams.prm_cplusplus)
                    *tp = &stdbool;
                else
                    *tp = &stdint;
            }
            break;
        case complx:
            lex = getsym();
            lex = expression_cast(lex, funcsp, atp, tp, exp, nullptr, flags);
            if (!*tp)
            {
                error(ERR_EXPRESSION_SYNTAX);
                *tp = &stdint;
            }
            LookupSingleAggregate(*tp, exp);
            if (*tp)
            {
                if ((cparams.prm_cplusplus || chosenAssembler->msil) &&
                    insertOperatorFunc(ovcl_unary_int, complx, funcsp, tp, exp, nullptr, nullptr, nullptr, flags))
                {
                }
                else
                {
                    castToArithmetic(true, tp, exp, kw, nullptr, true);
                    *exp = exprNode(en_compl, *exp, nullptr);
                    if (isstructured(*tp))
                        error(ERR_ILL_STRUCTURE_OPERATION);
                    else if (iscomplex(*tp))
                        error(ERR_ILL_USE_OF_COMPLEX);
                    else if (isfloat(*tp) || isimaginary(*tp))
                        error(ERR_ILL_USE_OF_FLOATING);
                    else if (ispointer(*tp))
                        error(ERR_ILL_POINTER_OPERATION);
                    else if (isvoid(*tp) || ismsil(*tp))
                        error(ERR_NOT_AN_ALLOWED_TYPE);
                    else if (basetype(*tp)->type == bt_memberptr)
                        error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                    else if (basetype(*tp)->scoped)
                        error(ERR_SCOPED_TYPE_MISMATCH);
                    else if (atp && basetype(atp)->type < bt_int)
                    {
                        cast(atp, exp);
                        *tp = atp;
                    }
                    else if (basetype(*tp)->type < bt_int)
                    {
                        cast(&stdint, exp);
                        *tp = &stdint;
                    }
                    else
                    {
                        cast(basetype(*tp), exp);
                    }
                }
            }
            break;
        case autoinc:
        case autodec:
            lex = getsym();
            lex = expression_cast(lex, funcsp, atp, tp, exp, &localMutable, flags);
            if (*tp)
            {
                if ((cparams.prm_cplusplus || chosenAssembler->msil) &&
                    insertOperatorFunc(ovcl_unary_prefix, kw, funcsp, tp, exp, nullptr, nullptr, nullptr, flags))
                {
                }
                else
                {
                    castToArithmetic(false, tp, exp, kw, nullptr, true);
                    if (isstructured(*tp))
                        error(ERR_ILL_STRUCTURE_OPERATION);
                    else if (iscomplex(*tp))
                        error(ERR_ILL_USE_OF_COMPLEX);
                    else if (isconstraw(*tp, true) && !localMutable)
                        error(ERR_CANNOT_MODIFY_CONST_OBJECT);
                    else if (isvoid(*tp) || (*tp)->type == bt_aggregate || ismsil(*tp))
                        error(ERR_NOT_AN_ALLOWED_TYPE);
                    else if (basetype(*tp)->type == bt_memberptr)
                        error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                    else if (basetype(*tp)->scoped)
                        error(ERR_SCOPED_TYPE_MISMATCH);
                    else if (!lvalue(*exp))
                    {
                        if ((*exp)->type != en_templateparam && basetype(*tp)->type != bt_templateparam &&
                            basetype(*tp)->type != bt_templateselector)
                            error(ERR_LVALUE);
                    }
                    else
                    {
                        EXPRESSION* exp3 = nullptr;
                        if ((*exp)->left->type == en_func || (*exp)->left->type == en_thisref)
                        {
                            EXPRESSION* exp2 = anonymousVar(sc_auto, *tp);
                            deref(&stdpointer, &exp2);
                            exp3 = exprNode(en_assign, exp2, (*exp)->left);
                            deref(*tp, &exp2);
                            *exp = exp2;
                        }
                        if (ispointer(*tp))
                        {
                            TYPE* tpx;
                            if (basetype(basetype(*tp)->btp)->type == bt_void)
                            {
                                if (cparams.prm_cplusplus)
                                    error(ERR_ARITHMETIC_WITH_VOID_STAR);
                                tpx = &stdchar;
                            }
                            else
                            {
                                tpx = basetype(*tp)->btp;
                            }
                            *exp =
                                exprNode(en_assign, *exp, exprNode(kw == autoinc ? en_add : en_sub, *exp, nodeSizeof(tpx, *exp)));
                        }
                        else if (kw == autoinc && basetype(*tp)->type == bt_bool)
                        {
                            *exp = exprNode(en_assign, *exp, intNode(en_c_i, 1));  // set to true as per C++
                        }
                        else if (kw == autodec && basetype(*tp)->type == bt_bool)
                        {
                            if (cparams.prm_cplusplus)
                                error(ERR_CANNOT_USE_bool_HERE);

                            *exp = exprNode(en_assign, *exp, intNode(en_c_i, 0));  // c++ doesn't allow it, set it to true for C.
                        }
                        else
                        {
                            EXPRESSION *dest = *exp, *exp1 = intNode(en_c_i, 1);
                            *exp = RemoveAutoIncDec(*exp);
                            cast(*tp, &exp1);
                            *exp = exprNode(en_assign, dest, exprNode(kw == autoinc ? en_add : en_sub, *exp, exp1));
                        }
                        if (exp3)
                            *exp = exprNode(en_void, exp3, *exp);
                    }
                }
            }
            break;
        case kw_sizeof:
            lex = expression_sizeof(lex, funcsp, tp, exp);
            break;
        case kw_alignof:
            lex = expression_alignof(lex, funcsp, tp, exp);
            break;
        case kw_new:
            lex = expression_new(lex, funcsp, tp, exp, false, flags);
            break;
        case kw_delete:
            lex = expression_delete(lex, funcsp, tp, exp, false, flags);
            break;
        case kw_noexcept:
            lex = expression_noexcept(lex, funcsp, tp, exp);
            break;
        case classsel:
        {
            LEXEME* placeholder = lex;
            lex = getsym();
            switch (KW(lex))
            {
                case kw_new:
                    return expression_new(lex, funcsp, tp, exp, true, flags);
                case kw_delete:
                    return expression_delete(lex, funcsp, tp, exp, true, flags);
                default:
                    break;
            }
            lex = prevsym(placeholder);
        }
            // fallthrough
        default:
            lex = expression_postfix(lex, funcsp, atp, tp, exp, ismutable, flags);
            break;
    }
    return lex;
}
LEXEME* expression_cast(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    TYPE* throwaway = nullptr;
    if (MATCHKW(lex, openpa))
    {
        bool loadedAttribs = false;
        attributes oldAttribs;
        LEXEME* start = lex;
        lex = getsym();
        if (MATCHKW(lex, kw__attribute))
        {
            loadedAttribs = true;
            oldAttribs = basisAttribs;

            basisAttribs = { 0 };
            ParseAttributeSpecifiers(&lex, funcsp, true);

        }
        if (startOfType(lex, false))
        {
            if (!cparams.prm_cplusplus || resolveToDeclaration(lex))
            {
                bool done = false;
                lex = get_type_id(lex, tp, funcsp, sc_cast, false, true);
                if (!*tp)
                {
                    error(ERR_TYPE_NAME_EXPECTED);
                    *tp = &stdint;
                    *exp = intNode(en_c_i, 0);
                }
                else
                {
                    (*tp)->used = true;
                    needkw(&lex, closepa);
                    checkauto(*tp, ERR_AUTO_NOT_ALLOWED);
                    if (MATCHKW(lex, begin))
                    {
                        INITIALIZER* init = nullptr;
                        SYMBOL* sym = nullptr;
                        if (!cparams.prm_c99 && !cparams.prm_cplusplus)
                            error(ERR_C99_STYLE_INITIALIZATION_USED);
                        if (cparams.prm_cplusplus)
                        {
                            sym = makeID(sc_auto, *tp, nullptr, AnonymousName());
                            insert(sym, localNameSpace->valueData->syms);
                        }
                        lex = initType(lex, funcsp, 0, sc_auto, &init, nullptr, *tp, sym, false, flags);
                        *exp = convertInitToExpression(*tp, nullptr, nullptr, funcsp, init, *exp, false);
                        while (!done && lex)
                        {
                            enum e_kw kw;
                            switch (KW(lex))
                            {
                                case openbr:
                                    lex = expression_bracket(lex, funcsp, tp, exp, flags);
                                    break;
                                case openpa:
                                    lex = expression_arguments(lex, funcsp, tp, exp, flags);
                                    break;
                                case pointsto:
                                case dot:
                                    lex = expression_member(lex, funcsp, tp, exp, ismutable, flags);
                                    break;
                                case autoinc:
                                case autodec:

                                    kw = KW(lex);
                                    lex = getsym();
                                    if ((cparams.prm_cplusplus || chosenAssembler->msil) &&
                                        insertOperatorFunc(ovcl_unary_postfix, kw, funcsp, tp, exp, nullptr, nullptr, nullptr,
                                                           flags))
                                    {
                                    }
                                    else
                                    {
                                        castToArithmetic(false, tp, exp, kw, nullptr, true);
                                        if (isstructured(*tp))
                                            error(ERR_ILL_STRUCTURE_OPERATION);
                                        else if (!lvalue(*exp) && basetype(*tp)->type != bt_templateparam)
                                            error(ERR_LVALUE);
                                        else
                                        {
                                            EXPRESSION* exp1 = nullptr;
                                            if (basetype(*tp)->type == bt_pointer)
                                            {
                                                TYPE* btp = basetype(*tp)->btp;
                                                exp1 = nodeSizeof(btp, *exp);
                                            }
                                            else
                                            {
                                                if (isvoid(*tp) || (*tp)->type == bt_aggregate || ismsil(*tp))
                                                    error(ERR_NOT_AN_ALLOWED_TYPE);
                                                if (basetype(*tp)->scoped)
                                                    error(ERR_SCOPED_TYPE_MISMATCH);
                                                if (basetype(*tp)->type == bt_memberptr)
                                                    error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                                                exp1 = intNode(en_c_i, 1);
                                            }
                                            if (basetype(*tp)->type == bt_bool)
                                            {
                                                /* autoinc of a bool sets it true.  autodec not allowed
                                                 * these aren't spelled out in the C99 standard, we are
                                                 * following the C++ standard here
                                                 */
                                                if (kw == autodec)
                                                    error(ERR_CANNOT_USE_bool_HERE);
                                                *exp = exprNode(en_assign, *exp, intNode(en_c_bool, 1));
                                            }
                                            else
                                            {
                                                cast(*tp, &exp1);
                                                *exp = exprNode(kw == autoinc ? en_autoinc : en_autodec, *exp, exp1);
                                            }
                                            while (lvalue(exp1))
                                                exp1 = exp1->left;
                                            if (exp1->type == en_auto)
                                                exp1->v.sp->altered = true;
                                        }
                                    }
                                    break;
                                default:
                                    done = true;
                                    break;
                            }
                        }
                    }
                    else
                    {
                        LEXEME *lastSym = lex;
                        lex = expression_cast(lex, funcsp, nullptr, &throwaway, exp, ismutable, flags);
                        //                if ((*exp)->type == en_func)
                        //                    *exp = (*exp)->v.func->fcall;
                        if (throwaway)
                        {
                            if ((*exp)->type == en_pc || ((*exp)->type == en_func && !(*exp)->v.func->ascall))
                                thunkForImportTable(exp);
                            if (basetype(*tp)->type == bt___string)
                            {
                                if ((*exp)->type == en_labcon && (*exp)->string)
                                    (*exp)->type = en_c_string;
                                else if (basetype(throwaway)->type != bt___string)
                                    *exp = exprNode(en_x_string, *exp, nullptr);
                            }
                            else if (basetype(*tp)->type == bt___object)
                            {
                                if (basetype(throwaway)->type != bt___object)
                                    if (!isstructured(throwaway) && (!isarray(throwaway) || !basetype(throwaway)->msil))
                                        *exp = exprNode(en_x_object, *exp, nullptr);
                            }
                            else if ((isvoid(throwaway) && !isvoid(*tp)) || ismsil(*tp))
                            {
                                error(ERR_NOT_AN_ALLOWED_TYPE);
                            }
                            else if (!cparams.prm_cplusplus &&
                                     ((isstructured(throwaway) && !isvoid(*tp)) || basetype(throwaway)->type == bt_memberptr ||
                                      basetype(*tp)->type == bt_memberptr) &&
                                     !comparetypes(throwaway, *tp, true))
                            {
                                error(ERR_INCOMPATIBLE_TYPE_CONVERSION);
                            }
                            else if (cparams.prm_cplusplus)
                            {
                                if (!doStaticCast(tp, throwaway, exp, funcsp, false) &&
                                    !doReinterpretCast(tp, throwaway, exp, funcsp, false))
                                {
                                    cast(*tp, exp);
                                }
                            }
                            else if (!isstructured(*tp) && (!isarray(*tp) || !basetype(*tp)->msil))
                            {
                                cast(*tp, exp);
                            }
                        }
                        else
                        {
                            *exp = intNode(en_c_i, 0);
                            *tp = &stdint;
                            prevsym(lastSym);
                            error(ERR_EXPRESSION_SYNTAX);
                        }
                    }
                }
            }
            else  // expression in parenthesis
            {
                lex = prevsym(start);
                lex = expression_unary(lex, funcsp, atp, tp, exp, ismutable, flags);
            }
        }
        else
        {
            lex = prevsym(start);
            lex = expression_unary(lex, funcsp, atp, tp, exp, ismutable, flags);
        }
        if (loadedAttribs)
            basisAttribs = oldAttribs;
    }
    else
    {
        lex = expression_unary(lex, funcsp, atp, tp, exp, ismutable, flags);
    }
    return lex;
}
static LEXEME* expression_pm(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    lex = expression_cast(lex, funcsp, atp, tp, exp, ismutable, flags);
    if (*tp == nullptr)
        return lex;
    while (MATCHKW(lex, dotstar) || MATCHKW(lex, pointstar))
    {
        bool points = false;
        enum e_kw kw = KW(lex);
        TYPE* tp1 = nullptr;
        EXPRESSION* exp1 = nullptr;
        lex = getsym();
        lex = expression_cast(lex, funcsp, nullptr, &tp1, &exp1, nullptr, flags);
        if (cparams.prm_cplusplus && kw == pointstar &&
            insertOperatorFunc(ovcl_binary_any, pointstar, funcsp, tp, exp, tp1, exp1, nullptr, flags))
        {
            continue;
        }
        if (kw == pointstar)
        {
            points = true;
            if (ispointer(*tp))
            {
                *tp = basetype(*tp);
                *tp = (*tp)->btp;
                if (!isstructured(*tp))
                {
                    errorstr(ERR_POINTER_TO_STRUCTURE_EXPECTED, lex->kw->name);
                }
            }
            else
                errorstr(ERR_POINTER_TO_STRUCTURE_EXPECTED, lex->kw->name);
        }
        else if (!isstructured(*tp))
        {
            errorstr(ERR_STRUCTURED_TYPE_EXPECTED, lex->kw->name);
        }
        if (isfunction(tp1) && isstructured(*tp))
        {
            exp1 = getMemberPtr(basetype(tp1)->sp, basetype(*tp)->sp, &tp1, funcsp);
        }
        if (basetype(tp1)->type != bt_memberptr)
        {
            error(ERR_INCOMPATIBLE_TYPE_CONVERSION);
        }
        else
        {
            if (isstructured(*tp) && basetype(tp1)->type == bt_memberptr)
            {
                if ((*tp)->sp != basetype(tp1)->sp && (*tp)->sp->mainsym != basetype(tp1)->sp &&
                    (*tp)->sp != basetype(tp1)->sp->mainsym)
                {
                    if (classRefCount(basetype(tp1)->sp, (*tp)->sp) != 1)
                    {
                        errorsym2(ERR_NOT_UNAMBIGUOUS_BASE, basetype(tp1)->sp, (*tp)->sp);
                    }
                    else
                    {
                        *exp = baseClassOffset(basetype(tp1)->sp, (*tp)->sp, *exp);
                    }
                }
                if (exp1->type == en_memberptr)
                {
                    int lab = dumpMemberPtr(exp1->v.sp, *tp, true);
                    exp1 = intNode(en_labcon, lab);
                }
                if (isfunction(basetype(tp1)->btp))
                {
                    FUNCTIONCALL* funcparams = (FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
                    if ((*tp)->sp->vbaseEntries)
                    {
                        EXPRESSION* ec = exprNode(en_add, exp1, intNode(en_c_i, getSize(bt_pointer) + getSize(bt_int)));
                        EXPRESSION* ec1;
                        deref(&stdint, &ec);
                        ec1 = exprNode(en_sub, ec, intNode(en_c_i, 1));
                        deref(&stdint, &ec1);
                        ec = exprNode(en_cond, ec, exprNode(en_void, ec1, intNode(en_c_i, 0)));
                        *exp = exprNode(en_add, *exp, ec);
                    }
                    else
                    {
                        EXPRESSION* ec = exprNode(en_add, exp1, intNode(en_c_i, getSize(bt_pointer)));
                        deref(&stdpointer, &ec);
                        *exp = exprNode(en_add, *exp, ec);
                    }
                    funcparams->sp = tp1->btp->sp;
                    funcparams->fcall = exp1;
                    deref(&stdpointer, &funcparams->fcall);
                    funcparams->thisptr = *exp;
                    funcparams->thistp = (TYPE*)Alloc(sizeof(TYPE));
                    funcparams->thistp->size = getSize(bt_pointer);
                    funcparams->thistp->type = bt_pointer;
                    funcparams->thistp->btp = *tp;
                    funcparams->thistp->rootType = funcparams->thistp;
                    *exp = varNode(en_func, nullptr);
                    (*exp)->v.func = funcparams;
                    *tp = basetype(tp1);
                }
                else
                {
                    if ((*tp)->sp->vbaseEntries)
                    {
                        EXPRESSION* ec = exprNode(en_add, exp1, intNode(en_c_i, getSize(bt_int)));
                        EXPRESSION* ec1;
                        deref(&stdint, &ec);
                        ec1 = exprNode(en_add, ec, intNode(en_c_i, -1));
                        ec1 = exprNode(en_add, *exp, ec1);
                        deref(&stdint, &ec1);
                        *exp = exprNode(en_cond, ec, exprNode(en_void, ec1, *exp));
                    }
                    deref(&stdint, &exp1);
                    *exp = exprNode(en_add, *exp, exp1);
                    *exp = exprNode(en_add, *exp, intNode(en_c_i, -1));
                    if (!isstructured(basetype(tp1)->btp))
                    {
                        deref(basetype(tp1)->btp, exp);
                    }
                    *tp = basetype(tp1)->btp;
                }
            }
        }
    }
    return lex;
}
static LEXEME* expression_times(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    lex = expression_pm(lex, funcsp, atp, tp, exp, ismutable, flags);
    if (*tp == nullptr)
        return lex;
    while (MATCHKW(lex, star) || MATCHKW(lex, divide) || MATCHKW(lex, mod))
    {
        enum e_kw kw = KW(lex);
        enum e_node type;
        TYPE* tp1 = nullptr;
        EXPRESSION* exp1 = nullptr;
        lex = getsym();
        lex = expression_pm(lex, funcsp, nullptr, &tp1, &exp1, nullptr, flags);
        if (!tp1)
        {
            *tp = nullptr;
            return lex;
        }
        ResolveTemplateVariable(tp, exp, tp1, atp);
        ResolveTemplateVariable(&tp1, &exp1, *tp, atp);
        if (cparams.prm_cplusplus &&
            insertOperatorFunc(kw == mod ? ovcl_binary_int : ovcl_binary_numeric, kw, funcsp, tp, exp, tp1, exp1, nullptr, flags))
        {
        }
        else
        {
            castToArithmetic(kw == mod, tp, exp, kw, tp1, true);
            castToArithmetic(kw == mod, &tp1, &exp1, (enum e_kw) - 1, *tp, true);
            LookupSingleAggregate(*tp, exp);
            LookupSingleAggregate(tp1, &exp1);
            if (isstructured(*tp) || isstructured(tp1))
                error(ERR_ILL_STRUCTURE_OPERATION);
            else if (isvoid(*tp) || isvoid(tp1) || ismsil(*tp) || ismsil(tp1))
                error(ERR_NOT_AN_ALLOWED_TYPE);
            else if (basetype(*tp)->type == bt_memberptr || basetype(tp1)->type == bt_memberptr)
                error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
            else if (basetype(*tp)->scoped || basetype(tp1)->scoped)
                error(ERR_SCOPED_TYPE_MISMATCH);
            else if (ispointer(*tp) || ispointer(tp1))
                error(ERR_ILL_POINTER_OPERATION);
            else
            {
                int m1 = -1, m2 = -1;
                if (isimaginary(*tp) && isimaginary(tp1))
                {
                    if (kw == star)
                        *exp = exprNode(en_uminus, *exp, nullptr);
                    m1 = (*tp)->type - bt_float_imaginary;
                    m2 = tp1->type - bt_float_imaginary;
                    m1 = m1 > m2 ? m1 : m2;
                    switch (m1)
                    {
                        case 0:
                            *tp = &stdfloat;
                            tp1 = &stdfloatimaginary;
                            break;
                        case 1:
                            *tp = &stddouble;
                            tp1 = &stddoubleimaginary;
                            break;
                        case 2:
                            *tp = &stdlongdouble;
                            tp1 = &stdlongdoubleimaginary;
                            break;
                    }
                    cast(tp1, exp);
                    cast(tp1, &exp1);
                }
                else
                {
                    if (isimaginary(*tp) && (isfloat(tp1) || isint(tp1)))
                    {
                        m1 = (*tp)->type - bt_float_imaginary;
                        m2 = isfloat(tp1) ? tp1->type - bt_float : m1;
                    }
                    else if ((isfloat(*tp) || isint(*tp)) && isimaginary(tp1))
                    {
                        m1 = tp1->type - bt_float_imaginary;
                        m2 = isfloat(*tp) ? (*tp)->type - bt_float : m1;
                    }
                    if (m1 >= 0)
                    {
                        bool b = isimaginary(*tp);
                        m1 = m1 > m2 ? m1 : m2;
                        switch (m1)
                        {
                            case 0:
                                *tp = &stdfloatimaginary;
                                tp1 = &stdfloat;
                                break;
                            case 1:
                                *tp = &stddoubleimaginary;
                                tp1 = &stddouble;
                                break;
                            default:
                                *tp = &stdlongdoubleimaginary;
                                tp1 = &stdlongdouble;
                                break;
                        }
                        if (b)
                        {
                            cast(*tp, exp);
                            cast(tp1, &exp1);
                        }
                        else
                        {
                            cast(tp1, exp);
                            cast(*tp, &exp1);
                        }
                    }
                    else
                    {
                        *tp = destSize(*tp, tp1, exp, &exp1, false, nullptr);
                    }
                }
                switch (kw)
                {
                    case star:
                        type = en_mul;
                        break;
                    case divide:
                        type = isunsigned(*tp) ? en_udiv : en_div;
                        break;
                    case mod:
                        type = isunsigned(*tp) ? en_umod : en_mod;
                        break;
                    default:
                        break;
                }
                *exp = exprNode(type, *exp, exp1);
            }
        }
    }
    return lex;
}

static LEXEME* expression_add(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    /* fixme add vlas */
    lex = expression_times(lex, funcsp, atp, tp, exp, ismutable, flags);
    if (*tp == nullptr)
        return lex;
    while (MATCHKW(lex, plus) || MATCHKW(lex, minus))
    {
        bool msil = false;
        enum e_kw kw = KW(lex);
        TYPE* tp1 = nullptr;
        EXPRESSION* exp1 = nullptr;
        lex = getsym();
        lex = expression_times(lex, funcsp, atp, &tp1, &exp1, nullptr, flags);
        if (!tp1)
        {
            *tp = nullptr;
            return lex;
        }
        ResolveTemplateVariable(tp, exp, tp1, atp);
        ResolveTemplateVariable(&tp1, &exp1, *tp, atp);
        if ((cparams.prm_cplusplus || chosenAssembler->msil) &&
            insertOperatorFunc(ovcl_binary_numericptr, kw, funcsp, tp, exp, tp1, exp1, nullptr, flags))
        {
            continue;
        }
        else
        {
            if (!ispointer(*tp) && !ispointer(tp1))
            {
                castToArithmetic(false, tp, exp, kw, tp1, true);
                castToArithmetic(false, &tp1, &exp1, (enum e_kw) - 1, *tp, true);
            }
            LookupSingleAggregate(*tp, exp);
            LookupSingleAggregate(tp1, &exp1);
            if (chosenAssembler->msil && chosenAssembler->msil->allowExtensions && kw == plus &&
                (basetype(*tp)->type == bt___string || basetype(tp1)->type == bt___string ||
                 (atp && basetype(atp)->type == bt___string)))
            {
                msil = true;
                if ((*exp)->type == en_labcon && (*exp)->string)
                    (*exp)->type = en_c_string;
                else if (!ismsil(*tp))
                    *exp = exprNode(en_x_object, *exp, nullptr);
                if (exp1->type == en_labcon && exp1->string)
                    exp1->type = en_c_string;
                else if (!ismsil(tp1))
                    exp1 = exprNode(en_x_object, exp1, nullptr);
            }
            else if (kw == plus && ispointer(*tp) && ispointer(tp1))
                error(ERR_ILL_POINTER_ADDITION);
            else if (isvoid(*tp) || isvoid(tp1) || ismsil(*tp) || ismsil(tp1))
                error(ERR_NOT_AN_ALLOWED_TYPE);
            else if (basetype(*tp)->type == bt_memberptr || basetype(tp1)->type == bt_memberptr)
                error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
            else if (basetype(*tp)->scoped || basetype(tp1)->scoped)
                error(ERR_SCOPED_TYPE_MISMATCH);
            else if (kw != plus && !ispointer(*tp) && ispointer(tp1))
                error(ERR_ILL_POINTER_SUBTRACTION);
            else if (isstructured(*tp) || isstructured(tp1))
                error(ERR_ILL_STRUCTURE_OPERATION);
            else if (ispointer(*tp))
            {
                if (isarray(*tp) && (*tp)->msil)
                    error(ERR_MANAGED_OBJECT_NO_ADDRESS);
                else if (ispointer(tp1) && !comparetypes(*tp, tp1, true) && !comparetypes(tp1, *tp, true))
                    error(ERR_NONPORTABLE_POINTER_CONVERSION);
                else if (iscomplex(tp1))
                    error(ERR_ILL_USE_OF_COMPLEX);
                else if (isfloat(tp1) || isimaginary(tp1))
                    error(ERR_ILL_USE_OF_FLOATING);
            }
            else if (ispointer(tp1))
            {
                if (iscomplex(*tp))
                    error(ERR_ILL_USE_OF_COMPLEX);
                else if (isfloat(*tp) || isimaginary(*tp))
                    error(ERR_ILL_USE_OF_FLOATING);
                else if (isarray(tp1) && (tp1)->msil)
                    error(ERR_MANAGED_OBJECT_NO_ADDRESS);
            }
        }
        if (msil)
        {
            // MSIL back end will take care of figuring out what function to call
            // to perform the concatenation
            *exp = exprNode(en_add, *exp, exp1);
            *tp = &std__string;
        }
        else if (ispointer(*tp))
        {
            EXPRESSION* ns;
            if (basetype(basetype(*tp)->btp)->type == bt_void)
            {
                if (cparams.prm_cplusplus)
                    error(ERR_ARITHMETIC_WITH_VOID_STAR);
                ns = nodeSizeof(&stdchar, *exp);
            }
            else
            {
                ns = nodeSizeof(basetype(*tp)->btp, *exp);
            }
            if (ispointer(tp1))
            {
                *exp = exprNode(en_sub, *exp, exp1);
                *exp = exprNode(en_arraydiv, *exp, ns);
                *tp = &stdint; /* ptrdiff_t */
            }
            else
            {
                /*				*tp = tp1 = destSize(*tp, tp1, exp, &exp1, false, nullptr); */
                if (basetype(tp1)->type < bt_int)
                    cast(&stdint, &exp1);
                exp1 = exprNode(en_umul, exp1, ns);
                *exp = exprNode(kw == plus ? en_add : en_sub, *exp, exp1);
            }
        }
        else if (ispointer(tp1))
        {
            EXPRESSION* ns;
            if (basetype(basetype(tp1)->btp)->type == bt_void)
            {
                if (cparams.prm_cplusplus)
                    error(ERR_ARITHMETIC_WITH_VOID_STAR);
                ns = nodeSizeof(&stdchar, *exp);
            }
            else
            {
                ns = nodeSizeof(basetype(tp1)->btp, *exp);
            }
            /*			*tp = tp1 = destSize(*tp, tp1, exp, &exp1, false, nullptr); */
            if (basetype(*tp)->type < bt_int)
                cast(&stdint, exp);
            *exp = exprNode(en_umul, *exp, ns);
            *exp = exprNode(en_add, *exp, exp1);
            *tp = tp1;
        }
        else
        {
            int m1 = -1, m2 = -1;
            if (isimaginary(*tp) && (isfloat(tp1) || isint(tp1)))
            {
                m1 = (*tp)->type - bt_float_imaginary;
                m2 = isfloat(tp1) ? tp1->type - bt_float : m1;
            }
            else if ((isfloat(*tp) || isint(*tp)) && isimaginary(tp1))
            {
                m1 = tp1->type - bt_float_imaginary;
                m2 = isfloat(*tp) ? (*tp)->type - bt_float : m1;
            }
            if (m1 >= 0)
            {
                m1 = m1 > m2 ? m1 : m2;
                TYPE *tpa, *tpb;
                switch (m1)
                {
                    case 0:
                        *tp = &stdfloatcomplex;
                        tpa = &stdfloat;
                        tpb = &stdfloatimaginary;
                        break;
                    case 1:
                        *tp = &stddoublecomplex;
                        tpa = &stddouble;
                        tpb = &stddoubleimaginary;
                        break;
                    default:
                        *tp = &stdlongdoublecomplex;
                        tpa = &stdlongdouble;
                        tpb = &stdlongdoubleimaginary;
                        break;
                }
                if (isimaginary(tp1))
                {
                    cast(tpa, exp);
                    cast(tpb, &exp1);
                }
                else
                {
                    cast(tpb, exp);
                    cast(tpa, &exp1);
                }
                if (kw != plus)
                    exp1 = exprNode(en_uminus, exp1, nullptr);
                *exp = exprNode(en_add, *exp, exp1);
                cast(*tp, exp);
            }
            else
            {
                *tp = destSize(*tp, tp1, exp, &exp1, true, atp);
                *exp = exprNode(kw == plus ? en_add : en_sub, *exp, exp1);
            }
        }
    }
    return lex;
}
static LEXEME* expression_shift(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    lex = expression_add(lex, funcsp, atp, tp, exp, ismutable, flags);
    if (*tp == nullptr)
        return lex;
    while ((MATCHKW(lex, rightshift) && !(flags & _F_INTEMPLATEPARAMS)) || MATCHKW(lex, leftshift))
    {
        TYPE* tp1 = nullptr;
        EXPRESSION* exp1 = nullptr;
        enum e_node type;
        enum e_kw kw = KW(lex);
        lex = getsym();
        lex = expression_add(lex, funcsp, nullptr, &tp1, &exp1, nullptr, flags);
        if (!tp1)
        {
            *tp = nullptr;
            return lex;
        }
        ResolveTemplateVariable(tp, exp, tp1, atp);
        ResolveTemplateVariable(&tp1, &exp1, *tp, atp);
        LookupSingleAggregate(*tp, exp);
        LookupSingleAggregate(tp1, &exp1);
        if ((cparams.prm_cplusplus || chosenAssembler->msil) &&
            insertOperatorFunc(ovcl_binary_int, kw, funcsp, tp, exp, tp1, exp1, nullptr, flags))
        {
        }
        else
        {
            castToArithmetic(true, tp, exp, kw, tp1, true);
            castToArithmetic(true, &tp1, &exp1, (enum e_kw) - 1, *tp, true);
            if (isstructured(*tp) || isstructured(tp1))
                error(ERR_ILL_STRUCTURE_OPERATION);
            else if (isvoid(*tp) || isvoid(tp1) || ismsil(*tp) || ismsil(tp1))
                error(ERR_NOT_AN_ALLOWED_TYPE);
            else if (basetype(*tp)->type == bt_memberptr || basetype(tp1)->type == bt_memberptr)
                error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
            else if (basetype(*tp)->scoped || basetype(tp1)->scoped)
                error(ERR_SCOPED_TYPE_MISMATCH);
            else if (ispointer(*tp) || ispointer(tp1))
                error(ERR_ILL_POINTER_OPERATION);
            else if (isfloat(*tp) || isfloat(tp1) || isimaginary(*tp) || isimaginary(tp1))
                error(ERR_ILL_USE_OF_FLOATING);
            else if (iscomplex(*tp) || iscomplex(tp1))
                error(ERR_ILL_USE_OF_COMPLEX);
            else
            {
                if (kw == rightshift)
                    if (isunsigned(*tp))
                        type = en_ursh;
                    else
                        type = en_rsh;
                else
                    type = en_lsh;
                if (basetype(*tp)->type < bt_int)
                {
                    *tp = &stdint;
                    cast(*tp, exp);
                }
                *exp = exprNode(type, *exp, exprNode(en_shiftby, exp1, 0));
            }
        }
    }
    return lex;
}
static LEXEME* expression_inequality(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable,
                                     int flags)
{
    bool done = false;
    lex = expression_shift(lex, funcsp, atp, tp, exp, ismutable, flags);
    if (*tp == nullptr)
        return lex;
    while (!done && lex)
    {
        enum e_kw kw = KW(lex);
        enum e_node type;
        const char* opname;
        TYPE* tp1 = nullptr;
        EXPRESSION* exp1 = nullptr;
        switch (kw)
        {
            case gt:
                opname = lex->kw->name;
                done = (flags & _F_INTEMPLATEPARAMS);
                break;
            case geq:
            case lt:
            case leq:
                opname = lex->kw->name;
                break;
            default:
                done = true;
                break;
        }
        if (!done)
        {
            lex = getsym();

            lex = expression_shift(lex, funcsp, nullptr, &tp1, &exp1, nullptr, flags);
            if (!tp1)
            {
                *tp = nullptr;
                return lex;
            }
            ResolveTemplateVariable(tp, exp, tp1, atp);
            ResolveTemplateVariable(&tp1, &exp1, *tp, atp);
            if ((cparams.prm_cplusplus || chosenAssembler->msil) &&
                insertOperatorFunc(ovcl_binary_numericptr, kw, funcsp, tp, exp, tp1, exp1, nullptr, flags))
            {
            }
            else
            {
                checkscope(*tp, tp1);
                castToArithmetic(false, tp, exp, kw, tp1, true);
                castToArithmetic(false, &tp1, &exp1, (enum e_kw) - 1, *tp, true);
                LookupSingleAggregate(*tp, exp);
                LookupSingleAggregate(tp1, &exp1);

                if (cparams.prm_cplusplus)
                {
                    SYMBOL* funcsp = nullptr;
                    if ((ispointer(*tp) || basetype(*tp)->type == bt_memberptr) && tp1->type == bt_aggregate)
                    {
                        if (tp1->syms->table[0]->next)
                            errorstr(ERR_OVERLOADED_FUNCTION_AMBIGUOUS, ((SYMBOL*)tp1->syms->table[0]->p)->name);
                        exp1 = varNode(en_pc, tp1->syms->table[0]->p);
                        tp1 = ((SYMBOL*)tp1->syms->table[0]->p)->tp;
                    }
                    else if ((ispointer(tp1) || basetype(tp1)->type == bt_memberptr) && (*tp)->type == bt_aggregate)
                    {
                        if ((*tp)->syms->table[0]->next)
                            errorstr(ERR_OVERLOADED_FUNCTION_AMBIGUOUS, ((SYMBOL*)(*tp)->syms->table[0]->p)->name);
                        (*exp) = varNode(en_pc, (*tp)->syms->table[0]->p);
                        (*tp) = ((SYMBOL*)(*tp)->syms->table[0]->p)->tp;
                    }
                    if (funcsp)
                        GENREF(funcsp);
                }
                if ((*exp)->type == en_pc || ((*exp)->type == en_func && !(*exp)->v.func->ascall))
                    thunkForImportTable(exp);
                if (exp1->type == en_pc || (exp1->type == en_func && !exp1->v.func->ascall))
                    thunkForImportTable(&exp1);
                if (isstructured(*tp) || isstructured(tp1))
                    error(ERR_ILL_STRUCTURE_OPERATION);
                else if (isvoid(*tp) || isvoid(tp1) || ismsil(*tp) || ismsil(tp1))
                    error(ERR_NOT_AN_ALLOWED_TYPE);
                else if (basetype(*tp)->type == bt_memberptr || basetype(tp1)->type == bt_memberptr)
                    error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                else if (ispointer(*tp))
                {
                    if (isintconst(exp1))
                    {
                        if (!isconstzero(tp1, exp1))
                            error(ERR_NONPORTABLE_POINTER_COMPARISON);
                    }
                    else if (isint(tp1))
                        error(ERR_NONPORTABLE_POINTER_COMPARISON);
                    else if (isfloat(tp1) || isimaginary(tp1))
                        error(ERR_ILL_USE_OF_FLOATING);
                    else if (iscomplex(tp1))
                        error(ERR_ILL_USE_OF_COMPLEX);
                    else if (ispointer(tp1) && !isvoidptr(*tp) && !isvoidptr(tp1) && !comparetypes(*tp, tp1, false))
                        error(ERR_NONPORTABLE_POINTER_COMPARISON);
                }
                else if (ispointer(tp1))
                {
                    if (isintconst(*exp))
                    {
                        if (!isconstzero(*tp, *exp))
                            error(ERR_NONPORTABLE_POINTER_COMPARISON);
                    }
                    else if (isint(*tp))
                        error(ERR_NONPORTABLE_POINTER_COMPARISON);
                    else if (isfloat(*tp) || isimaginary(*tp))
                        error(ERR_ILL_USE_OF_FLOATING);
                    else if (iscomplex(*tp))
                        error(ERR_ILL_USE_OF_COMPLEX);
                }
                else if (isint(*tp) && isint(tp1))
                {
                    if ((isunsigned(*tp) && !isunsigned(tp1)) || (isunsigned(tp1) && !isunsigned(*tp)))
                        errorstr(ERR_SIGNED_UNSIGNED_MISMATCH_RELAT, opname);
                }
                *tp = destSize(*tp, tp1, exp, &exp1, true, nullptr);
                switch (kw)
                {
                    case gt:
                        type = isunsigned(*tp) ? en_ugt : en_gt;
                        break;
                    case geq:
                        type = isunsigned(*tp) ? en_uge : en_ge;
                        break;
                    case lt:
                        type = isunsigned(*tp) ? en_ult : en_lt;
                        break;
                    case leq:
                        type = isunsigned(*tp) ? en_ule : en_le;
                        break;
                    default:
                        done = true;
                        break;
                }
                *exp = exprNode(type, *exp, exp1);
                if (cparams.prm_cplusplus)
                    *tp = &stdbool;
                else
                    *tp = &stdint;
            }
        }
    }
    return lex;
}
static LEXEME* expression_equality(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    lex = expression_inequality(lex, funcsp, atp, tp, exp, ismutable, flags);
    if (*tp == nullptr)
        return lex;
    while (MATCHKW(lex, eq) || MATCHKW(lex, neq))
    {
        bool done = false;
        TYPE* tp1 = nullptr;
        EXPRESSION* exp1 = nullptr;
        enum e_kw kw = KW(lex);
        lex = getsym();
        lex = expression_inequality(lex, funcsp, nullptr, &tp1, &exp1, nullptr, flags);
        if (!tp1)
        {
            *tp = nullptr;
            return lex;
        }
        ResolveTemplateVariable(tp, exp, tp1, atp);
        ResolveTemplateVariable(&tp1, &exp1, *tp, atp);
        if ((cparams.prm_cplusplus || chosenAssembler->msil) &&
            insertOperatorFunc(ovcl_binary_numericptr, kw, funcsp, tp, exp, tp1, exp1, nullptr, flags))
        {
        }
        else
        {
            checkscope(*tp, tp1);
            castToArithmetic(false, tp, exp, kw, tp1, true);
            castToArithmetic(false, &tp1, &exp1, (enum e_kw) - 1, *tp, true);
            if (TotalErrors())
            {
                insertOperatorFunc(ovcl_binary_numericptr, kw, funcsp, tp, exp, tp1, exp1, nullptr, flags);
            }
            if (cparams.prm_cplusplus)
            {
                SYMBOL* funcsp = nullptr;
                if ((ispointer(*tp) || basetype(*tp)->type == bt_memberptr) && tp1->type == bt_aggregate)
                {
                    if (tp1->syms->table[0]->next)
                        errorstr(ERR_OVERLOADED_FUNCTION_AMBIGUOUS, ((SYMBOL*)tp1->syms->table[0]->p)->name);
                    exp1 = varNode(en_pc, tp1->syms->table[0]->p);
                    tp1 = ((SYMBOL*)tp1->syms->table[0]->p)->tp;
                }
                else if ((ispointer(tp1) || basetype(tp1)->type == bt_memberptr) && (*tp)->type == bt_aggregate)
                {
                    if ((*tp)->syms->table[0]->next)
                        errorstr(ERR_OVERLOADED_FUNCTION_AMBIGUOUS, ((SYMBOL*)(*tp)->syms->table[0]->p)->name);
                    (*exp) = varNode(en_pc, (*tp)->syms->table[0]->p);
                    (*tp) = ((SYMBOL*)(*tp)->syms->table[0]->p)->tp;
                }
                if (funcsp)
                    GENREF(funcsp);
            }
            if ((*exp)->type == en_pc || ((*exp)->type == en_func && !(*exp)->v.func->ascall))
                thunkForImportTable(exp);
            if (exp1->type == en_pc || (exp1->type == en_func && !exp1->v.func->ascall))
                thunkForImportTable(&exp1);
            if (isstructured(*tp) || isstructured(tp1))
            {
                if (!chosenAssembler->msil || (!isconstzero(*tp, *exp) && !isconstzero(tp1, exp1)))
                    error(ERR_ILL_STRUCTURE_OPERATION);
            }
            else if (isvoid(*tp) || isvoid(tp1) || ismsil(*tp) || ismsil(tp1))
                error(ERR_NOT_AN_ALLOWED_TYPE);
            if (ispointer(*tp))
            {
                if (isintconst(exp1))
                {
                    if (!isconstzero(tp1, exp1))
                        error(ERR_NONPORTABLE_POINTER_COMPARISON);
                }
                else if (isint(tp1))
                    error(ERR_NONPORTABLE_POINTER_COMPARISON);
                else if (isfloat(tp1) || isimaginary(tp1))
                    error(ERR_ILL_USE_OF_FLOATING);
                else if (iscomplex(tp1))
                    error(ERR_ILL_USE_OF_COMPLEX);
                else if (ispointer(tp1) && !isvoidptr(*tp) && !isvoidptr(tp1) && !comparetypes(*tp, tp1, false))
                    error(ERR_NONPORTABLE_POINTER_COMPARISON);
            }
            else if (ispointer(tp1))
            {
                if (isintconst(*exp))
                {
                    if (!isconstzero(*tp, *exp))
                        error(ERR_NONPORTABLE_POINTER_COMPARISON);
                }
                else if (isint(*tp))
                    error(ERR_NONPORTABLE_POINTER_COMPARISON);
                else if (isfloat(*tp) || isimaginary(*tp))
                    error(ERR_ILL_USE_OF_FLOATING);
                else if (iscomplex(*tp))
                    error(ERR_ILL_USE_OF_COMPLEX);
            }
            else if (basetype(*tp)->type == bt_memberptr)
            {
                if (basetype(tp1)->type == bt_memberptr)
                {
                    if (!comparetypes(basetype(*tp)->btp, basetype(tp1)->btp, true))
                    {
                        error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                    }
                    *exp = exprNode(en_mp_compare, *exp, exp1);
                    (*exp)->size = (*tp)->size;
                    if (kw == neq)
                        *exp = exprNode(en_not, *exp, nullptr);
                    done = true;
                }
                else if (isconstzero(tp1, exp1))
                {
                    *exp = exprNode(en_mp_as_bool, *exp, nullptr);
                    (*exp)->size = (*tp)->size;
                    if (kw == eq)
                        *exp = exprNode(en_not, *exp, nullptr);
                    done = true;
                }
                else if (comparetypes(basetype(*tp)->btp, tp1, true))
                {
                    int lbl = dumpMemberPtr(exp1->v.sp, *tp, true);
                    exp1 = intNode(en_labcon, lbl);
                    *exp = exprNode(en_mp_compare, *exp, exp1);
                    (*exp)->size = (*tp)->size;
                    if (kw == neq)
                        *exp = exprNode(en_not, *exp, nullptr);
                    done = true;
                }
                else
                {
                    error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                }
            }
            else if (basetype(tp1)->type == bt_memberptr)
            {
                if (isconstzero(*tp, *exp))
                {
                    *exp = exprNode(en_mp_as_bool, exp1, nullptr);
                    (*exp)->size = (tp1)->size;
                    if (kw == eq)
                        *exp = exprNode(en_not, *exp, nullptr);
                    done = true;
                }
                else if (comparetypes(*tp, basetype(tp1)->btp, true))
                {
                    int lbl = dumpMemberPtr((*exp)->v.sp, tp1, true);
                    *(exp) = intNode(en_labcon, lbl);
                    *exp = exprNode(en_mp_compare, *exp, exp1);
                    (*exp)->size = tp1->size;
                    if (kw == neq)
                        *exp = exprNode(en_not, *exp, nullptr);
                    done = true;
                }
                else
                {
                    error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                }
            }
            if (!done)
            {
                if (!(chosenAssembler->arch->preferopts & OPT_BYTECOMPARE) ||
                    (!fittedConst(*tp, *exp) && !fittedConst(tp1, exp1)) || !isint(*tp) || !isint(tp1))
                    if (!isstructured(*tp) && !isstructured(tp1))
                        destSize(*tp, tp1, exp, &exp1, true, nullptr);
                *exp = exprNode(kw == eq ? en_eq : en_ne, *exp, exp1);
            }
            if (cparams.prm_cplusplus)
                *tp = &stdbool;
            else
                *tp = &stdint;
        }
    }
    return lex;
}
void GetLogicalDestructors(EXPRESSION* top, EXPRESSION* cur)
{
    if (!cur || cur->type == en_land || cur->type == en_lor || cur->type == en_cond)
        return;
    if (cur->type == en_func)
    {
        INITLIST* args = cur->v.func->arguments;
        while (args)
        {
            GetLogicalDestructors(top, args->exp);
            args = args->next;
        }
        if (cur->v.func->returnSP)
        {
            SYMBOL* sym = cur->v.func->returnSP;
            if (!sym->destructed && sym->dest && sym->dest->exp)
            {
                LIST* listitem;
                sym->destructed = true;
                listitem = (LIST*)(LIST*)Alloc(sizeof(LIST));
                listitem->data = sym->dest->exp;
                listitem->next = top->destructors;
                top->destructors = listitem;
            }
        }
    }
    if (cur->left)
    {
        GetLogicalDestructors(top, cur->left);
    }
    if (cur->right)
    {
        GetLogicalDestructors(top, cur->right);
    }
}
static LEXEME* binop(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, enum e_kw kw, enum e_node type,
                     LEXEME*(nextFunc)(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable,
                                       int flags),
                     bool* ismutable, int flags)
{
    bool first = true;
    lex = (*nextFunc)(lex, funcsp, atp, tp, exp, ismutable, flags);
    if (*tp == nullptr)
        return lex;
    while (MATCHKW(lex, kw))
    {
        TYPE* tp1 = nullptr;
        EXPRESSION* exp1 = nullptr;
        if (first)
        {
            first = false;
            GetLogicalDestructors(*exp, *exp);
        }
        lex = getsym();
        lex = (*nextFunc)(lex, funcsp, atp, &tp1, &exp1, nullptr, flags);
        if (!tp1)
        {
            *tp = nullptr;
            break;
        }
        GetLogicalDestructors(exp1, exp1);
        if (cparams.prm_cplusplus && insertOperatorFunc(kw == lor || kw == land ? ovcl_binary_numericptr : ovcl_binary_int, kw,
                                                        funcsp, tp, exp, tp1, exp1, nullptr, flags))
        {
            continue;
        }
        if (kw == land || kw == lor)
        {
            castToArithmetic(kw != land && kw != lor, tp, exp, kw, &stdbool, kw != land && kw != lor);
            castToArithmetic(kw != land && kw != lor, &tp1, &exp1, (enum e_kw) - 1, &stdbool, kw != land && kw != lor);
        }
        else
        {
            castToArithmetic(kw != land && kw != lor, tp, exp, kw, tp1, kw != land && kw != lor);
            castToArithmetic(kw != land && kw != lor, &tp1, &exp1, (enum e_kw) - 1, *tp, kw != land && kw != lor);
        }
        LookupSingleAggregate(*tp, exp);
        LookupSingleAggregate(tp1, &exp1);

        if (isstructured(*tp) || isstructured(tp1))
            error(ERR_ILL_STRUCTURE_OPERATION);
        else if (isvoid(*tp) || isvoid(tp1) || ismsil(*tp) || ismsil(tp1))
            error(ERR_NOT_AN_ALLOWED_TYPE);
        else if ((basetype(*tp)->scoped || basetype(tp1)->scoped) &&
                 (!chosenAssembler->msil || !chosenAssembler->msil->allowExtensions))
            error(ERR_SCOPED_TYPE_MISMATCH);
        if (type != en_lor && type != en_land)
        {
            if (ispointer(*tp) || ispointer(tp1))
                error(ERR_ILL_POINTER_OPERATION);
            else if (isfloat(*tp) || isfloat(tp1) || isimaginary(*tp) || isimaginary(tp1))
                error(ERR_ILL_USE_OF_FLOATING);
            else if (iscomplex(*tp) || iscomplex(tp1))
                error(ERR_ILL_USE_OF_COMPLEX);
            else if (basetype(*tp)->type == bt_memberptr || basetype(tp1)->type == bt_memberptr)
                error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
            *tp = destSize(*tp, tp1, exp, &exp1, true, atp);
        }
        else
        {
            if (cparams.prm_cplusplus)
                *tp = &stdbool;
            else
                *tp = &stdint;
        }
        if (basetype(*tp)->type == bt_memberptr)
        {
            *exp = exprNode(en_mp_as_bool, *exp, nullptr);
            (*exp)->size = (*tp)->size;
        }
        if (basetype(tp1)->type == bt_memberptr)
        {
            exp1 = exprNode(en_mp_as_bool, exp1, nullptr);
            exp1->size = (tp1)->size;
        }
        *exp = exprNode(type, *exp, exp1);
    }
    return lex;
}
static LEXEME* expression_and(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    return binop(lex, funcsp, atp, tp, exp, andx, en_and, expression_equality, ismutable, flags);
}
static LEXEME* expression_xor(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    return binop(lex, funcsp, atp, tp, exp, uparrow, en_xor, expression_and, ismutable, flags);
}
static LEXEME* expression_or(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    return binop(lex, funcsp, atp, tp, exp, orx, en_or, expression_xor, ismutable, flags);
}
static LEXEME* expression_land(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    return binop(lex, funcsp, atp, tp, exp, land, en_land, expression_or, ismutable, flags);
}
static LEXEME* expression_lor(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    return binop(lex, funcsp, atp, tp, exp, lor, en_lor, expression_land, ismutable, flags);
}

static LEXEME* expression_hook(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    lex = expression_lor(lex, funcsp, atp, tp, exp, ismutable, flags);
    if (*tp == nullptr)
        return lex;
    if (MATCHKW(lex, hook))
    {
        TYPE *tph = nullptr, *tpc = nullptr;
        EXPRESSION *eph = nullptr, *epc = nullptr;
        castToArithmetic(false, tp, exp, (enum e_kw) - 1, &stdint, true);
        GetLogicalDestructors(*exp, *exp);
        LookupSingleAggregate(*tp, exp);

        if (isstructured(*tp))
            error(ERR_ILL_STRUCTURE_OPERATION);
        else if (isvoid(*tp) || ismsil(*tp))
            error(ERR_NOT_AN_ALLOWED_TYPE);
        lex = getsym();
        isCallExit = false;
        if (MATCHKW(lex, colon))
        {
            // replicate the selector into the 'true' value
            eph = anonymousVar(sc_auto, *tp);
            deref(*tp, &eph);
            tph = *tp;
            *exp = exprNode(en_assign, eph, *exp);
        }
        else
        {
            lex = expression_comma(lex, funcsp, nullptr, &tph, &eph, nullptr, flags);
        }
        bool oldCallExit = isCallExit;
        isCallExit = false;
        if (!tph)
        {
            *tp = nullptr;
        }
        else if (MATCHKW(lex, colon))
        {
            lex = getsym();
            lex = expression_assign(lex, funcsp, nullptr, &tpc, &epc, nullptr, flags);
            isCallExit &= oldCallExit;
            if (!tpc)
            {
                *tp = nullptr;
            }
            else
            {
                ResolveTemplateVariable(tp, exp, tph, atp);
                ResolveTemplateVariable(&tph, &eph, *tp, atp);
                ResolveTemplateVariable(tp, exp, tpc, atp);
                ResolveTemplateVariable(&tpc, &epc, *tp, atp);
                if (basetype(*tp)->type == bt_memberptr)
                {
                    *exp = exprNode(en_mp_as_bool, *exp, nullptr);
                    (*exp)->size = (*tp)->size;
                }
                if (tph->type == bt_void)
                    tph = tpc;
                else if (tpc->type == bt_void)
                    tpc = tph;
                if (cparams.prm_cplusplus && (isstructured(tpc) || isstructured(tph)) && !comparetypes(tph, tpc, false))
                {
                    // call a constructor?
                    if (isstructured(tph))
                    {
                        EXPRESSION* rv = eph;
                        TYPE* ctype = tph;
                        callConstructorParam(&ctype, &rv, tpc, epc, true, false, false, false);
                        epc = rv;
                        tpc = tph;
                    }
                    else
                    {
                        EXPRESSION* rv = epc;
                        TYPE* ctype = tpc;
                        callConstructorParam(&ctype, &rv, tph, eph, true, false, false, false);
                        eph = rv;
                        tph = tpc;
                    }
                }
                if (ispointer(tph) || ispointer(tpc))
                    if (!comparetypes(tph, tpc, false))
                        if (!isconstzero(tph, eph) && !isconstzero(tpc, epc))
                            error(ERR_NONPORTABLE_POINTER_CONVERSION);
                if (isfunction(tph) || isfunction(tpc))
                    if (!comparetypes(tph, tpc, true))
                        if (!isconstzero(tph, eph) && !isconstzero(tpc, epc))
                            error(ERR_NONPORTABLE_POINTER_CONVERSION);
                if (tph != tpc && (ispointer(tph) || isfunction(tph) || ispointer(tpc) || isfunction(tpc)))
                {
                    if (!comparetypes(tpc, tph, true))
                    {
                        if ((isvoidptr(tpc) && ispointer(tph)) || (isvoidptr(tph) && ispointer(tpc)))
                        {
                            if (tph->nullptrType)
                                tph = tpc;
                            else if (tpc->nullptrType)
                                tpc = tph;
                            else
                                tpc = tph = &stdpointer;
                        }
                        else if (!((ispointer(tph) || isfunction(tph))) && !((ispointer(tpc) || isfunction(tpc))))
                        {
                            if (!comparetypes(tpc, tph, false))
                                if (!(isarithmetic(tpc) && isarithmetic(tph)))
                                    errortype(ERR_TWO_OPERANDS_SAME_TYPE, tpc, tph);
                        }
                        else if ((isfunction(tph) || isfunction(tpc)) && !comparetypes(tpc, tph, true))
                            if (!isarithmetic(tph) && !isarithmetic(tpc))
                                errortype(ERR_TWO_OPERANDS_SAME_TYPE, tpc, tph);
                    }
                }
                if (isfunction(tph))
                    *tp = tph;
                else if (isfunction(tpc))
                    *tp = tpc;
                else if (!isvoid(tpc))
                    *tp = destSize(tpc, tph, &epc, &eph, false, nullptr);
                else
                {
                    if (chosenAssembler->msil)
                    {
                        EXPRESSION* exp1 = eph;
                        while (castvalue(exp1))
                            exp1 = exp1->left;
                        if (exp1->type == en_thisref)
                            exp1 = exp1->left;
                        if (exp1->type == en_func)
                            if (isvoid(basetype(exp1->v.func->sp->tp)->btp))
                                eph = exprNode(en_void, eph, intNode(en_c_i, 0));
                        exp1 = epc;
                        while (castvalue(exp1))
                            exp1 = exp1->left;
                        if (exp1->type == en_thisref)
                            exp1 = exp1->left;
                        if (exp1->type == en_func)
                            if (isvoid(basetype(exp1->v.func->sp->tp)->btp))
                                epc = exprNode(en_void, epc, intNode(en_c_i, 0));
                    }
                    *tp = tpc;
                }
                *exp = exprNode(en_cond, *exp, exprNode(en_void, eph, epc));
                if (isstructured(*tp))
                    *exp = exprNode(en_not_lvalue, *exp, nullptr);
            }
        }
        else
        {
            error(ERR_HOOK_NEEDS_COLON);
            *tp = nullptr;
        }
    }
    return lex;
}
static bool isTemplatedPointer(TYPE* tp)
{
    TYPE* tpb = basetype(tp)->btp;
    while (tp != tpb)
    {
        if (tp->templateTop)
            return true;
        tp = tp->btp;
    }
    return false;
}
LEXEME* expression_throw(LEXEME* lex, SYMBOL* funcsp, TYPE** tp, EXPRESSION** exp)
{
    TYPE* tp1 = nullptr;
    EXPRESSION* exp1 = nullptr;
    *tp = &stdvoid;
    hasXCInfo = true;
    lex = getsym();
    functionCanThrow = true;
    if (!MATCHKW(lex, semicolon))
    {
        SYMBOL* sym = namespacesearch("_ThrowException", globalNameSpace, false, false);
        makeXCTab(funcsp);
        lex = expression_assign(lex, funcsp, nullptr, &tp1, &exp1, nullptr, 0);
        if (!tp1)
        {
            error(ERR_EXPRESSION_SYNTAX);
        }
        else if (sym)
        {
            FUNCTIONCALL* params = (FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
            INITLIST* arg1 = (INITLIST*)Alloc(sizeof(INITLIST));  // exception table
            INITLIST* arg2 = (INITLIST*)Alloc(sizeof(INITLIST));  // instance
            INITLIST* arg3 = (INITLIST*)Alloc(sizeof(INITLIST));  // array size
            INITLIST* arg4 = (INITLIST*)Alloc(sizeof(INITLIST));  // constructor
            INITLIST* arg5 = (INITLIST*)Alloc(sizeof(INITLIST));  // exception block
            SYMBOL* rtti = RTTIDumpType(tp1);
            SYMBOL* cons = nullptr;
            if (isstructured(tp1))
            {
                cons = getCopyCons(basetype(tp1)->sp, false);
                if (!cons->inlineFunc.stmt)
                {
                    if (cons->defaulted)
                        createConstructor(basetype(tp1)->sp, cons);
                    else if (cons->deferredCompile)
                        deferredCompileOne(cons);
                }
                GENREF(cons);
            }
            sym = (SYMBOL*)basetype(sym->tp)->syms->table[0]->p;
            arg1->next = arg2;
            arg2->next = arg3;
            arg3->next = arg4;
            arg4->next = arg5;
            arg1->exp = varNode(en_auto, funcsp->xc->xctab);
            arg1->tp = &stdpointer;
            if (isstructured(tp1))
            {
                arg2->exp = exp1;
            }
            else
            {
                EXPRESSION* exp3 = anonymousVar(sc_auto, tp1);
                arg2->exp = exp3;
                deref(tp1->type == bt_pointer ? &stdpointer : tp1, &exp3);
                exp3 = exprNode(en_assign, exp3, exp1);
                arg2->exp = exprNode(en_void, exp3, arg2->exp);
            }
            arg2->tp = &stdpointer;
            arg3->exp = isarray(tp1) ? intNode(en_c_i, tp1->size / (basetype(tp1)->btp->size)) : intNode(en_c_i, 1);
            arg3->tp = &stdint;
            arg4->exp = cons ? varNode(en_pc, cons) : intNode(en_c_i, 0);
            arg4->tp = &stdpointer;
            if (cons && cons->attribs.inheritable.linkage2 == lk_import)
            {
               arg4->exp = exprNode(en_l_p, arg4->exp, nullptr);   
            }
            arg5->exp = rtti ? varNode(en_global, rtti) : intNode(en_c_i, 0);
            arg5->tp = &stdpointer;
            params->arguments = arg1;
            params->ascall = true;
            params->sp = sym;
            params->functp = sym->tp;
            params->fcall = varNode(en_pc, sym);
            *exp = exprNode(en_func, nullptr, nullptr);
            (*exp)->v.func = params;
        }
    }
    else
    {
        SYMBOL* sym = namespacesearch("_RethrowException", globalNameSpace, false, false);
        if (sym)
        {
            FUNCTIONCALL* parms = (FUNCTIONCALL*)Alloc(sizeof(FUNCTIONCALL));
            INITLIST* arg1 = (INITLIST*)Alloc(sizeof(INITLIST));  // exception table
            makeXCTab(funcsp);
            sym = (SYMBOL*)basetype(sym->tp)->syms->table[0]->p;
            parms->ascall = true;
            parms->sp = sym;
            parms->functp = sym->tp;
            parms->fcall = varNode(en_pc, sym);
            parms->arguments = arg1;
            arg1->exp = varNode(en_auto, funcsp->xc->xctab);
            arg1->tp = &stdpointer;
            *exp = exprNode(en_func, nullptr, nullptr);
            (*exp)->v.func = parms;
        }
    }
    return lex;
}
static void ReplaceThisAssign(EXPRESSION** init, SYMBOL* sym, EXPRESSION* exp)
{
    if (*init)
    {
        if ((*init)->type == en_global)
        {
            if ((*init)->v.sp == sym)
                *init = exp;
        }
        else
        {
            ReplaceThisAssign(&(*init)->left, sym, exp);
            ReplaceThisAssign(&(*init)->right, sym, exp);
        }
    }
}
LEXEME* expression_assign(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    bool done = false;
    EXPRESSION *exp1 = nullptr, **exp2;
    EXPRESSION* asndest = nullptr;

    bool localMutable = false;
    TYPE* tp2;
    if (MATCHKW(lex, kw_throw))
    {
        return expression_throw(lex, funcsp, tp, exp);
    }
    lex = expression_hook(lex, funcsp, atp, tp, exp, &localMutable, flags);
    if (*tp == nullptr)
        return lex;
    while (!done && lex)
    {
        enum e_kw kw = KW(lex);
        enum e_node op;
        enum ovcl selovcl;
        TYPE* tp1 = nullptr;
        EXPRESSION* symRef;
        switch (kw)
        {
            case asand:
            case asor:
            case asxor:
            case asmod:
            case asleftshift:
            case asrightshift:
                selovcl = ovcl_assign_int;
                break;
            case astimes:
            case asdivide:
                selovcl = ovcl_assign_numeric;
                break;
            case assign:
                selovcl = ovcl_assign_any;
                break;
            case asplus:
            case asminus:
                selovcl = ovcl_assign_numericptr;
                break;
            default:
                if (ismutable)
                    *ismutable = localMutable;
                done = true;
                continue;
        }
        lex = getsym();
        switch (kw)
        {
            case assign:
                if (cparams.prm_cplusplus && MATCHKW(lex, begin))
                {
                    if (isstructured(*tp))
                    {
                        INITIALIZER* init = nullptr;
                        SYMBOL* spinit = nullptr;
                        tp1 = *tp;
                        spinit = anonymousVar(sc_localstatic, tp1)->v.sp;
                        insert(spinit, localNameSpace->valueData->syms);
                        lex = initType(lex, funcsp, 0, sc_auto, &init, nullptr, tp1, spinit, false, flags | _F_ASSIGNINIT);
                        EXPRESSION *exp2 = nullptr;
                        if (init && init->exp->type == en_thisref)
                        {
                            ReplaceThisAssign(&init->exp->left->v.func->thisptr, spinit, *exp);
                            spinit->allocate = false;
                        }
                        else
                        {
                            exp2 = *exp;
                        }
                        *exp = convertInitToExpression(tp1, nullptr, exp2, funcsp, init, exp1, false);

                        return lex;
                    }
                    else
                    {
                        lex = getsym();
                        lex = expression_assign(lex, funcsp, *tp, &tp1, &exp1, nullptr, flags);
                        if (!needkw(&lex, end))
                        {
                            errskim(&lex, skim_end);
                            skip(&lex, end);
                        }
                    }
                }
                else
                {
                    lex = expression_assign(lex, funcsp, *tp, &tp1, &exp1, nullptr, flags);
                }
                break;
            case asplus:
            case asminus:
            case asand:
            case asor:
            case asxor:
                lex = expression_assign(lex, funcsp, *tp, &tp1, &exp1, nullptr, flags);
                break;
            default:
                lex = expression_assign(lex, funcsp, nullptr, &tp1, &exp1, nullptr, flags);
                break;
        }
        if (!tp1)
        {
            *tp = nullptr;
            return lex;
        }
        ResolveTemplateVariable(tp, exp, tp1, nullptr);
        ResolveTemplateVariable(&tp1, &exp1, *tp, nullptr);
        if ((cparams.prm_cplusplus || chosenAssembler->msil) &&
            insertOperatorFunc(selovcl, kw, funcsp, tp, exp, tp1, exp1, nullptr, flags))
        {
            // unallocated var for destructor
            if (asndest)
            {
                SYMBOL* sym = anonymousVar(sc_auto, tp1)->v.sp;
                callDestructor(sym, nullptr, &asndest, nullptr, true, false, false);
                initInsert(&sym->dest, tp1, asndest, 0, true);
            }

            continue;
        }
        if (kw == assign && (flags & _F_SELECTOR))
        {
            /* a little naive... */
            error(ERR_POSSIBLE_INCORRECT_ASSIGNMENT);
            flags &= ~_F_SELECTOR;
        }
        checkscope(*tp, tp1);
        if (cparams.prm_cplusplus)
        {
            if (isarithmetic(*tp))
            {
                castToArithmetic(false, &tp1, &exp1, (enum e_kw) - 1, *tp, true);
            }
            else if (isstructured(tp1))
            {
                cppCast(*tp, &tp1, &exp1);
            }
        }
        exp2 = &exp1;
        while (castvalue(*exp2))
            exp2 = &(*exp2)->left;
        if ((*exp2)->type == en_func && (*exp2)->v.func->sp->storage_class == sc_overloads)
        {
            TYPE* tp2 = nullptr;
            SYMBOL* funcsp;
            if ((*exp2)->v.func->sp->parentClass && !(*exp2)->v.func->asaddress)
                error(ERR_NO_IMPLICIT_MEMBER_FUNCTION_ADDRESS);
            funcsp = MatchOverloadedFunction((*tp), isfuncptr(*tp) || basetype(*tp)->type == bt_memberptr ? &tp1 : &tp2,
                                             (*exp2)->v.func->sp, exp2, flags);
            if (funcsp && basetype(*tp)->type == bt_memberptr)
            {
                int lbl = dumpMemberPtr(funcsp, *tp, true);
                exp1 = intNode(en_labcon, lbl);
            }
            if (funcsp)
            {
                tp1 = funcsp->tp;
                if (exp1->type == en_pc || (exp1->type == en_func && !exp1->v.func->ascall))
                {
                    thunkForImportTable(&exp1);
                }
            }
            if (basetype(*tp)->btp && !comparetypes(basetype(*tp)->btp, tp1, true))
            {
                if (!isvoidptr(*tp))
                    errortype(ERR_CANNOT_CONVERT_TYPE, tp1, *tp);
            }
        }
        if (exp1->type == en_pc || (exp1->type == en_func && !exp1->v.func->ascall))
        {
            if (chosenAssembler->msil)
            {
                ValidateMSILFuncPtr(*tp, tp1, &exp1);
            }
        }
        symRef = chosenAssembler->msil ? GetSymRef(*exp) : nullptr;
        LookupSingleAggregate(tp1, &exp1);

        EXPRESSION*temp = GetSymRef(*exp);
        if (isconstraw(*tp, true) && !localMutable && (!temp || temp->v.sp->storage_class != sc_parameter || !isarray(*tp)))
            error(ERR_CANNOT_MODIFY_CONST_OBJECT);
        else if (isvoid(*tp) || isvoid(tp1) || (*tp)->type == bt_aggregate)
            error(ERR_NOT_AN_ALLOWED_TYPE);
        else if (!isstructured(*tp) && ((*tp)->btp && !ispointer((*tp)->btp)) && (!isarray(*tp) || !basetype(*tp)->msil) &&
                 basetype(*tp)->type != bt_memberptr && basetype(*tp)->type != bt_templateparam &&
                 basetype(*tp)->type != bt_templateselector && !lvalue(*exp) && (*exp)->type != en_msil_array_access)
            error(ERR_LVALUE);
        else if (symRef && symRef->v.sp->attribs.inheritable.linkage2 == lk_property && !symRef->v.sp->has_property_setter)
            errorsym(ERR_CANNOT_MODIFY_PROPERTY_WITHOUT_SETTER, symRef->v.sp);
        else
            switch (kw)
            {
                case asand:
                case asor:
                case asxor:
                case asleftshift:
                case asrightshift:
                    if (ismsil(*tp) || ismsil(tp1))
                        error(ERR_NOT_AN_ALLOWED_TYPE);
                    if (iscomplex(*tp) || iscomplex(tp1))
                        error(ERR_ILL_USE_OF_COMPLEX);
                    if (isfloat(*tp) || isfloat(tp1) || isimaginary(*tp) || isimaginary(tp1))
                        error(ERR_ILL_USE_OF_FLOATING);
                    /* fall through */
                case astimes:
                case asdivide:
                case asmod:
                    if (ismsil(*tp) || ismsil(tp1))
                        error(ERR_NOT_AN_ALLOWED_TYPE);
                    if (ispointer(*tp) || ispointer(tp1))
                        error(ERR_ILL_POINTER_OPERATION);
                    if (isstructured(*tp) || isstructured(tp1))
                        error(ERR_ILL_STRUCTURE_OPERATION);
                    if (basetype(*tp)->type == bt_memberptr || basetype(tp1)->type == bt_memberptr)
                        error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                    break;
                case asplus:
                    if ((*tp)->type == bt___string)
                    {
                        if (exp1->type == en_labcon && exp1->string)
                            exp1->type = en_c_string;
                        else if (!ismsil(tp1))
                            exp1 = exprNode(en_x_object, exp1, nullptr);
                    }
                    else if (ismsil(*tp) || ismsil(tp1))
                        error(ERR_NOT_AN_ALLOWED_TYPE);
                    else if (ispointer(*tp))
                    {
                        if (ispointer(tp1))
                            error(ERR_ILL_POINTER_ADDITION);
                        else
                        {
                            EXPRESSION* ns;
                            if (iscomplex(tp1))
                                error(ERR_ILL_USE_OF_COMPLEX);
                            else if (isfloat(tp1) || isimaginary(tp1))
                                error(ERR_ILL_USE_OF_FLOATING);
                            else if (isstructured(tp1))
                                error(ERR_ILL_STRUCTURE_OPERATION);
                            if (basetype(basetype(*tp)->btp)->type == bt_void)
                            {
                                if (cparams.prm_cplusplus)
                                    error(ERR_ARITHMETIC_WITH_VOID_STAR);
                                cast(&stdcharptr, &exp1);
                                ns = nodeSizeof(&stdchar, exp1);
                            }
                            else
                            {
                                cast((*tp), &exp1);
                                ns = nodeSizeof(basetype(*tp)->btp, exp1);
                            }
                            exp1 = exprNode(en_umul, exp1, ns);
                        }
                    }
                    else if (ispointer(tp1))
                    {
                        error(ERR_ILL_POINTER_OPERATION);
                    }
                    else if (isstructured(*tp) || isstructured(tp1))
                        error(ERR_ILL_STRUCTURE_OPERATION);
                    else if (basetype(*tp)->type == bt_memberptr || basetype(tp1)->type == bt_memberptr)
                        error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                    break;
                case asminus:
                    if (ismsil(*tp) || ismsil(tp1))
                        error(ERR_NOT_AN_ALLOWED_TYPE);
                    if (isstructured(*tp) || isstructured(tp1))
                        error(ERR_ILL_STRUCTURE_OPERATION);
                    else if (ispointer(tp1))
                    {
                        error(ERR_ILL_POINTER_SUBTRACTION);
                    }
                    else if (basetype(*tp)->type == bt_memberptr || basetype(tp1)->type == bt_memberptr)
                        error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                    else if (ispointer(*tp))
                    {
                        EXPRESSION* ns;
                        if (basetype(basetype(*tp)->btp)->type == bt_void)
                        {
                            if (cparams.prm_cplusplus)
                                error(ERR_ARITHMETIC_WITH_VOID_STAR);
                            cast(&stdcharptr, &exp1);
                            ns = nodeSizeof(&stdchar, exp1);
                        }
                        else
                        {
                            cast((*tp), &exp1);
                            ns = nodeSizeof(basetype(*tp)->btp, exp1);
                        }
                        exp1 = exprNode(en_umul, exp1, ns);
                    }
                    break;
                case assign:
                    if (basetype(*tp)->type == bt___string)
                    {
                        if (exp1->type == en_labcon && exp1->string)
                            exp1->type = en_c_string;
                    }
                    else if (basetype(*tp)->type == bt___object)
                    {
                        if (tp1->type != bt___object && !isstructured(tp1) && (!isarray(tp1) || !basetype(tp1)->msil))
                            exp1 = exprNode(en_x_object, exp1, nullptr);
                    }
                    else if (ismsil(*tp) || ismsil(tp1))
                        error(ERR_NOT_AN_ALLOWED_TYPE);
                    if (ispointer(*tp))
                    {
                        if (isarray(*tp) && (*tp)->msil && !comparetypes(*tp, tp1, true) && natural_size(*exp) != ISZ_OBJECT)
                        {
                            *exp = exprNode(en_l_object, *exp, nullptr);
                            (*exp)->v.tp = tp1;
                        }
                        if (isarithmetic(tp1))
                        {
                            if (iscomplex(tp1))
                                error(ERR_ILL_USE_OF_COMPLEX);
                            else if (isfloat(tp1) || isimaginary(tp1))
                                error(ERR_ILL_USE_OF_FLOATING);
                            else if (isarithmeticconst(exp1))
                            {
                                if (!isintconst(exp1) || !isconstzero(*tp, exp1))
                                    error(ERR_NONPORTABLE_POINTER_CONVERSION);
                            }
                            else if (cparams.prm_cplusplus)
                            {
                                errortype(ERR_CANNOT_CONVERT_TYPE, tp1, *tp);
                            }
                            else
                            {
                                error(ERR_NONPORTABLE_POINTER_CONVERSION);
                            }
                        }
                        else if (ispointer(basetype(tp1)) || tp1->type == bt_any)
                        {
                            if (cparams.prm_cplusplus && !isconst(basetype(*tp)->btp) && basetype(tp1)->stringconst)
                                error(ERR_INVALID_CHARACTER_STRING_CONVERSION);

                            while (tp1->type == bt_any && tp1->btp)
                                tp1 = tp1->btp;
                            if (!ispointer(basetype(tp1)))
                                goto end;
                            if (!comparetypes(*tp, tp1, true))
                            {
                                bool found = false;
                                if (ispointer(tp1))
                                {
                                    TYPE* tpo = basetype(basetype(tp1)->btp);
                                    TYPE* tpn = basetype(basetype(*tp)->btp);
                                    if (isstructured(tpo) && isstructured(tpn))
                                    {
                                        if (classRefCount(tpo->sp, tpn->sp) == 1)
                                        {
                                            EXPRESSION* v = (EXPRESSION*)Alloc(sizeof(EXPRESSION));
                                            v->type = en_c_i;
                                            v = baseClassOffset(tpo->sp, tpn->sp, v);
                                            optimize_for_constants(&v);
                                            if (v->type == en_c_i)  // check for no virtual base
                                            {
                                                if (isAccessible(tpn->sp, tpn->sp, tpo->sp, funcsp, ac_public, false))
                                                {
                                                    *exp = exprNode(en_add, *exp, v);
                                                    found = true;
                                                }
                                            }
                                        }
                                        else if (classRefCount(tpn->sp, tpo->sp) == 1)
                                        {
                                            EXPRESSION* v = (EXPRESSION*)Alloc(sizeof(EXPRESSION));
                                            v->type = en_c_i;
                                            v = baseClassOffset(tpn->sp, tpo->sp, v);
                                            optimize_for_constants(&v);
                                            if (v->type == en_c_i)  // check for no virtual base
                                            {
                                                if (isAccessible(tpo->sp, tpo->sp, tpn->sp, funcsp, ac_public, false))
                                                {
                                                    *exp = exprNode(en_sub, *exp, v);
                                                    found = true;
                                                }
                                            }
                                        }
                                    }
                                }
                                if (!found)
                                {
                                    if (cparams.prm_cplusplus)
                                    {
                                        if ((!isvoidptr(*tp) || !ispointer(tp1)) && !tp1->nullptrType)
                                            if (!isTemplatedPointer(*tp))
                                            {
                                                errortype(ERR_CANNOT_CONVERT_TYPE, tp1, *tp);
                                            }
                                    }
                                    else if (!isvoidptr(*tp) && !isvoidptr(tp1))
                                    {
                                        if (!matchingCharTypes(*tp, tp1))
                                            error(ERR_SUSPICIOUS_POINTER_CONVERSION);
                                    }
                                    else if (cparams.prm_cplusplus && !isvoidptr(*tp) && isvoidptr(tp1) && exp1->type != en_nullptr)
                                    {
                                        error(ERR_ANSI_FORBIDS_IMPLICIT_CONVERSION_FROM_VOID);
                                    }
                                }
                            }
                        }
                        else if (isfunction(tp1))
                        {
                            if (!isvoidptr(*tp) && (!isfunction(basetype(*tp)->btp) ||
                                                    !comparetypes(basetype(basetype(*tp)->btp)->btp, basetype(tp1)->btp, true)))
                                error(ERR_SUSPICIOUS_POINTER_CONVERSION);
                        }
                        else
                        {
                            if (tp1->type == bt_memberptr)
                            {
                                errortype(ERR_CANNOT_CONVERT_TYPE, tp1, *tp);
                            }
                            else
                                error(ERR_INVALID_POINTER_CONVERSION);
                        }
                    end:;
                    }
                    else if (ispointer(tp1))
                    {
                        if (iscomplex(*tp))
                            error(ERR_ILL_USE_OF_COMPLEX);
                        else if (isfloat(*tp) || isimaginary(*tp))
                            error(ERR_ILL_USE_OF_FLOATING);
                        else if (isint(*tp))
                            error(ERR_NONPORTABLE_POINTER_CONVERSION);
                        else if (isarray(tp1) && (tp1)->msil)
                            error(ERR_MANAGED_OBJECT_NO_ADDRESS);
                    }
                    if (isstructured(*tp) && (!isstructured(tp1) || !comparetypes(*tp, tp1, true)))
                    {
                        if (!(chosenAssembler->msil && basetype(*tp)->sp->msil &&
                              (isconstzero(tp1, exp1) || basetype(tp1)->nullptrType)))
                            error(ERR_ILL_STRUCTURE_ASSIGNMENT);
                    }
                    else if (isstructured(*tp) && !(*tp)->size)
                    {
                        if (!(flags & _F_SIZEOF))
                            errorsym(ERR_STRUCT_NOT_DEFINED, basetype(*tp)->sp);
                    }
                    else if (!isstructured(*tp) && isstructured(tp1))
                    {
                        error(ERR_ILL_STRUCTURE_ASSIGNMENT);
                    }
                    else if (basetype(*tp)->type == bt_memberptr)
                    {
                        if (exp1->type == en_memberptr)
                        {
                            if (exp1->v.sp != basetype(*tp)->sp && exp1->v.sp != basetype(*tp)->sp->mainsym &&
                                !sameTemplate(exp1->v.sp->tp, basetype(*tp)->sp->tp))  // DAL FIXED
                                errortype(ERR_CANNOT_CONVERT_TYPE, tp1, *tp);
                        }
                        else if ((!isfunction(basetype(*tp)->btp) || !comparetypes(basetype(*tp)->btp, tp1, true)) &&
                                 !isconstzero(tp1, *exp) && !comparetypes(*tp, tp1, true))
                        {
                            errortype(ERR_CANNOT_CONVERT_TYPE, tp1, *tp);
                        }
                    }
                    else if (basetype(tp1)->type == bt_memberptr)
                        errortype(ERR_CANNOT_CONVERT_TYPE, tp1, *tp);
                    break;
                default:
                    done = true;
                    break;
            }
        switch (kw)
        {
            case asand:
                op = en_and;
                break;
            case asor:
                op = en_or;
                break;
            case asxor:
                op = en_xor;
                break;

            case astimes:
                op = en_mul;
                break;
            case asdivide:
                tp2 = destSize(*tp, tp1, nullptr, nullptr, false, nullptr);
                op = isunsigned(*tp) && basetype(*tp)->type > bt_int ? en_udiv : en_div;
                break;
            case asmod:
                tp2 = destSize(*tp, tp1, nullptr, nullptr, false, nullptr);
                op = isunsigned(*tp) && basetype(*tp)->type > bt_int ? en_umod : en_mod;
                break;
            case assign:
                op = en_assign;
                break;

            case asplus:
                op = en_add;
                break;
            case asminus:
                op = en_sub;
                break;

            case asleftshift:
                op = en_lsh;
                break;
            case asrightshift:
                tp2 = destSize(*tp, tp1, nullptr, nullptr, false, nullptr);
                op = isunsigned(*tp) ? en_ursh : en_rsh;
                break;
            default:
                break;
        }
        if (ismsil(*tp))
        {
            if (exp1->type == en_labcon && exp1->string)
                exp1->type = en_c_string;
            else if (!ismsil(tp1) && !isstructured(tp1) && (!isarray(tp1) || !basetype(tp1)->msil))
                exp1 = exprNode(en_x_object, exp1, nullptr);
            if (op == en_assign)
            {
                *exp = exprNode(op, *exp, exp1);
            }
            else
            {
                EXPRESSION* dest = *exp;
                *exp = exprNode(op, *exp, exp1);
                *exp = exprNode(en_assign, dest, *exp);
            }
        }
        else if (basetype(*tp)->type == bt_memberptr)
        {
            if ((*exp)->type == en_not_lvalue || ((*exp)->type == en_func && !(*exp)->v.func->ascall) || (*exp)->type == en_void ||
                (*exp)->type == en_memberptr)
            {
                if (basetype(*tp)->type != bt_templateparam)
                    error(ERR_LVALUE);
            }
            else if (exp1->type == en_memberptr)
            {
                int lbl = dumpMemberPtr(exp1->v.sp, *tp, true);
                exp1 = intNode(en_labcon, lbl);
                *exp = exprNode(en_blockassign, *exp, exp1);
                (*exp)->size = (*tp)->size;
                (*exp)->altdata = (void*)(*tp);
            }
            else if (isconstzero(tp1, exp1) || exp1->type == en_nullptr)
            {
                *exp = exprNode(en_blockclear, *exp, nullptr);
                (*exp)->size = (*tp)->size;
            }
            else if (exp1->type == en_func && exp1->v.func->returnSP)
            {
                exp1->v.func->returnSP->allocate = false;
                exp1->v.func->returnEXP = *exp;
                *exp = exp1;
            }
            else
            {
                *exp = exprNode(en_blockassign, *exp, exp1);
                (*exp)->size = (*tp)->size;
                (*exp)->altdata = (void*)(*tp);
            }
        }
        else if (isstructured(*tp))
        {
            EXPRESSION* exp2 = exp1;
            if (((*exp)->type == en_not_lvalue || (*exp)->type == en_func || (*exp)->type == en_void) && !(flags & _F_SIZEOF))
                error(ERR_LVALUE);
            if (lvalue(exp2))
                exp2 = exp2->left;
            if (exp2->type == en_func && exp2->v.func->returnSP)
            {
                exp2->v.func->returnSP->allocate = false;
                exp2->v.func->returnEXP = *exp;
                *exp = exp1;
            }
            else
            {
                *exp = exprNode(en_blockassign, *exp, exp1);
                (*exp)->size = (*tp)->size;
                (*exp)->altdata = (void*)(*tp);
                if (isatomic(*tp))
                    (*exp)->size -= ATOMIC_FLAG_SPACE;
            }
            *exp = exprNode(en_not_lvalue, *exp, nullptr);
        }
        else
        {
            if (kw == assign)
            {
                if ((*exp)->type != en_msil_array_access && exp1->type != en_msil_array_access)
                {
                    if (isarithmetic(*tp) || isarithmetic(tp1) ||
                        ((ispointer(*tp) && (!isarray(*tp) || !basetype(*tp)->msil)) ||
                         (ispointer(tp1) && (!isarray(tp1) || !basetype(tp1)->msil))))
                    {
                        int n = natural_size(*exp);
                        if (natural_size(exp1) != n)
                            cast((*tp), &exp1);
                    }
                    else if (isarray(*tp) && basetype(*tp)->msil)
                    {
                        *exp = exprNode(en_l_object, *exp, nullptr);
                        (*exp)->v.tp = *tp;
                    }
                }

                *exp = exprNode(op, *exp, exp1);
            }
            else
            {
                EXPRESSION* dest = *exp;
                *exp = RemoveAutoIncDec(*exp);
                // we want to optimize the as* operations for the backend
                // but can't do the optimization for divisions
                // otherwise it is fine for the processor we are on
                if (kw == asmod || kw == asdivide || basetype(*tp)->type == bt_bool)
                {
                    int n = natural_size(*exp);
                    destSize(*tp, tp1, exp, &exp1, false, nullptr);
                    *exp = exprNode(op, *exp, exp1);
                    if (natural_size(exp1) != n)
                        cast((*tp), exp);
                    *exp = exprNode(en_assign, dest, *exp);
                }
                else if (kw == asleftshift || kw == asrightshift)
                {
                    int n = natural_size(*exp);
                    if (natural_size(exp1) != n)
                        cast(&stdint, &exp1);
                    *exp = exprNode(op, *exp, exp1);
                    *exp = exprNode(en_assign, dest, *exp);
                }
                else
                {
                    int n = natural_size(*exp);
                    if (natural_size(exp1) != n)
                        destSize(*tp, tp1, exp, &exp1, false, nullptr);
                    *exp = exprNode(op, *exp, exp1);
                    if (natural_size(*exp) != n)
                        cast(*tp, exp);
                    *exp = exprNode(en_assign, dest, *exp);
                }
            }
        }
    }
    if ((*exp)->type == en_pc || ((*exp)->type == en_func && !(*exp)->v.func->ascall))
        thunkForImportTable(exp);
    return lex;
}
LEXEME* expression_comma(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    lex = expression_assign(lex, funcsp, atp, tp, exp, ismutable, flags);
    if (*tp == nullptr)
        return lex;
    while (MATCHKW(lex, comma))
    {
        EXPRESSION* exp1 = nullptr;
        TYPE* tp1 = nullptr;
        lex = getsym();
        lex = expression_assign(lex, funcsp, atp, &tp1, &exp1, nullptr, flags);
        if (!tp1)
        {
            break;
        }
        if ((cparams.prm_cplusplus || chosenAssembler->msil) &&
            insertOperatorFunc(ovcl_comma, comma, funcsp, tp, exp, tp1, exp1, nullptr, flags))
        {
            continue;
        }
        else
        {
            *tp = tp1;
            *exp = exprNode(en_void, *exp, exp1);
            exp = &(*exp)->right;
        }
    }
    return lex;
}
LEXEME* expression_no_comma(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, bool* ismutable, int flags)
{
    lex = expression_assign(lex, funcsp, atp, tp, exp, ismutable, flags);
    assignmentUsages(*exp, false);
    return lex;
}
LEXEME* expression_no_check(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, int flags)
{
    if (flags & _F_TYPETEST)
        anonymousNotAlloc++;
    lex = expression_comma(lex, funcsp, atp, tp, exp, nullptr, flags);
    if (flags & _F_TYPETEST)
        anonymousNotAlloc--;
    return lex;
}

LEXEME* expression(LEXEME* lex, SYMBOL* funcsp, TYPE* atp, TYPE** tp, EXPRESSION** exp, int flags)
{
    lex = expression_comma(lex, funcsp, atp, tp, exp, nullptr, flags);
    assignmentUsages(*exp, false);
    return lex;
}
