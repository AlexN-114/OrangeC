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
#include "compiler.h"
#include "rtti.h"

extern COMPILER_PARAMS cparams;
extern ARCH_ASM *chosenAssembler;
extern int stdpragmas;
extern enum e_kw skim_closepa[], skim_end[];
extern enum e_kw skim_closebr[];
extern enum e_kw skim_semi[];
extern TYPE stdpointer, stdnullpointer;
extern TYPE stdint;
extern TYPE stddouble;
extern TYPE stdvoid;
extern TYPE stdwcharptr;
extern TYPE stdcharptr;
extern TYPE stdfloatimaginary;
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
extern TYPE std__func__;
extern TYPE stdchar16tptr;
extern TYPE stdchar32tptr;
extern BOOL setjmp_used;
extern char *overloadNameTab[];
extern NAMESPACEVALUES *localNameSpace;
extern LAMBDA *lambdas;
extern int instantiatingTemplate;
extern int currentErrorLine;

int packIndex;

/* lvaule */
/* handling of const int */
/*-------------------------------------------------------------------------------------------------------------------------------- */
static EXPRESSION *nodeSizeof(TYPE *tp, EXPRESSION *exp);
static LEXEME *expression_primary(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL ampersand, BOOL noinline, BOOL packable);
LEXEME *expression_assign(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL selector, BOOL noinline, BOOL inTemplateParams, BOOL packable);
static LEXEME *expression_comma(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL selector, BOOL noinline, BOOL inTemplateParams, BOOL packable);

EXPRESSION *exprNode(enum e_node type, EXPRESSION *left, EXPRESSION *right)
{
    EXPRESSION *rv = Alloc(sizeof(EXPRESSION));
    rv->type = type;
    rv->left = left;
    rv->right = right;
    return rv;
}
EXPRESSION *varNode(enum e_node type, SYMBOL *sp)
{
    EXPRESSION *rv = Alloc(sizeof(EXPRESSION));
    rv->type = type;
    rv->v.sp = sp;
    return rv;	
}
static BOOL inreg(EXPRESSION *exp, BOOL first)
{
    while (castvalue(exp))
        exp = exp->left;
    if (first && lvalue(exp))
    {
        exp = exp->left;
        first = FALSE;
    }
    if (exp->type == en_auto)
    {
        return exp->v.sp->storage_class == sc_register;
    }
    else if (exp->type == en_add || exp->type == en_arrayadd 
             || exp->type == en_arraymul || exp->type == en_lsh
             || exp->type == en_structadd || exp->type == en_arraylsh)
        return inreg(exp->left,first) || inreg(exp->right, first);
    else
        return FALSE;		
}
EXPRESSION *intNode(enum e_node type, LLONG_TYPE val)
{
    EXPRESSION *rv = Alloc(sizeof(EXPRESSION));
    rv->type = type;
    rv->v.i = val;
    return rv;
}
void checkauto(TYPE *tp1)
{
    if (basetype(tp1)->type == bt_auto)
    {
        error(ERR_AUTO_NOT_ALLOWED);
        while (tp1->type == bt_const || tp1->type == bt_volatile)
        {
            tp1->size = getSize(bt_int);
            tp1 = tp1->btp;
        }
        tp1-> type = bt_int;
        tp1->size = getSize(bt_int);
    }
}
static LEXEME *variableName(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL ampersand, BOOL noinline, BOOL packable)
{
    char idname[512];
    FUNCTIONCALL *funcparams = NULL;
    SYMBOL *sp = NULL;
    SYMBOL *strSym = NULL;
    NAMESPACEVALUES *nsv = NULL;
    LEXEME *placeholder = lex;
    if (cparams.prm_cplusplus)
    {
        lex = id_expression(lex, funcsp, &sp, &strSym, &nsv, NULL, FALSE, FALSE, idname);
    }
    else
    {
        strcpy(idname, lex->value.s.a);
        sp = gsearch(idname);
    }
    if (sp)
    {
        SYMBOL *spx;
        HASHREC *hr;
        *tp = sp->tp;
        lex = getsym();
        switch (sp->storage_class)
        {	
            case sc_member:
                sp = lambda_capture(sp, cmThis, FALSE);
                break;
            case sc_auto:
            case sc_register:
            case sc_parameter:            
                sp = lambda_capture(sp, cmNone, FALSE);
                break;
            case sc_constant:
                if (ampersand && sp->parent)
                    sp = lambda_capture(sp, cmNone, FALSE);
                break;
        }
        if (instantiatingTemplate && sp->tp->type == bt_templateparam && sp->tp->templateParam->byNonType.val)
        {
            switch (sp->tp->templateParam->type)
            {
                case kw_typename:
                    *tp = sp->tp->templateParam->byClass.val;
                    *exp = varNode(en_templateparam, sp);   
                    return lex;
                case kw_template:
                    break;
                case kw_int:
                    *exp = sp->tp->templateParam->byNonType.val;
                    *tp = sp->tp->templateParam->byNonType.tp;
                    return lex;
            }
        }
        else
        { 
            if (sp->tp->type == bt_templateparam)
            {
                *exp = varNode(en_templateparam, sp);   
            }
            else switch (sp->storage_class)
            {	
                case sc_member:
                    if (cparams.prm_cplusplus && ampersand)
                    {
                        *exp = getMemberPtr(sp, strSym, tp, funcsp);
                    }
                    else
                    {
                        *exp = getMemberNode(sp, strSym, tp, funcsp);
                    }
                    break;
                case sc_type:
                case sc_typedef:
                    lex = prevsym(placeholder);
                    *tp = NULL;
                    lex = expression_func_type_cast(lex, funcsp, tp, exp, noinline);
                    return lex;
                case sc_overloads:
                    if (!strcmp(sp->name, "setjmp") && sp->parentClass == NULL && sp->parentNameSpace == NULL)
                        setjmp_used = TRUE;
                    hr = basetype(sp->tp)->syms->table[0];
                    funcparams = Alloc(sizeof(FUNCTIONCALL));
                    if (cparams.prm_cplusplus && MATCHKW(lex, lt))
                    {
                        lex = GetTemplateArguments(lex, funcsp, &funcparams->templateParams);
                        funcparams->astemplate = TRUE;
                    }
                    if (hr->next || cparams.prm_cplusplus)
                    {
                        if (ampersand || !MATCHKW(lex, openpa))
                        {
                            BOOL throughClass = sp->throughClass;
                            SYMBOL *sp1 = GetOverloadedFunction(tp, &funcparams->fcall, sp, funcparams->templateParams ? funcparams : NULL, atp,TRUE, FALSE);
                            if (sp1)
                            {
                                SYMBOL *tpl = sp1;
                                while (tpl)
                                {
                                    if (tpl->isTemplate)
                                        break;
                                    tpl = tpl->parentClass;
                                }
                                if (tpl && tpl->instantiated)
                                {
                                    if (!sp1->genreffed)
                                    {
                                        InsertInline(sp1);
                                        sp1->genreffed = TRUE;
                                    }
                                }
                                sp = sp1;
                                sp->throughClass = throughClass;
                                if (!isExpressionAccessible(sp, funcsp, FALSE))
                                    errorsym(ERR_CANNOT_ACCESS, sp);		
                            }
                        }
                        else
                        {
                            
                            SYMBOL *sym = getStructureDeclaration();
                            if (!sym || sym != sp->parentClass)
                                funcparams->noobject = TRUE;
                            funcparams->ascall = TRUE;
                        }
                        funcparams->sp = sp;
                    }
                    else
                    {
                        // we only get here for C language, sadly we have to do
                        // argument based lookup in C++...
                        funcparams->sp = (SYMBOL *)hr->p;
                        funcparams->fcall = varNode(en_pc, funcparams->sp);
    //					if (((SYMBOL *)hr->p)->linkage2 == lk_import)
    //					{
        //					*exp = exprNode(en_add, *exp, intNode(en_c_i, 2));
        //					deref(&stdpointer, exp);
    //						deref(&stdpointer, exp);
    //					}
                    }
                    funcparams->functp = funcparams->sp->tp;
                    *tp = funcparams->sp->tp;
                    if (cparams.prm_cplusplus 
                        && (basetype(*tp)->sp->storage_class == sc_virtual || basetype(*tp)->sp->storage_class == sc_member) 
                        && (ampersand || !MATCHKW(lex, openpa)))
                    {
                        EXPRESSION *exp1 = Alloc(sizeof(EXPRESSION));
                        exp1->type = en_memberptr;
                        exp1->left = *exp;
                        exp1->v.sp = funcparams->sp;
                        *exp = exp1;
                        getMemberPtr(sp, strSym, tp, funcsp);
                    }
                    else
                    {
                        *exp = Alloc(sizeof(EXPRESSION));
                        (*exp)->type = en_func;
                        (*exp)->v.func = funcparams;
                    }
                    break;
                case sc_catchvar:
                    makeXCTab(funcsp);
                    *exp = varNode(en_auto, funcsp->xc->xctab);
                    *exp = exprNode(en_add, *exp, intNode(en_c_i, (LLONG_TYPE)&(((struct _xctab *)0)->instance)));
                    deref(&stdpointer, exp);
                    break;
                case sc_enumconstant:		
                    *exp = intNode(en_c_i, sp->value.i);
                    break;
                case sc_constant:
                    *exp = varNode(en_const, sp);
                    break;
                case sc_auto:
                case sc_register:	/* register variables are treated as 
                                     * auto variables in this compiler
                                     * of course the usage restraints of the
                                     * register keyword are enforced elsewhere
                                     */
                    *exp = varNode(en_auto, sp);
                    break;
                case sc_parameter:
                    if (sp->packed)
                    {
                        if (!packable)
                            error(ERR_PACK_SPECIFIER_MUST_BE_USED_IN_ARGUMENT);
                        if (packIndex >= 0)
                        {
                            TEMPLATEPARAM *templateParam = sp->tp->templateParam->byPack.pack;
                            int i;
                            for (i=0; i < packIndex && templateParam; i++)
                                templateParam = templateParam->next;
                            if (templateParam)
                            {
                                sp = templateParam->packsym;
                                *tp = sp->tp;
                                *exp = varNode(en_auto, sp);
                            }
                            else
                            {
                                *exp = intNode(en_c_i, 0);
                            }
                        }
                        else
                        {
                            *exp = varNode(en_auto, sp);
                        }
                    }
                    else
                    {
                        *exp = varNode(en_auto, sp);
                    }
                    /* derefereance parameters which are declared as arrays */
                    if (basetype(sp->tp)->array)
                        deref(&stdpointer, exp);
                    break;
                
                case sc_localstatic:
                    if (funcsp && funcsp->linkage == lk_inline 
                        && funcsp->storage_class == sc_global)
                        errorsym(ERR_INLINE_CANNOT_REFER_TO_STATIC, sp);
                    if (sp->linkage3 == lk_threadlocal)
                        *exp = varNode(en_threadlocal, sp);
                    else
                        *exp = varNode(en_label, sp);
                    sp->used = TRUE;
                    break;
                case sc_absolute:
                    *exp = varNode(en_absolute, sp);
                    break;				
                case sc_static:
                    sp->used = TRUE;
                case sc_global:
                case sc_external:
                    if (sp->parentClass && !isExpressionAccessible(sp, funcsp, FALSE))
                        errorsym(ERR_CANNOT_ACCESS, sp);		
                    if (sp->linkage3 == lk_threadlocal)
                        *exp = varNode(en_threadlocal, sp);
                    else
                        *exp = varNode(en_global, sp);
                    if (sp->linkage2 == lk_import)
                    {
                        *exp = exprNode(en_add, *exp, intNode(en_c_i, 2));
                        deref(&stdpointer, exp);
                        deref(&stdpointer, exp);
                    }
                    break;
                case sc_namespace:
                case sc_namespacealias:
                    errorsym(ERR_INVALID_USE_OF_NAMESPACE, sp);
                    *exp = intNode(en_c_i, 1);
                    break;
                default:
                    error(ERR_IDENTIFIER_EXPECTED);
                    *exp = intNode(en_c_i, 1);
                    break;
            }
        }
        sp->tp->used = TRUE;
        if (sp->isTemplate && istype(sp->tp))
        {
            lex = prevsym(placeholder);
            *tp = NULL;
            lex = expression_func_type_cast(lex, funcsp, tp, exp, noinline);
        }
        else
        {
            if (sp->tp->type == bt_any)
                deref(&stdint, exp);
            else
            {
                BOOL rref = FALSE;
                if (isref(*tp))
                {
                    deref(*tp, exp);
                    if ((*tp)->type == bt_rref)
                    {
                        rref = TRUE;
                    }
                    *tp = basetype(*tp)->btp;
                }
                if (sp->storage_class != sc_overloads)
                {
                    if (!isstructured(*tp) && basetype(*tp)->type != bt_memberptr && !isfunction(*tp) && 
                        sp->storage_class != sc_constant && sp->storage_class != sc_enumconstant)
                    {
                        if (!(*tp)->array || (*tp)->vla || sp->storage_class == sc_parameter)
                            if ((*tp)->vla)
                                deref(&stdpointer, exp);
                            else
                                deref(*tp, exp);
                        else if ((*tp)->array && inreg(*exp, TRUE))
                            error(ERR_CANNOT_TAKE_ADDRESS_OF_REGISTER);
                    }
                }
                if (rref && !isfunction(*tp))
                    *exp = exprNode(en_not_lvalue, *exp, 0);
            }
    
            if (lvalue(*exp))
                (*exp)->v.sp = sp; // catch for constexpr
            (*exp)->pragmas = stdpragmas;
            if (isvolatile(*tp))
                (*exp)->isvolatile = TRUE;
            if (isrestrict(*tp))
                (*exp)->isrestrict = TRUE;
            if (isatomic(*tp))
            {
                (*exp)->isatomic = TRUE;
                if (needsAtomicLockFromType(*tp))
                    (*exp)->lockOffset = (*tp)->size - ATOMIC_FLAG_SPACE;
            }
            if (strSym && funcparams)
                funcparams->novtab = TRUE;
            if (cparams.prm_cplusplus && sp->storage_class == sc_member)
            {
                qualifyForFunc(funcsp, tp);
            }
        }
    }
    else
    {
        char *name ;
        IncGlobalFlag();
        name = litlate(idname);
        sp = Alloc(sizeof(SYMBOL ));
        sp->name = name;
        sp->used = TRUE;
        sp->declfile= lex->file;
        sp->declline = lex->line;
        sp->declfilenum = lex->filenum;
        lex = getsym();
        if (MATCHKW(lex, openpa))
        {
            if (cparams.prm_cplusplus)
            {
                sp->storage_class = sc_overloads;
                (*tp) = Alloc(sizeof(TYPE));
                (*tp)->type = bt_aggregate;
                (*tp)->sp = sp;
            }
            else
            {
                /* no prototype error will be added later */
                sp->storage_class = sc_external;
                sp->linkage = lk_c;
                sp->tp = Alloc(sizeof(TYPE));
                sp->tp->type = bt_func;
                sp->tp->size = getSize(bt_pointer);
                sp->tp->syms=CreateHashTable(1);
                sp->tp->sp = sp;
                sp->tp->btp = Alloc(sizeof(TYPE));
                sp->oldstyle = TRUE;
                sp->tp->btp->type = bt_int;
                sp->tp->btp->size = getSize(bt_int);
                InsertExtern(sp);
                *tp = sp->tp;
            }
            SetLinkerNames(sp, lk_c);
            if (sp->storage_class != sc_overloads)
            {
                InsertSymbol(sp, sp->storage_class, FALSE, FALSE);
                *exp = varNode(sp->storage_class ==sc_auto ? en_auto : en_global, sp);
            }
            else
            {
                sp->tp = Alloc(sizeof(TYPE));
                sp->tp->type = bt_func;
                sp->tp->size = getSize(bt_pointer);
                sp->tp->syms=CreateHashTable(1);
                sp->tp->sp = sp;
                sp->tp->btp = Alloc(sizeof(TYPE));
                sp->oldstyle = TRUE;
                sp->tp->btp->type = bt_int;
                sp->tp->btp->size = getSize(bt_int);
                funcparams = Alloc(sizeof(FUNCTIONCALL));
                funcparams->sp = sp;
                funcparams->functp = sp->tp;
                funcparams->fcall = intNode(en_c_i, 0);
                *exp = Alloc(sizeof(EXPRESSION));
                *tp = sp->tp;
                (*exp)->type = en_func;
                (*exp)->v.func = funcparams;
            }
        }
        else
        {
            if (cparams.prm_cplusplus && atp && isfuncptr(atp))
            {
                SYMBOL *sym;
                sp->storage_class = sc_overloads;
                (*tp) = Alloc(sizeof(TYPE));
                (*tp)->type = bt_aggregate;
                (*tp)->sp = sp;
                DecGlobalFlag();
                funcparams = Alloc(sizeof(FUNCTIONCALL));
                funcparams->ascall = TRUE;    
                 sym = GetOverloadedFunction(tp, &funcparams->fcall, sp, NULL, atp, TRUE, FALSE);
                 if (sym)
                 {
                     sym->throughClass = sp->throughClass;
                     sp = sym;
                    if (!isExpressionAccessible(sp, funcsp, FALSE))
                        errorsym(ERR_CANNOT_ACCESS, sp);		
                 }
                funcparams->sp = sp;
                funcparams->functp = funcparams->sp->tp;
                *exp = Alloc(sizeof(EXPRESSION));
                (*exp)->type = en_func;
                (*exp)->v.func = funcparams;
                IncGlobalFlag();
            }
            else
            {
                sp->storage_class = funcsp ? sc_auto : sc_global;
                sp->tp = Alloc(sizeof(TYPE));
                sp->tp->type = bt_any;
                *tp = sp->tp;
                if (!nsv)
                    errorstr(ERR_UNDEFINED_IDENTIFIER, name);
                if (nsv)
                {
                    errorqualified(ERR_NAME_IS_NOT_A_MEMBER_OF_NAME, strSym, nsv, sp->name);
                }
                deref(&stdint, exp);
                SetLinkerNames(sp, lk_c);
                if (sp->storage_class != sc_overloads)
                {
                    if (localNameSpace->syms || sp->storage_class != sc_auto)
                        InsertSymbol(sp, sp->storage_class, FALSE, FALSE);
                    *exp = varNode(sp->storage_class ==sc_auto ? en_auto : en_global, sp);
                }
                else
                {
                    funcparams = Alloc(sizeof(FUNCTIONCALL));
                    *exp = Alloc(sizeof(EXPRESSION));
                    (*exp)->type = en_func;
                    (*exp)->v.func = funcparams;
                }
            }
        }
        DecGlobalFlag();
    }
    sp->allocate = TRUE;
    return lex;
}
static LEXEME *expression_member(LEXEME *lex, SYMBOL *funcsp, TYPE **tp, EXPRESSION **exp, BOOL noinline)
{
    TYPE *typein = *tp;
    BOOL points = FALSE;
    BOOL thisptr = (*exp)->type == en_auto && (*exp)->v.sp->thisPtr;
    char *tokenName = lex->kw->name;
    (void)funcsp;
    if (MATCHKW(lex, pointsto))
    {
        SYMBOL *nesting[100];
        int n = 0;
        points = TRUE;
        do
        {
            if (isstructured(*tp))
            {
                TYPE *x = basetype(*tp);
                int i;
                for (i=0; i < n; i++)
                    if (nesting[i] == x)
                    {
                        break;
                    }
                nesting[n++] = x;
                if (n >= sizeof(nesting)/sizeof(nesting[0]))
                {
                    break;
                }
                typein = x;
            }
        }
        while (cparams.prm_cplusplus && insertOperatorFunc(ovcl_pointsto, pointsto,
                               funcsp, tp, exp, NULL,NULL, NULL, noinline));
        if (ispointer(*tp))
        {
            *tp = basetype(*tp);
            *tp = (*tp)->btp;
        }
    }
    lex = getsym();
    if (cparams.prm_cplusplus && MATCHKW(lex, compl))
    {
        // direct destructor or psuedo-destructor
        enum e_lk linkage = lk_none, linkage2 = lk_none, linkage3 = lk_none;
        BOOL defd = FALSE;
        SYMBOL *sp = NULL;
        BOOL notype = FALSE;
        TYPE *tp1 = NULL;
        lex = getsym();
        lex = getBasicType(lex, funcsp, &tp1, NULL, NULL, sc_auto, &linkage, &linkage2, &linkage3, ac_public, &notype, &defd, NULL);
        if (!tp1)
        {
            error(ERR_TYPE_NAME_EXPECTED);
        }
        else if (!comparetypes(*tp, tp1, TRUE))
        {
            error(ERR_DESTRUCTOR_MUST_MATCH_CLASS);
        }
        else if (isstructured(*tp))
        {
            // destructor
            SYMBOL *sp2 = search(overloadNameTab[CI_DESTRUCTOR], (basetype(*tp)->sp)->tp->syms);
            if (sp2)
            {
                FUNCTIONCALL *funcparams = Alloc(sizeof(FUNCTIONCALL));
                funcparams->sp = sp2;
                funcparams->thisptr = *exp;
                funcparams->thistp = Alloc(sizeof(TYPE));
                funcparams->thistp->size = getSize(bt_pointer);
                funcparams->thistp->type = bt_pointer;
                funcparams->thistp->btp = basetype(*tp);
                if (!points)
                    funcparams->novtab = TRUE;
                *exp = Alloc(sizeof(EXPRESSION));
                (*exp)->type = en_func;
                (*exp)->v.func = funcparams;   
            }
            else
            {
                if (needkw(&lex, openpa))
                    needkw(&lex, closepa);
            }
        }
        else
        {
            // psuedo-destructor, no further activity required.
            if (needkw(&lex, openpa))
                needkw(&lex, closepa);
        }
        *tp = &stdvoid;
    }
    else if (!isstructured(*tp) || points && !ispointer(typein))
    {
        if (cparams.prm_cplusplus && ISKW(lex) && (lex->kw->tokenTypes & TT_BASETYPE))
        {
            // possible psuedo destructor with selector
            enum e_lk linkage = lk_none, linkage2 = lk_none, linkage3 = lk_none;
            BOOL defd = FALSE;
            SYMBOL *sp = NULL;
            BOOL notype = FALSE;
            TYPE *tp1 = NULL;
            lex = getBasicType(lex, funcsp, &tp1, NULL, NULL, sc_auto, &linkage, &linkage2, &linkage3, ac_public, &notype, &defd, NULL);
            if (!tp1)
            {
                error(ERR_TYPE_NAME_EXPECTED);
            }
            else if (!comparetypes(*tp, tp1, TRUE))
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
                if (!MATCHKW(lex, compl))
                {
                    error(ERR_INVALID_PSUEDO_DESTRUCTOR);
                }
                else
                {
                    lex = getsym();
                    tp1 = NULL;
                    lex = getBasicType(lex, funcsp, &tp1, NULL, NULL, sc_auto, &linkage, &linkage2, &linkage3, ac_public, &notype, &defd, NULL);
                    if (!tp1)
                    {
                        error(ERR_TYPE_NAME_EXPECTED);
                    }
                    else if (!comparetypes(*tp, tp1, TRUE))
                    {
                        error(ERR_DESTRUCTOR_MUST_MATCH_CLASS);
                    }
                    else if (needkw(&lex , openpa))
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
            while (ISID(lex))
            {
                lex = getsym();
                if (!MATCHKW(lex, pointsto) && !MATCHKW(lex, dot))
                    break;
                lex = getsym();
            }
        }
    }
    else
    {
        BOOL isTemplate = FALSE;
        if (MATCHKW(lex, kw_template))
        {
            lex = getsym();
            isTemplate = TRUE;
        }
        if (!ISID(lex) && !MATCHKW(lex, kw_operator))
        {
            error(ERR_IDENTIFIER_EXPECTED);
        }
        else
        {
            SYMBOL *sp2 = NULL;
            if ((*exp)->type == en_literalclass)
            {
                CONSTEXPRSYM *ces = (CONSTEXPRSYM *)search(lex->value.s.a, (*exp)->v.syms);
                if (ces)
                {
                    lex = getsym();
                    *exp = ces->exp;
                    *tp = ces->sym->tp;
                    return lex;
                }
                else 
                {
                    STRUCTSYM l ;
                    l.str = basetype(*tp)->sp;
                    addStructureDeclaration(&l);
                    lex = id_expression(lex, funcsp, &sp2, NULL, NULL, &isTemplate, FALSE, TRUE, NULL);
                    dropStructureDeclaration();
                    if (!sp2)
                    {
                        membererror(lex->value.s.a, (*tp));
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
                        FUNCTIONCALL *funcparams = Alloc(sizeof(FUNCTIONCALL));
                        SYMBOL *match;
                        TYPE *tp1 = NULL;
                        EXPRESSION *exp1 = NULL;
                        lex = getArgs(lex, funcsp, funcparams, closepa, TRUE);
                        funcparams->thisptr = intNode(en_c_i, 0);
                        funcparams->thistp = Alloc(sizeof(TYPE));
                        funcparams->thistp->type = bt_pointer;
                        funcparams->thistp->size = getSize(bt_pointer);
                        funcparams->thistp->btp = *tp;
                        funcparams->ascall = TRUE;    
                        match = GetOverloadedFunction(&tp1, &exp1, sp2, funcparams,NULL,TRUE, FALSE);
                        if (match)
                        {
                            funcparams->sp = match;
                            *exp = substitute_params_for_function(funcparams, (*exp)->v.syms);
                            optimize_for_constants(exp);
                            *tp = basetype(match->tp)->btp;
                            if (!match->constexpression || !IsConstantExpression(*exp, TRUE))
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
                STRUCTSYM l ;
                l.str = basetype(*tp)->sp;
                addStructureDeclaration(&l);
                lex = id_expression(lex, funcsp, &sp2, NULL, NULL, &isTemplate, FALSE, TRUE, NULL);
                dropStructureDeclaration();
            }
            else
            {
                sp2 = search(lex->value.s.a, (basetype(*tp)->sp)->tp->syms);
            }
            if (!sp2)
            {
                membererror(lex->value.s.a, (*tp));
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
                TYPE *tpb;
                TYPE *basetp = *tp;
                SYMBOL *sp3 = sp2;
                TYPE *typ2 = typein;
                SYMBOL *tpl = sp2;
                if (ispointer(typ2))
                    typ2 = basetype(typ2)->btp;
                (*exp)->isatomic = FALSE;
                lex = getsym();
                sp2->used = TRUE;
                *tp = sp2->tp;
                tpb = basetype(*tp);
                if (!points && ((*exp)->type == en_not_lvalue || (*exp)->type == en_func
                    || (*exp)->type == en_void))
                    if (ISKW(lex) && lex->kw->key >= assign && lex->kw->key <= asxor)
                        error(ERR_LVALUE);
                if (sp2->storage_class == sc_overloads)
                {
                    FUNCTIONCALL *funcparams = Alloc(sizeof(FUNCTIONCALL));
                    if (cparams.prm_cplusplus && MATCHKW(lex, lt))
                    {
                        HASHREC *hr1 = basetype(sp2->tp)->syms->table[0];
                        BOOL isdest = ((SYMBOL *)hr1->p)->isDestructor;
                        while (hr1)
                        {
                             if (((SYMBOL *)hr1->p)->isTemplate)
                                 break;
                             hr1 = hr1->next;
                        }
                        if (hr1)
                        {
                            lex = GetTemplateArguments(lex, funcsp, &funcparams->templateParams);
                            funcparams->astemplate = TRUE;
                        }
                        else if (isdest && sp2->parentClass->isTemplate)
                        {
                            TEMPLATEPARAM *lst = NULL;
                            lex = GetTemplateArguments(lex, funcsp, &lst);
                            if (!exactMatchOnTemplateArgs(lst, sp2->parentClass->templateParams->bySpecialization.types))
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
                    funcparams->thistp = Alloc(sizeof(TYPE));
                    funcparams->thistp->size = getSize(bt_pointer);
                    funcparams->thistp->type = bt_pointer;
                    funcparams->thistp->btp = basetp;
                    if (!points)
                        funcparams->novtab = TRUE;
                    *exp = Alloc(sizeof(EXPRESSION));
                    (*exp)->type = en_func;
                    (*exp)->v.func = funcparams;
                }
                else 
                {
                    SYMBOL *sp3 = sp2->parentClass, *sp4 = basetype(typ2)->sp;
                    if (sp2->isTemplate)
                    {
                        if (MATCHKW(lex, lt))
                        {
                            TEMPLATEPARAM *lst = NULL;
                            SYMBOL *sp1 = sp2;
                            lex = GetTemplateArguments(lex, funcsp, &lst);
                            if (sp1)
                            {
                                sp1 = GetClassTemplate(sp1, lst);
                                if (sp1)
                                    sp1 = TemplateClassInstantiate(sp1, lst);
                                if (sp1)
                                    sp2 = sp1;
                                else
                                    errorsym(ERR_NO_TEMPLATE_MATCHES, sp2);
                            }
                        }
                        else
                        {
                            errorsym(ERR_NEED_SPECIALIZATION_PARAMETERS, sp2);
                        }
                    }
                    else if (isTemplate)
                    {
                        errorsym(ERR_NOT_A_TEMPLATE, sp2);
                    }
                    if (sp2->storage_class == sc_static || sp2->storage_class == sc_external)
                    {
                        SYMBOL *tpl = sp2;
                        while (tpl)
                        {
                            if (tpl->isTemplate)
                                break;
                            tpl = tpl->parentClass;
                        }
                        if (tpl && tpl->instantiated)
                        {
                            TemplateDataInstantiate(sp2, FALSE, FALSE);
                        }
                    }
                    if (sp3 && sp3->mainsym)
                        sp3 = sp3->mainsym;
                    if (sp4 && sp4->mainsym)
                        sp4 = sp4->mainsym;
                    if (sp3 && sp4 && sp3 != sp4 && classRefCount(sp3, sp4) != 1)
                    {
                        errorsym2(ERR_NOT_UNAMBIGUOUS_BASE, sp3, sp4);
                    }
                    if (!isAccessible(basetype(typ2)->sp, basetype(typ2)->sp, sp2, funcsp, thisptr ? ac_protected : ac_public, FALSE))
                    {
                        errorsym(ERR_CANNOT_ACCESS, sp2);
                    }
                    if (sp2->storage_class == sc_static || sp2->storage_class == sc_external)
                    {
                        EXPRESSION *exp2 = varNode(en_global, sp2);
                        *exp = exprNode(en_void, *exp, exp2);
                    }
                    else
                    {
                        EXPRESSION *offset = intNode(en_c_i, sp2->offset);
                        if (sp2->parentClass != basetype(typ2)->sp)
                        {
                            *exp = baseClassOffset(sp2->parentClass, basetype(typ2)->sp, *exp);
                        }
                        *exp = exprNode(en_structadd, *exp, offset);
                        do
                        {
                            if (basetype(sp3->tp)->type == bt_union)
                            {
                                offset->unionoffset = TRUE;
                                break;
                            }
                            if (sp3 != sp2 && ispointer(sp3->tp))
                                break;
                            sp3 = sp3->parent;
                        } while (sp3);
                    }
                    if (tpb->hasbits)
                    {
                        (*exp) = exprNode(en_bits, *exp, 0);
                        (*exp)->bits = tpb->bits;
                        (*exp)->startbit = tpb->startbit;
                    }
                    deref(*tp, exp);
                    (*exp)->v.sp = sp2; // caching the member symbol in the enode for constexpr handling
                    if (isatomic(basetp))
                    {
                        // note this won't work in C++ because of offset2...
                        (*exp)->isatomic = TRUE;
                        if (needsAtomicLockFromType(*tp))
                            (*exp)->lockOffset = basetp->size - ATOMIC_FLAG_SPACE - sp2->offset;
                    }
                    
                }
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
            TYPE *p = Alloc(sizeof(TYPE));
            p->type = bt_const;
            p->btp = *tp;
            p->size = p->btp->size;
            (*tp) = p;
        }
        if (isvolatile(typein) && !isvolatile(*tp))
        {
            TYPE *p = Alloc(sizeof(TYPE));
            p->type = bt_volatile;
            p->btp = *tp;
            p->size = p->btp->size;
            (*tp) = p;
        }
    }
    return lex;
}
static LEXEME *expression_bracket(LEXEME *lex, SYMBOL *funcsp, TYPE **tp, EXPRESSION **exp, BOOL noinline, BOOL packable)
{
    TYPE *tp2;
    EXPRESSION *expr2 = NULL;
    lex = getsym();
    if (cparams.prm_cplusplus && MATCHKW(lex, begin))
    {
        INITLIST *args = NULL;
        lex = getInitList(lex, funcsp, &args);
        if (cparams.prm_cplusplus && insertOperatorFunc(ovcl_openbr, openbr,
                               funcsp, tp, exp, NULL, NULL, args, noinline))
        {
        }
        else
        {
            error(ERR_CANNOT_USE_INIT_LIST_WITH_BUILTIN_BRACKET);
        }
                
    }
    else
    {
        lex = expression_comma(lex, funcsp, NULL, &tp2, &expr2, FALSE, noinline, FALSE, packable);
        if (tp2)
        {
            if (cparams.prm_cplusplus && insertOperatorFunc(ovcl_openbr, openbr,
                                   funcsp, tp, exp, tp2, expr2, NULL, noinline))
            {
            }
            else if (isvoid(tp2) || isvoid(*tp))
                error(ERR_NOT_AN_ALLOWED_TYPE);
            else if (basetype(tp2)->type == bt_memberptr || basetype(tp2)->type == bt_memberptr)
                error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
            else if (basetype(tp2)->scoped || basetype(*tp)->scoped)
                error(ERR_SCOPED_TYPE_MISMATCH);
            else if (ispointer(*tp))
            {
                if (!isint(tp2))
                    if (ispointer(tp2))
                        error(ERR_NONPORTABLE_POINTER_CONVERSION);
                    else
                        error(ERR_NEED_INTEGER_EXPRESSION);
                /* unreference pointer to VLA */
                if (!(*tp)->vla && basetype(basetype(*tp)->btp)->vla)
                    (*tp) = basetype(*tp)->btp;
                if ((*tp)->vla)
                {
                    int soa = getSize(bt_pointer);
                    int sou = getSize(bt_unsigned);
                    EXPRESSION *vlanode = varNode(en_auto, (*tp)->sp);
                    EXPRESSION *exp1 = exprNode(en_add, vlanode, intNode(en_c_i, soa + sou *(1 + (*tp)->vlaindex)));
                    deref(&stdint, &exp1);
                    cast(&stdpointer, &expr2);
                    exp1 = exprNode(en_umul, exp1, expr2);
                    *exp = exprNode(en_add, *exp, exp1);
                    *tp = basetype(*tp)->btp;
                }
                else
                {
                    EXPRESSION *exp1 = NULL;
                    *tp = basetype(*tp)->btp;
                    cast( &stdint, &expr2);
                    exp1 = exprNode(en_umul, expr2, 
                                   intNode(en_c_i, (*tp)->size + (*tp)->arraySkew));
                    *exp = exprNode(en_add, *exp, exp1);
                }
                if (!(*tp)->array && !(*tp)->vla)
                    deref(*tp, exp);
            }
            else if (ispointer(tp2))
            {
                if (!isint(*tp))
                    if (ispointer(*tp))
                        error(ERR_NONPORTABLE_POINTER_CONVERSION);
                    else
                        error(ERR_NEED_INTEGER_EXPRESSION);
                if (tp2->vla)
                {
                    int soa = getSize(bt_pointer);
                    int sou = getSize(bt_unsigned);
                    EXPRESSION *vlanode = varNode(en_auto, tp2->sp);
                    expr2 = exprNode(en_add, expr2, intNode(en_c_i, soa + sou *( 1 + tp2->vlaindex)));
                    deref(&stdint, exp);
                    *exp = exprNode(en_umul, expr2, *exp);
                    deref(&stdpointer, &vlanode);
                    *exp = exprNode(en_add, *exp, vlanode);
                    *tp = basetype(tp2)->btp;
                }
                else
                {
                    EXPRESSION *exp1 = NULL;
                    *tp = basetype(tp2)->btp;
                    cast(&stdint, exp);
                    exp1 = exprNode(en_umul, *exp, 
                                   intNode(en_c_i, (*tp)->size + (*tp)->arraySkew));
                    *exp = exprNode(en_add, expr2, exp1);
                }
                if (!(*tp)->array && !(*tp)->vla)
                    deref(*tp, exp);
            }
            else
                error(ERR_DEREF);		
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
    return lex;
}
static void checkArgs(FUNCTIONCALL *params, SYMBOL *funcsp)
{
    HASHREC *hr = basetype(params->functp)->syms->table[0];
    INITLIST *list = params->arguments;
    BOOL matching = TRUE;
    BOOL tooshort = FALSE;
    BOOL toolong = FALSE;
    BOOL noproto = params->sp ? params->sp->oldstyle : FALSE;
    int argnum = 0;
 
    if (hr && ((SYMBOL *)hr->p)->thisPtr)
        hr = hr->next;   
    if (!hr)
    {
        matching = FALSE;
        noproto = TRUE;
    }
    if (noproto)
    {
        if (params->sp->hasproto)
            noproto = FALSE;
//		else
//			params->sp->hasproto = TRUE;
    }
    if (hr && ((SYMBOL *)hr->p)->tp->type == bt_void)
    {
        if (list)
            errorsym(ERR_FUNCTION_TAKES_NO_PARAMETERS, params->sp);
    }
    else while (hr || list)
    {
        TYPE *dest = NULL;
        SYMBOL *decl = NULL;
        if (!hr || ((SYMBOL *)hr->p)->tp->type != bt_any)
        {               
            argnum++;
            if (matching && hr)
            {
                decl = (SYMBOL *)hr->p;
                if (!decl->tp)
                    noproto = TRUE;
                else if (decl->tp->type == bt_ellipse)
                {
                    matching = FALSE;
                    decl = NULL;
                    hr = NULL;
                    if (!list)
                        break;
                }
            }
            if (matching)
            {
                if (!decl)
                    toolong = TRUE;
                else if (!list)
                    tooshort = TRUE;
                else
                {
                    if (isref(decl->tp))
                    {
                        TYPE *tpb = basetype(basetype(decl->tp)->btp);
                        TYPE *tpd = list->tp;
                        list->byRef = TRUE;
                        if (isref(tpd))
                            tpd = basetype(tpd)->btp;
                        tpd = basetype(tpd);
                        if (isstructured(tpb) && isstructured(tpd))
                        {
                            SYMBOL *base = basetype(tpb)->sp;
                            SYMBOL *derived = basetype(tpd)->sp;
                            if (base != derived && classRefCount(base, derived) != 1)
                                errortype(ERR_CANNOT_CONVERT_TYPE, list->tp, decl->tp);
                            if (isAccessible(derived, derived, base, funcsp, ac_public, FALSE))
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
                        if (!comparetypes(list->tp, decl->tp, FALSE))
                        {
                            if (basetype(decl->tp)->type != bt_memberptr)
                                errorarg(ERR_TYPE_MISMATCH_IN_ARGUMENT, argnum, decl, params->sp);
                        }
                        else if (assignDiscardsConst(decl->tp, list->tp))
                            if (cparams.prm_cplusplus)
                                errortype(ERR_CANNOT_CONVERT_TYPE, list->tp, decl->tp);
                            else
                                error(ERR_SUSPICIOUS_POINTER_CONVERSION);
                        else if (ispointer(decl->tp))
                        {
                            if (!ispointer(list->tp) && isarithmeticconst(list->exp) && !isconstzero(decl->tp, list->exp))
                                error(ERR_NONPORTABLE_POINTER_CONVERSION);
                            else if (!comparetypes(decl->tp, list->tp, TRUE))
                                if (!isconstzero(list->tp, list->exp))
                                    if (!isvoidptr(decl->tp) && !isvoidptr(list->tp))
                                        if (cparams.prm_cplusplus)
                                        {
                                            TYPE *tpb = basetype(decl->tp)->btp;
                                            TYPE *tpd = basetype(list->tp)->btp;
                                            if (isstructured(tpb) && isstructured(tpd))
                                            {
                                                SYMBOL *base = basetype(tpb)->sp;
                                                SYMBOL *derived = basetype(tpd)->sp;
                                                if (base != derived && classRefCount(base, derived) != 1)
                                                    errortype(ERR_CANNOT_CONVERT_TYPE, list->tp, decl->tp);
                                                if (isAccessible(derived, derived, base, funcsp, ac_public, FALSE))
                                                    errorsym(ERR_CANNOT_ACCESS, base);
                                            }
                                            else
                                            {
                                                errortype(ERR_CANNOT_CONVERT_TYPE, list->tp, decl->tp);
                                            }
                                        }
                                        else
                                            error(ERR_SUSPICIOUS_POINTER_CONVERSION);
                        } 
                        else if (ispointer(list->tp))
                            if (!isvoidptr(list->tp))
                                error(ERR_NONPORTABLE_POINTER_CONVERSION);
                    }
                      dest = decl->tp;
                }
            }
            else if (isint(list->tp))
            {
                if (basetype(list->tp)->type <= bt_int)
                    dest = &stdint;
                else
                    cast(list->tp, &list->exp);
            }
            else if (isfloat(list->tp))
            {
                if (basetype(list->tp)->type < bt_double)
                    dest = &stddouble;
                else
                    cast(list->tp, &list->exp);
            }
            if (dest && basetype(dest)->type != bt_memberptr && (!list || !comparetypes(dest, list->tp, TRUE)))
            {
                cast(basetype(dest), &list->exp);
                list->tp = dest;
            }
            else if (dest && list && basetype(dest)->type == bt_enum)
            {
                // this needs to be revisited to get proper typing in C++
                cast(&stdint, &list->exp);
            }
        }
        if (hr)
            hr = hr->next;
        if (list)
        {
            list = list->next;
        }
    }
    if (noproto)
        errorsym(ERR_CALL_FUNCTION_NO_PROTO, params->sp);
    else if (toolong)
        errorsym(ERR_PARAMETER_LIST_TOO_LONG, params->sp);
    else if (tooshort)
        errorsym(ERR_PARAMETER_LIST_TOO_SHORT, params->sp);
}
static LEXEME *getInitInternal(LEXEME *lex, SYMBOL *funcsp, INITLIST **lptr, enum e_kw finish, BOOL allowNesting, BOOL allowPack, BOOL toErr)
{
    *lptr = NULL;
    lex = getsym(); /* past ( */
    while (!MATCHKW(lex,finish))
    {
        INITLIST *p = Alloc(sizeof(INITLIST));
        if (MATCHKW(lex, begin))
        {
            lex = getInitInternal(lex, funcsp, &p->nested, end, FALSE, FALSE, FALSE);
            *lptr = p;
            lptr = &(*lptr)->next;
            if (!allowNesting)
                error(ERR_CANNOT_USE_INIT_LIST);
        }
        else
        {
            LEXEME *start = lex;
            lex = expression_assign(lex, funcsp, NULL, &p->tp, &p->exp, FALSE, FALSE, FALSE, TRUE);
            if (p->tp && isvoid(p->tp))
                error(ERR_NOT_AN_ALLOWED_TYPE);
            optimize_for_constants(&p->exp);
            assignmentUsages(p->exp, FALSE);
            if (p->tp)
            {
                
                if (allowPack && cparams.prm_cplusplus && MATCHKW(lex, ellipse))
                {
                    // lose p
                    lex = getsym();
                    checkPackedExpression(p->exp);  
                    // this is going to presume that the expression involved
                    // is not too long to be cached by the LEXEME mechanism.          
                    lptr = expandPackedInitList(lptr, funcsp, start, p->exp);
                }
                else
                {
                    if (toErr)
                        checkUnpackedExpression(p->exp);
                    *lptr = p;
                    lptr = &(*lptr)->next;
                }
            }
            else
            {
                error(ERR_IDENTIFIER_EXPECTED);
                errskim(&lex, finish == closepa ? skim_closepa : skim_end);
            }
        }
        if (!MATCHKW(lex, comma))
        {
            break;
        }
        lex = getsym();
        if (MATCHKW(lex, finish))
            error(ERR_IDENTIFIER_EXPECTED);
    }
    if (!needkw(&lex, finish))
    {
        errskim(&lex, finish == closepa ? skim_closepa : skim_end);
        skip(&lex, finish);
    }
	return lex;
}
LEXEME *getInitList(LEXEME *lex, SYMBOL *funcsp, INITLIST **owner)
{
    lex = getInitInternal(lex, funcsp, owner, end, FALSE, TRUE, TRUE);
}
LEXEME *getArgs(LEXEME *lex, SYMBOL *funcsp, FUNCTIONCALL *funcparams, enum e_kw finish, BOOL allowPack)
{
    LEXEME *rv;
    int old = packIndex;
    packIndex = -1;
    rv = getInitInternal(lex, funcsp, &funcparams->arguments, finish, TRUE,allowPack, TRUE);
    packIndex = old;
    return rv;
}
LEXEME *getMemberInitializers(LEXEME *lex, SYMBOL *funcsp, FUNCTIONCALL *funcparams, enum e_kw finish, BOOL allowPack)
{
    return getInitInternal(lex, funcsp, &funcparams->arguments, finish, TRUE,allowPack, FALSE);
}
void DerivedToBase(TYPE *tpn, TYPE *tpo, EXPRESSION **exp)
{
    if (isref(tpn))
        tpn = basetype(tpn)->btp;
    if (isref(tpo))
        tpo = basetype(tpo)->btp;
    if (isstructured(tpn) && isstructured(tpo))
    {
        SYMBOL *spn = basetype(tpn)->sp;
        SYMBOL *spo = basetype(tpo)->sp;
        if (spo != spn)
        {
            int n = classRefCount(spn, spo);
            if (n == 1)
            {
                // derived to base
                EXPRESSION *v = Alloc(sizeof(EXPRESSION));
                v->type = en_c_i;
                v = baseClassOffset(spn, spo, v);
                optimize_for_constants(&v);
                if (v->type == en_c_i) // check for no virtual base
                {
//                    if (isAccessible(spo, spo, spn, NULL, ac_public, FALSE))
                    {
                        SYMBOL *sp = anonymousVar(sc_auto, &stdpointer);
                        EXPRESSION *var = exprNode(en_l_p, varNode(en_auto, sp), NULL);
                        EXPRESSION *asn = exprNode(en_assign, var, *exp);
                        EXPRESSION *left = exprNode(en_add, var, v);
                        EXPRESSION *right = var;
                        insert(sp, localNameSpace->syms);
                        *exp = exprNode(en_cond, var, exprNode(en_void, left, right));
                        *exp = exprNode(en_void, asn, *exp);
                        
                        return;
                    }
                }
            }
            errortype(ERR_CANNOT_CONVERT_TYPE, tpo, tpn);
        }
    }
}
void AdjustParams(HASHREC *hr, INITLIST **lptr, BOOL operands, BOOL noinline)
{
    if (hr && ((SYMBOL *)hr->p)->thisPtr)
        hr = hr->next;
    while (hr && (*lptr || ((SYMBOL *)hr->p)->init != NULL))
    {
        SYMBOL *sym= (SYMBOL *)hr->p;
        EXPRESSION *exp = NULL;
        INITLIST *p;
        if (!*lptr)
        {
            EXPRESSION *p = sym->init->exp;
            optimize_for_constants(&p);
            *lptr = Alloc(sizeof(INITLIST));
            (*lptr)->exp = p;
            (*lptr)->tp = sym->tp;
        }        
        p = *lptr;
        if (cparams.prm_cplusplus)
        {
            BOOL done = FALSE;
            if (!p->tp)
            {
                // initlist
                INITLIST *pinit = p->nested;
                if (isstructured(sym->tp) || isref(sym->tp) && isstructured(basetype(sym->tp)->btp))
                {
                    TYPE *stype = sym->tp;
                    SYMBOL *sp;
                    EXPRESSION *thisptr;
                    if (isref(stype))
                        stype = basetype(stype)->btp;
                    sp = anonymousVar(theCurrentFunc ? sc_auto : sc_localstatic, stype);
                    thisptr = varNode(theCurrentFunc ? en_auto : en_label, sp);
                    if (!theCurrentFunc)
                    {
                        sp->label = nextLabel++;
                        insertInitSym(sp);
                    }
                    if (stype->sp->trivialCons)
                    {
                        INITIALIZER *init = NULL, **it = &init;
                        HASHREC *hr = stype->syms->table[0];
                        while (pinit && hr)
                        {
                            SYMBOL *shr = (SYMBOL *)hr->p;
                            if (ismemberdata(shr))
                            {
                                initInsert(it, pinit->tp, pinit->exp, shr->offset, FALSE);
                                it = &(*it)->next;
                                pinit = pinit->next;
                            }
                            hr = hr->next;
                        }
                        p->exp = convertInitToExpression(stype, NULL, theCurrentFunc, init, thisptr, noinline);
                        if (!isref(sym->tp))
                            sp->stackblock = TRUE;
                        done = TRUE;
                    }
                    else
                    {
                        FUNCTIONCALL *funcparams = Alloc(sizeof(FUNCTIONCALL));
                        TYPE *ctype = sp->tp;
                        EXPRESSION *dexp = thisptr;
                        funcparams->arguments = pinit;
                        p->exp = thisptr;
                        callConstructor(&ctype, &p->exp, funcparams, FALSE, NULL, TRUE, TRUE, noinline, TRUE, FALSE);
                        if (!isref(sym->tp))
                        {
                            sp->stackblock = TRUE;
                        }
                        else
                        {
                            callDestructor(stype->sp, &dexp, NULL, TRUE, noinline, FALSE);
                            if (dexp)
                                p->dest = dexp;
                        }
                        done = TRUE;
                    }
                    p->tp = sym->tp;
                }
                else if (ispointer(sym->tp))
                {
                    INITIALIZER *init = NULL, **it = &init;
                    SYMBOL *sp = anonymousVar(theCurrentFunc ? sc_auto : sc_localstatic, sym->tp);
                    EXPRESSION *thisptr = varNode(theCurrentFunc ? en_auto : en_label, sp);
                    int n = 0;
                    TYPE *btp = sym->tp;
                    while (isarray(btp))
                        btp = basetype(btp)->btp;
                    if (!theCurrentFunc)
                    {
                        sp->label = nextLabel++;
                        insertInitSym(sp);
                    }
                    if (!isarray(sym->tp))
                    {
                        TYPE *gtype = Alloc(sizeof(TYPE));
                        INITLIST *xx = pinit;
                        *gtype = *sym->tp;
                        gtype->array = TRUE;
                        while (xx)
                        {
                            n ++;
                            xx = xx->next;
                        }
                        sym->tp = gtype;
                        sym->tp->size = btp->size * n;
                    }
                    n = 0;
                    while (pinit)
                    {
                        initInsert(it, pinit->tp, pinit->exp, n, FALSE);
                        it = &(*it)->next;
                        n += btp->size;
                        pinit = pinit->next;
                    }
                    p->exp = convertInitToExpression(sym->tp, NULL, theCurrentFunc, init, thisptr, noinline);
                    p->tp = sym->tp;
                    done = TRUE;
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
            if (!done)
            {
                if (isstructured(sym->tp))
                {
                    TYPE *tpx = p->tp;
                    if (isref(tpx))
                        tpx = basetype(tpx)->btp;
                    // use constructor or conversion function and push on stack ( no destructor)
                    if (p->exp->type == en_func && comparetypes(sym->tp, tpx, TRUE))
                    {
                        EXPRESSION **exp = NULL;
                        SYMBOL *esp;
                        EXPRESSION *consexp;
                        TYPE *tp;
                        if (p->exp->v.func->returnEXP)
                        {
                            exp = &p->exp->v.func->returnEXP;
                            tp = p->exp->v.func->returnSP->tp;
                        }
                        else
                        {
                            exp = &p->exp->v.func->thisptr;
                            tp = p->exp->v.func->thistp;
                        }
                        if (exp)
                        {
                            esp = anonymousVar(sc_auto, tp); // sc_parameter to push it...
                            esp->stackblock = TRUE;
                            consexp = varNode(en_auto, esp);
                            *exp = consexp;
                        }
                    }
                    else
                    {
                        TYPE *ctype = sym->tp;
                        FUNCTIONCALL *funcparams = Alloc(sizeof(FUNCTIONCALL));
                        INITLIST *arg = Alloc(sizeof(INITLIST));
                        SYMBOL *esp = anonymousVar(sc_auto, sym->tp); // sc_parameter to push it...
                        EXPRESSION *consexp = varNode(en_auto, esp);
                        EXPRESSION *destexp = consexp;
                        EXPRESSION *old = p->exp;
                        esp->stackblock = TRUE;
                        arg->exp = p->exp;
                        arg->tp = p->tp;
                        funcparams->arguments = arg;
                        callConstructor(&ctype, &consexp, funcparams, FALSE, NULL, TRUE, TRUE, noinline, TRUE, FALSE);
                        p->exp=consexp;
                        if (p->exp->type == en_func)
                        {
                            BOOL ref = FALSE;
                            SYMBOL *spx = p->exp->v.func->sp;
                            TYPE *tpx = basetype(spx->tp);
                            TYPE *tpx1, *tpx2;
                            HASHREC *hr1;
                            if (spx->castoperator)
                            {
                                tpx1 = spx->parentClass->tp;
                                tpx2 = tpx->btp;
                            }
                            else
                            {
                                tpx1 = ((SYMBOL *)tpx->syms->table[0]->next->p)->tp;
                                tpx2 = spx->parentClass->tp;
                            }
                            esp->tp = tpx2;
                            if (!comparetypes(basetype(sym->tp), basetype(tpx2), TRUE))
                            {
                                esp->stackblock = FALSE; // make it a real variable
                                insert(esp, localNameSpace->syms);
                                funcparams = Alloc(sizeof(FUNCTIONCALL));
                                arg = Alloc(sizeof(INITLIST));
                                esp = anonymousVar(sc_auto, sym->tp); // sc_parameter to push it...
                                esp->stackblock = TRUE;
                                consexp = varNode(en_auto, esp);
                                // have to do another constructor
    //                            DerivedToBase(sym->tp, tpx2, &p->exp);
                                arg->exp = p->exp;
                                arg->tp = tpx2;
                                funcparams->arguments = arg;
                                ctype = sym->tp;
                                callConstructor(&ctype, &consexp, funcparams, FALSE, NULL, TRUE, FALSE, noinline, TRUE, FALSE);
                                p->exp = consexp;
                                callDestructor(basetype(tpx2)->sp, &destexp, NULL, TRUE, noinline, FALSE);
                                (*lptr)->dest = destexp;
                            }
                            else if (old->type == en_func && old->v.func->returnEXP)
                            {
                                destexp = old->v.func->returnEXP;
                                callDestructor(old->v.func->returnSP, &destexp, NULL, TRUE, noinline, FALSE);
                                (*lptr)->dest = destexp;
                            }
                        }
                    }
                    p->tp = sym->tp;
                }
                else if (isref(sym->tp))
                {
                    if (isstructured(basetype(sym->tp)->btp))
                    {
                        TYPE *tpx = p->tp;
                        if (isref(tpx))
                            tpx = basetype(tpx)->btp;
                        if (!isconst(basetype(sym->tp)->btp) && isconst(tpx) || !comparetypes(sym->tp, tpx, TRUE) 
                            && !classRefCount(basetype(basetype(sym->tp)->btp)->sp, basetype(tpx)->sp))
                        { 
                            // make temp via constructor or conversion function
                            SYMBOL *esp = anonymousVar(sc_auto,basetype(sym->tp)->btp);
                            EXPRESSION *consexp = varNode(en_auto, esp);
                            EXPRESSION *destexp = consexp;
                            TYPE *ctype = basetype(sym->tp)->btp;
                            FUNCTIONCALL *funcparams = Alloc(sizeof(FUNCTIONCALL));
                            INITLIST *arg = Alloc(sizeof(INITLIST));
                            insert(esp, localNameSpace->syms);
                            arg->exp = p->exp;
                            arg->tp = basetype(p->tp);
                            funcparams->arguments = arg;
                            p->exp = consexp;
                            callConstructor(&ctype, &p->exp, funcparams, FALSE, NULL, TRUE, TRUE, noinline, TRUE, FALSE); 
                            if (p->exp->type == en_func)
                            {
                                SYMBOL *spx = p->exp->v.func->sp;
                                TYPE *tpx = basetype(spx->tp);
                                TYPE *tpx1, *tpx2;
                                HASHREC *hr1;
                                if (spx->castoperator)
                                {
                                    tpx1 = spx->parentClass->tp;
                                    tpx2 = tpx->btp;
                                    if (isref(tpx2))
                                        tpx2 = basetype(tpx2)->btp;
                                }
                                else
                                {
                                    tpx1 = ((SYMBOL *)tpx->syms->table[0]->next->p)->tp;
                                    tpx2 = spx->parentClass->tp;
                                }
                                esp->tp = tpx1; // guaranteed to be a structured type or reference to one
                                DerivedToBase(tpx1, p->tp, &p->exp->v.func->thisptr);
                                if (isstructured(tpx2) || isref(tpx2) && isstructured(basetype(tpx2)->btp))
                                    DerivedToBase(sym->tp, tpx2, &p->exp);
                                else
                                    cast(sym->tp, &p->exp);
                            }
                        }
                        else
                        {
                            if (!comparetypes(sym->tp, p->tp, TRUE))
                                DerivedToBase(sym->tp, p->tp, &p->exp);
                        }
                    }
                    else if (comparetypes(sym->tp, p->tp, TRUE))
                    {
                        if (isarithmeticconst(p->exp) || basetype(sym->tp)->type != bt_rref && !isconst(basetype(sym->tp)->btp) && isconst(p->tp))
                        {
                            // make numeric temp and perform cast
                            p->exp = createTemporary(sym->tp, p->exp);
                        }
                        else
                        {
                            // pass address
                            EXPRESSION *exp = p->exp;
                            while (castvalue(exp) || exp->type == en_not_lvalue)
                                exp = exp->left;
                            if (exp->type != en_l_ref)
                            {
                                if (!lvalue(exp))
                                {
                                    // make numeric temp and perform cast
                                    exp = createTemporary(sym->tp, exp);
                                }
                                exp = exp->left; // take address
                                p->exp = exp;
                            }
                        }
                    }
                    else if (isstructured(p->tp))
                    {
                        // arithmetic or pointer
                        TYPE *etp = basetype(sym->tp)->btp;
                        if (cppCast(p->tp, &etp, &p->exp, noinline))
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
                    // arithmetic or pointer
                    TYPE *etp = sym->tp;
                    if (cppCast(p->tp, &etp, &p->exp, noinline))
                        p->tp = etp;
                }
                else if (ispointer(sym->tp) && ispointer(p->tp))
                {
                    // handle base class conversion
                    TYPE *tpb = basetype(sym->tp)->btp;
                    TYPE *tpd = basetype(p->tp)->btp;
                    if (!comparetypes(basetype(tpb), basetype(tpd), TRUE))
                    {
                        if (isstructured(tpb) && isstructured(tpd))
                        {
                            DerivedToBase(tpb, tpd, &p->exp);
                        }
                        p->tp = sym->tp;
                    }
                }
                else if (basetype(sym->tp)->type == bt_memberptr)
                {
                    if (p->exp->type == en_memberptr)
                    {
                        int lbl = dumpMemberPtr(p->exp->v.sp, sym->tp, TRUE);
                        p->exp = intNode(en_labcon, lbl);
                        p->exp = exprNode(en_stackblock, p->exp, NULL);
                        p->exp->size = sym->tp->size;
                    }
                    else if (isconstzero(p->tp, p->exp) || p->exp->type == en_nullptr)
                    {
                        EXPRESSION *dest = createTemporary(sym->tp, NULL);
                        p->exp = exprNode(en_blockclear, dest, NULL);
                        p->exp->size = sym->tp->size;
                        p->exp = exprNode(en_void, p->exp, dest);
                        p->exp = exprNode(en_stackblock, p->exp, NULL);
                        p->exp->size = sym->tp->size;
                        p->tp = sym->tp;
                    }
                    else if (p->exp->type == en_func && p->exp->v.func->returnSP)
                    {
                        SYMBOL *esp = anonymousVar(sc_auto, sym->tp);
                        EXPRESSION *dest = varNode(en_auto, esp);
                        esp->stackblock = TRUE;
                        p->exp->v.func->returnSP->allocate = FALSE;
                        p->exp->v.func->returnEXP = dest;
                        p->exp->size = sym->tp->size;
                        /*
                        EXPRESSION *dest = createTemporary(sym->tp, NULL);
                        p->exp->v.func->returnSP->allocate = FALSE;
                        p->exp->v.func->returnEXP = dest;
                        p->exp = exprNode(en_void, p->exp, dest) ;
                        p->exp = exprNode(en_stackblock, p->exp, NULL);
                        p->exp->size = sym->tp->size;
                        */
                    }
                    else
                    {
                        p->exp = exprNode(en_stackblock, p->exp, NULL);
                        p->exp->size = sym->tp->size;
                    }
                    p->tp->size = sym->tp->size;
                }
            }
        }
        else
        {
            // legacy c language support
            if (isstructured(p->tp))
            {
                p->exp = exprNode(en_stackblock, p->exp, NULL);
                p->exp->size = p->tp->size;
            }
        }            
        hr = hr->next;
        lptr = &(*lptr)->next;
    }
    while (*lptr) // take care of elliptical arguments and arguments without a prototype
    {
        INITLIST *p = *lptr;
        if (isstructured(p->tp))
        {
            p->exp = exprNode(en_stackblock, p->exp, NULL);
            p->exp->size = p->tp->size;
        }
        else if (p->tp->type == bt_float)
        {
            cast(&stddouble, &p->exp);
        }
        lptr = &(*lptr)->next;
    }
}
LEXEME *expression_arguments(LEXEME *lex, SYMBOL *funcsp, TYPE **tp, EXPRESSION **exp, BOOL noinline)
{
    TYPE *tp_cpp = *tp;
    EXPRESSION *exp_cpp = *exp;
    FUNCTIONCALL *funcparams;
    INITLIST **lptr ;
    EXPRESSION *exp_in = *exp;
    BOOL operands = FALSE;
    BOOL hasThisPtr = FALSE;
    if (exp_in->type != en_func || isfuncptr(*tp))
    {
        TYPE *tpx = *tp;
        SYMBOL *sym;
        funcparams = Alloc(sizeof(FUNCTIONCALL));
        if (ispointer(tpx))
            tpx = basetype(tpx)->btp;
        sym = basetype(tpx)->sp;
        if (sym)
        {
            funcparams->sp = sym;
            funcparams->functp = sym->tp;
            funcparams->fcall = *exp;
            *exp = Alloc(sizeof(EXPRESSION));
            (*exp)->type = en_func;
            (*exp)->v.func = funcparams;
        }
        else
            error(ERR_CALL_OF_NONFUNCTION);
    }
    else {
        SYMBOL *ss = getStructureDeclaration();
        funcparams = exp_in->v.func;
        hasThisPtr = funcparams->thisptr != NULL;
        if (ss)
        {
            funcparams->functp = ss->tp;
        }
    }
    if (lex)
    {
        lex = getArgs(lex, funcsp, funcparams, closepa, TRUE);
    }
    if (*tp)
        getFunctionSP(tp);
    if (cparams.prm_cplusplus && funcparams->sp)
    {
        SYMBOL *sp = NULL;
        // add in this ptr
        if (!funcparams->thisptr && funcparams->sp->parentClass && !isfuncptr(funcparams->sp->tp))
        {
            TYPE *tp = Alloc(sizeof(TYPE)), *tpx;
            funcparams->thisptr = getMemberBase(funcparams->sp, NULL, funcsp, FALSE);
            funcparams->thistp = tp;
            tp->type = bt_pointer;
            tp->size = getSize(bt_pointer);
            tpx = tp;
            if (funcsp)
            {
                if (isconst(funcsp->tp))
                {
                    tpx = tpx->btp = Alloc(sizeof(TYPE));
                    tpx->size = basetype(funcparams->sp->parentClass->tp)->size;
                    tpx->type = bt_const;
                }
                if (isvolatile(funcsp->tp))
                {
                    tpx = tpx->btp = Alloc(sizeof(TYPE));
                    tpx->size = basetype(funcparams->sp->parentClass->tp)->size;
                    tpx->type = bt_volatile;
                }
            }
            tpx->btp = funcparams->sp->parentClass->tp;
        }
        // we may get here with the overload resolution already done, e.g.
        // for operator or cast function calls...
        if (funcparams->sp->storage_class == sc_overloads)
        {
            // note at this pointer the arglist does NOT have the this pointer,
            // it will be added after we select a member function that needs it.
            funcparams->ascall = TRUE;    
            sp = GetOverloadedFunction(tp, &funcparams->fcall, funcparams->sp, funcparams, NULL, TRUE, FALSE);
            if (sp)
            {
                sp->throughClass = funcparams->sp->throughClass;
                funcparams->sp = sp;
                if (funcparams->noobject && (sp->storage_class == sc_member || sp->storage_class == sc_virtual))
                    errorsym(ERR_USE_DOT_OR_POINTSTO_TO_CALL, sp);
            }
        }
        else
        {
            operands = funcparams->sp->storage_class != sc_member 
                        && funcparams->sp->storage_class != sc_virtual && funcparams->thisptr;
            if (!isExpressionAccessible(funcparams->sp, funcsp, FALSE))
                errorsym(ERR_CANNOT_ACCESS, funcparams->sp);		
        }
        if (sp)
        {
            *tp = sp->tp;
            if (!isExpressionAccessible(sp, funcsp, FALSE))
                errorsym(ERR_CANNOT_ACCESS, sp);		
            if (funcsp && isconst(funcsp->tp)) // the fact it is const makes it a member
            {
                if (!isconst(*tp) && funcparams->sp->parentClass == funcsp->parentClass)
                    errorsym(ERR_NON_CONST_FUNCTION_CALLED_FOR_CONST_OBJECT, funcparams->sp);
            }
            if (funcparams->thistp && isconst(basetype(funcparams->thistp)->btp))
                if (!isconst(*tp))
                    errorsym(ERR_NON_CONST_FUNCTION_CALLED_FOR_CONST_OBJECT, funcparams->sp);
        }
    }

    if (!isfunction(*tp))
    {
        // might be operator ()
        if (cparams.prm_cplusplus)
        {
            EXPRESSION *exp_arg = exp_cpp;
            TYPE *tp_arg = tp_cpp;
            if (insertOperatorParams(funcsp, &tp_cpp, &exp_cpp, funcparams, noinline))
            {
                *tp = tp_cpp;
                *exp = exp_cpp;
            }
        }
    }
    {
        HASHTABLE *temp = basetype(*tp)->syms;
        if (temp)
        {
            HASHREC *hr = temp->table[0];
            if (funcparams->sp && funcparams->sp->storage_class != sc_member && funcparams->sp->storage_class != sc_virtual)
            {
                if (operands)
                {
                    INITLIST *al = Alloc(sizeof(INITLIST));
                    al->exp = funcparams->thisptr;
                    al->tp = funcparams->thistp;
                    al->next = funcparams->arguments;
                    funcparams->arguments = al;
                }
                funcparams->thisptr = NULL;
            }
            else
            {
                if (!getStructureDeclaration() && !ispointer(tp_cpp) && !hasThisPtr)
                    errorsym(ERR_ACCESS_MEMBER_NO_OBJECT, funcparams->sp);
                operands = FALSE;
            }
            lptr = &funcparams->arguments;
            AdjustParams(hr, lptr, operands, noinline);
            if (cparams.prm_cplusplus)
            {
                lptr = &funcparams->arguments;
                while (*lptr)
                {
                    if (isstructured((*lptr)->tp))
                    {
                        EXPRESSION *exp = (*lptr)->exp;
                        if (exp-> type == en_not_lvalue)
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
                if (isstructured(basetype(*tp)->btp) || basetype(basetype(*tp)->btp)->type == bt_memberptr)
                {
                    funcparams->returnSP = anonymousVar(sc_auto, basetype(*tp)->btp);
                    funcparams->returnEXP = varNode(en_auto, funcparams->returnSP);
                }
                funcparams->ascall = TRUE;    
                funcparams->functp = *tp;
                *tp = basetype(*tp)->btp;
                if (isref(*tp))
                    *tp = basetype(*tp)->btp;
                  checkArgs(funcparams, funcsp);
                if (funcparams->sp->constexpression)
                {
                    exp_in = EvaluateConstFunction(funcparams, funcsp);
                    if (exp_in)
                        *exp = exp_in;
                }
                if (!funcparams->novtab && funcparams->sp && funcparams->sp->storage_class == sc_virtual)
                {
                    exp_in = funcparams->thisptr;
                    deref(&stdpointer, &exp_in);
                    exp_in = exprNode(en_add, exp_in, intNode(en_c_i, funcparams->sp->offset));
                    deref(&stdpointer, &exp_in);
                    funcparams->fcall = exp_in;
                }
                else if (!noinline)
                {
                    exp_in = doinline(funcparams, funcsp);
                    if (exp_in)
                        *exp = exp_in;
                }
                if (funcparams->sp && isref(basetype(funcparams->sp->tp)->btp))
                {
                    deref(basetype(basetype(funcparams->sp->tp)->btp)->btp, exp);
                }
            }
            else
            {
                *tp = &stdvoid;
                error(ERR_CALL_OF_NONFUNCTION);
            }
        }
        else
        {
            *tp = &stdvoid;
            error(ERR_CALL_OF_NONFUNCTION);
        }
        
    }
    return lex;
}
static LEXEME *expression_alloca(LEXEME *lex, SYMBOL *funcsp, TYPE **tp, EXPRESSION **exp, BOOL noinline)
{
    lex = getsym();
    if (needkw(&lex, openpa))
    {
        lex = expression_comma(lex, funcsp, NULL, tp, exp, FALSE, noinline, FALSE, FALSE);
        if (*tp)
        {
            if (!isint(*tp))
                error(ERR_NEED_INTEGER_EXPRESSION);
            optimize_for_constants(exp);
        
            funcsp->allocaUsed = TRUE;
            *exp = exprNode(en_alloca, *exp, 0);
            needkw(&lex, closepa);
            *tp = &stdpointer;
        }
        else
        {
            error(ERR_EXPRESSION_SYNTAX);
            *tp = NULL;
        }
    }
    else
    {
        errskim(&lex, skim_closepa);
        skip(&lex, closepa);
        *tp = NULL;
    }
    return lex;
}
static LEXEME *expression_string(LEXEME *lex, SYMBOL *funcsp, TYPE **tp, EXPRESSION **exp)
{
    BOOL wide;
    int elems = 0;
    STRING *data;
    (void)funcsp;
    IncGlobalFlag();
    lex = concatStringsInternal(lex, &data, &elems);
    *exp = stringlit(data);
    DecGlobalFlag();
    if (data->suffix)
    {
        enum e_bt tpb = (*tp)->type;
        SYMBOL *sym;
        char name[512];
        switch(data->strtype)
        {
            default:
            case l_astr:
                tpb = stdcharptr.btp->type;
                break;
            case l_wstr:
                tpb = stdwcharptr.btp->type;
                break;
            case l_ustr:
                tpb = stdchar16tptr.btp->type;
                break;
            case l_Ustr:
                tpb = stdchar32tptr.btp->type;
                break;
        }
        sprintf(name, "%s@%s", overloadNameTab[CI_LIT], data->suffix);
        sym = LookupSym(name);
        if (sym)
        {
            HASHREC *hr = sym->tp->syms->table[0], *hr1;
            SYMBOL *sym1, *sym2;
            while (hr)
            {
                sym1 = (SYMBOL *)hr->p;
                hr1 = sym1->tp->syms->table[0];
                sym2 = (SYMBOL *)hr1->p;
                if (hr1->next && ispointer(sym2->tp))
                    if (isconst(sym2->tp->btp) && basetype(sym2->tp->btp)->type == tpb)
                        break;                
                hr = hr->next;
            }
            if (hr)
            {
                FUNCTIONCALL *f = Alloc(sizeof(FUNCTIONCALL));
                f->sp = sym1;
                f->functp = sym1->tp;
                f->fcall = varNode(en_pc, sym1);
                f->arguments = Alloc(sizeof(INITLIST));
                f->arguments->tp = sym2->tp;
                f->arguments->exp = *exp;
                f->arguments->next = Alloc(sizeof(INITLIST));
                f->arguments->next->tp = &stdunsigned;
                f->arguments->next->exp = intNode(en_c_i, elems);
                *exp = intNode(en_func, 0);
                (*exp)->v.func = f;
                *tp = sym1->tp;
                expression_arguments(NULL, funcsp, tp, exp, FALSE);
                return lex;
            }
        }
        errorstr(ERR_COULD_NOT_FIND_A_MATCH_FOR_LITERAL_SUFFIX, data->suffix);
    }
    *tp = Alloc(sizeof(TYPE));
    (*tp)->type = bt_pointer;
    (*tp)->array = TRUE;
    switch(data->strtype)
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
    (*tp)->size = (elems + 1) * (*tp)->btp->size;
    return lex;	
}
static LEXEME *expression_generic(LEXEME *lex, SYMBOL *funcsp, TYPE **tp, EXPRESSION **exp, BOOL noinline)
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
        TYPE *selectType = NULL;
        EXPRESSION *throwawayExpression = NULL;
        lex = expression_assign(lex, funcsp, NULL, &selectType, &throwawayExpression, FALSE, noinline, FALSE, FALSE);
        if (MATCHKW(lex, comma))
        {
            BOOL dflt = FALSE;
            struct genericHold
            {
                struct genericHold *next;
                TYPE *selector;
                TYPE *type;
                EXPRESSION *exp;
            };
            struct genericHold *list = NULL, **pos = &list;
            struct genericHold *selectedGeneric = NULL;
            while (MATCHKW(lex, comma))
            {
                struct genericHold *next = Alloc(sizeof(struct genericHold)), *scan;
                lex = getsym();
                if (MATCHKW(lex, kw_default))
                {
                    lex = getsym();
                    if (dflt)
                    {
                        error(ERR_GENERIC_TOO_MANY_DEFAULTS);
                    }
                    dflt = TRUE;
                    next->selector = NULL;
                }
                else
                {
                    lex = get_type_id(lex, &next->selector, funcsp, FALSE);
                    if (!next->selector)
                    {
                        error(ERR_GENERIC_MISSING_TYPE);
                        break;
                    }
                }
                if (MATCHKW(lex, colon))
                {
                    lex = getsym();
                    lex = expression_assign(lex, funcsp, NULL, &next->type, &next->exp, FALSE, noinline, FALSE, FALSE);
                    if (!next->type)
                    {
                        error(ERR_GENERIC_MISSING_EXPRESSION);
                        break;
                    }
                    scan = list;
                    while (scan)
                    {
                        if (scan->selector && next->selector && comparetypes(next->selector, scan->selector, TRUE))
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
                        if (selectType && next->selector && comparetypes(next->selector, selectType, TRUE))
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
static BOOL getSuffixedChar(LEXEME *lex, SYMBOL *funcsp, TYPE **tp, EXPRESSION **exp)
{
    char name[512];
    enum e_bt tpb = (*tp)->type;
    SYMBOL *sym;
    sprintf(name, "%s@%s", overloadNameTab[CI_LIT], lex->suffix);
    sym = LookupSym(name);
    if (sym)
    {
        HASHREC *hr = sym->tp->syms->table[0], *hr1;
        SYMBOL *sym1, *sym2;
        while (hr)
        {
            sym1 = (SYMBOL *)hr->p;
            hr1 = sym1->tp->syms->table[0];
            sym2 = (SYMBOL *)hr1->p;
            if (!hr1->next && sym2->tp->type == tpb)
                break;                
            hr = hr->next;
        }
        if (hr)
        {
            FUNCTIONCALL *f = Alloc(sizeof(FUNCTIONCALL));
            f->sp = sym1;
            f->functp = sym1->tp;
            f->fcall = varNode(en_pc, sym1);
            f->arguments = Alloc(sizeof(INITLIST));
            f->arguments->tp = *tp;
            f->arguments->exp = *exp;
            *exp = intNode(en_func, 0);
            (*exp)->v.func = f;
            *tp = sym1->tp;
            expression_arguments(NULL, funcsp, tp, exp, FALSE);
            return TRUE;
        }
    }
    errorstr(ERR_COULD_NOT_FIND_A_MATCH_FOR_LITERAL_SUFFIX, lex->suffix);
    return FALSE;
}
static BOOL getSuffixedNumber(LEXEME *lex, SYMBOL *funcsp, TYPE **tp, EXPRESSION **exp)
{
    char name[512];
    enum e_bt tpb;
    SYMBOL *sym;
    if (lex->type == l_ull)
        tpb  = bt_unsigned_long_long;
    else
        tpb = bt_long_double;
    sprintf(name, "%s@%s", overloadNameTab[CI_LIT], lex->suffix);
    sym = LookupSym(name);
    if (sym)
    {
        // look for parameter of type unsigned long long or long double
        HASHREC *hr = sym->tp->syms->table[0], *hr1;
        SYMBOL *sym1, *sym2;
        while (hr)
        {
            sym1 = (SYMBOL *)hr->p;
            hr1 = sym1->tp->syms->table[0];
            sym2 = (SYMBOL *)hr1->p;
            if (!hr1->next && sym2->tp->type == tpb)
                break;                
            hr = hr->next;
        }
        if (hr)
        {
            FUNCTIONCALL *f = Alloc(sizeof(FUNCTIONCALL));
            f->sp = sym1;
            f->functp = sym1->tp;
            f->fcall = varNode(en_pc, sym1);
            f->arguments = Alloc(sizeof(INITLIST));
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
            expression_arguments(NULL, funcsp, tp, exp, FALSE);
            return TRUE;
        }
        else
        {
            // not found, look for parameter of type const char *
            hr = sym->tp->syms->table[0];
            while (hr)
            {
                TYPE *tpx;
                sym1 = (SYMBOL *)hr->p;
                hr1 = sym1->tp->syms->table[0];
                sym2 = (SYMBOL *)hr1->p;
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
                FUNCTIONCALL *f = Alloc(sizeof(FUNCTIONCALL));
                STRING *data;
                int i;
                f->sp = sym1;
                f->functp = sym1->tp;
                f->fcall = varNode(en_pc, sym1);
                f->arguments = Alloc(sizeof(INITLIST));
                f->arguments->tp = &stdcharptr;
                f->arguments->tp->size = (strlen(lex->litaslit) + 1) * f->arguments->tp->btp->size;
                IncGlobalFlag();
                data = (STRING *)Alloc(sizeof(STRING));
                data->strtype = l_astr;
                data->size = 1;
                data->pointers = Alloc(sizeof(SLCHAR *));
                data->pointers[0] = Alloc(sizeof(SLCHAR));
                data->pointers[0]->count = strlen(lex->litaslit);
                data->pointers[0]->str = Alloc(sizeof(LCHAR) * (data->pointers[0]->count + 1));
                for (i=0; i < data->pointers[0]->count; i++)
                    data->pointers[0]->str[i] = lex->litaslit[i];
                f->arguments->exp = stringlit(data);
                DecGlobalFlag();
                *exp = intNode(en_func, 0);
                (*exp)->v.func = f;
                *tp = sym1->tp;
                expression_arguments(NULL, funcsp, tp, exp, FALSE);
                return TRUE;
            }
        }
    }
    errorstr(ERR_COULD_NOT_FIND_A_MATCH_FOR_LITERAL_SUFFIX, lex->suffix);
    return FALSE;
}
static LEXEME *expression_atomic_func(LEXEME *lex, SYMBOL *funcsp, TYPE **tp, EXPRESSION **exp, BOOL noinline)
{
    enum e_kw kw = KW(lex);
    lex = getsym();
    if (needkw(&lex, openpa))
    {
        if (kw == kw_atomic_kill_dependency)
        {
            lex = expression_assign(lex, funcsp, NULL, tp, exp, FALSE, noinline, FALSE, FALSE);
            if (!*tp)
                error(ERR_EXPRESSION_SYNTAX);
            needkw(&lex, closepa);
        }
        else if (kw == kw_atomic_var_init)
        {
            lex = expression_assign(lex, funcsp, NULL, tp, exp, FALSE, noinline, FALSE, FALSE);
            if (!*tp)
                error(ERR_EXPRESSION_SYNTAX);
            if (MATCHKW(lex, comma)) // atomic_init
            {
                TYPE *tp1;
                EXPRESSION *exp1;
                lex = getsym();
                lex = expression_assign(lex, funcsp, NULL, &tp1, &exp1, FALSE, noinline, FALSE, FALSE);
                if (*tp && tp1)
                {
                    ATOMICDATA *d;
                    d = (ATOMICDATA *)Alloc(sizeof(ATOMICDATA));
                    if (!ispointer(tp1))
                    {
                        error(ERR_DEREF);
                        d->tp = *tp = &stdint;
                    }
                    else
                    {
                        TYPE *tp2 = *tp;
                        d->tp = *tp = basetype(tp1)->btp;
                        tp1 = tp2;
                    }
                    if (!comparetypes(*tp, tp1, FALSE))
                    {
                        error(ERR_INCOMPATIBLE_TYPE_CONVERSION);
                    }
                    d->address = exp1;
                    d->value = *exp;        
                    d->atomicOp = ao_init;
                    *exp = exprNode(en_atomic, NULL , NULL);
                    (*exp)->v.ad = d;
                }
                else
                {
                    error(ERR_EXPRESSION_SYNTAX);
                }
            }
            else // ATOMIC_VAR_INIT
            {
                if (*exp)
                    (*exp)->atomicinit = TRUE; 
            }
            needkw(&lex, closepa);
        }
        else
        {
            TYPE *tpf = NULL, *tpf1;
            ATOMICDATA *d;
            d = (ATOMICDATA *)Alloc(sizeof(ATOMICDATA));
            switch (kw)
            {
                case kw_atomic_flag_test_set:
                    lex = expression_assign(lex, funcsp, NULL, &tpf, &d->flg, FALSE, noinline, FALSE, FALSE);
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
                        lex = expression_assign(lex, funcsp, NULL, &tpf, &d->memoryOrder1, FALSE, noinline, FALSE, FALSE);
                    }
                    else 
                    {
                        tpf = &stdint;
                        d->memoryOrder1 =intNode(en_c_i, mo_seq_cst);
                    }
                    d->atomicOp = ao_flag_set_test;
                    break;
                case kw_atomic_flag_clear:
                    lex = expression_assign(lex, funcsp, NULL, &tpf, &d->flg, FALSE, noinline, FALSE, FALSE);
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
                        lex = expression_assign(lex, funcsp, NULL, &tpf, &d->memoryOrder1, FALSE, noinline, FALSE, FALSE);
                    }
                    else 
                    {
                        tpf = &stdint;
                        d->memoryOrder1 =intNode(en_c_i, mo_seq_cst);
                    }
                    d->atomicOp = ao_flag_clear;
                    *tp = &stdvoid;
                    break;
                case kw_atomic_fence:
                    lex = expression_assign(lex, funcsp, NULL, &tpf, &d->memoryOrder1, FALSE, noinline, FALSE, FALSE);
                    d->atomicOp = ao_fence;
                    *tp = &stdvoid;
                    break;
                case kw_atomic_load:
                    lex = expression_assign(lex, funcsp, NULL, &tpf, &d->address, FALSE, noinline, FALSE, FALSE);
                    if (tpf)
                        if (!ispointer(tpf))
                        {
                            error(ERR_DEREF);
                            d->tp = *tp = &stdint;
                        }
                        else
                        {
                            d->tp = *tp = basetype(tpf)->btp;
                            if (isstructured(*tp))
                                error(ERR_ILL_STRUCTURE_OPERATION);
                        }
    
                    if (needkw(&lex, comma))
                    {
                        lex = expression_assign(lex, funcsp, NULL,&tpf, &d->memoryOrder1, FALSE, noinline, FALSE, FALSE);
                    }
                    else 
                    {
                        tpf = &stdint;
                        d->memoryOrder1 =intNode(en_c_i, mo_seq_cst);
                    }
                    d->atomicOp = ao_load;
                    break;
                case kw_atomic_store:
                    lex = expression_assign(lex, funcsp, NULL, &tpf, &d->address, FALSE, noinline, FALSE, FALSE);
                    if (tpf)
                        if (!ispointer(tpf))
                        {
                            error(ERR_DEREF);
                            d->tp = *tp = &stdint;
                        }
                        else
                        {
                            d->tp = *tp = basetype(tpf)->btp;
                            if (isstructured(*tp))
                                error(ERR_ILL_STRUCTURE_OPERATION);
                        }
    
                    if (needkw(&lex, comma))
                    {
                        lex = expression_assign(lex, funcsp, NULL, &tpf, &d->value, FALSE, noinline, FALSE, FALSE);
                        if (!comparetypes(tpf, *tp, FALSE))
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
                        lex = expression_assign(lex, funcsp, NULL, &tpf, &d->memoryOrder1, FALSE, noinline, FALSE, FALSE);
                    }
                    else 
                    {
                        tpf = &stdint;
                        d->memoryOrder1 =intNode(en_c_i, mo_seq_cst);
                    }
                    d->atomicOp = ao_store;
                    break;
                case kw_atomic_modify:
                    lex = expression_assign(lex, funcsp, NULL, &tpf, &d->address, FALSE, noinline, FALSE, FALSE);
                    if (tpf)
                        if (!ispointer(tpf))
                        {
                            error(ERR_DEREF);
                            d->tp = *tp = &stdint;
                        }
                        else
                        {
                            d->tp = *tp = basetype(tpf)->btp;
                            if (isstructured(*tp))
                                error(ERR_ILL_STRUCTURE_OPERATION);
                        }
                    if (needkw(&lex, comma))
                    {
                        switch(KW(lex))
                        {
                            default:
                                d->third = intNode(en_c_i, asplus);
                                break;
                            case asplus:
                            case asminus:
                            case asor:
                            case asand:
                            case asxor:
                                d->third = intNode(en_c_i, KW(lex));
                                break;
                        }
                        lex = getsym();
                    }
    
                    if (needkw(&lex, comma))
                    {
                        lex = expression_assign(lex, funcsp, NULL, &tpf, &d->value, FALSE, noinline, FALSE, FALSE);
                        if (!comparetypes(tpf, *tp, FALSE))
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
                        lex = expression_assign(lex, funcsp, NULL, &tpf, &d->memoryOrder1, FALSE, noinline, FALSE, FALSE);
                    }
                    else 
                    {
                        tpf = &stdint;
                        d->memoryOrder1 =intNode(en_c_i, mo_seq_cst);
                    }
                    d->atomicOp = ao_modify;
                    break;
                case kw_atomic_cmpswp:
                    lex = expression_assign(lex, funcsp, NULL, &tpf, &d->address, FALSE, noinline, FALSE, FALSE);
                    if (tpf)
                        if (!ispointer(tpf))
                        {
                            error(ERR_DEREF);
                            d->tp = *tp = &stdint;
                        }
                        else
                        {
                            d->tp = *tp = basetype(tpf)->btp;
                            if (isstructured(*tp))
                                error(ERR_ILL_STRUCTURE_OPERATION);
                        }
                    if (needkw(&lex, comma))
                    {
                        lex = expression_assign(lex, funcsp, NULL, &tpf1, &d->third, FALSE, noinline, FALSE, FALSE);
                        if (!comparetypes(tpf, tpf1, FALSE))
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
                        lex = expression_assign(lex, funcsp, NULL, &tpf, &d->value, FALSE, noinline, FALSE, FALSE);
                        if (!comparetypes(tpf, *tp, FALSE))
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
                        lex = expression_assign(lex, funcsp, NULL, &tpf, &d->memoryOrder1, FALSE, noinline, FALSE, FALSE);
                    }
                    else 
                    {
                        tpf = &stdint;
                        d->memoryOrder1 =intNode(en_c_i, mo_seq_cst);
                    }
                    if (needkw(&lex, comma))
                    {
                        lex = expression_assign(lex, funcsp, NULL, &tpf1, &d->memoryOrder2, FALSE, noinline, FALSE, FALSE);
                    }
                    else 
                    {
                        tpf1 = &stdint;
                        d->memoryOrder2 =intNode(en_c_i, mo_seq_cst);
                    }
                    d->atomicOp = ao_cmpswp;
                    *tp = &stdint;
                    break;
                
            }
            if (tpf && !isint(tpf))
            {
                error(ERR_NEED_INTEGER_TYPE);
                d->memoryOrder1 = *exp = intNode(en_c_i, mo_relaxed);
            }
            if (!d->memoryOrder2)
                d->memoryOrder2 = d->memoryOrder1;
            if (!needkw(&lex, closepa))
            {
                errskim(&lex, skim_closepa);
                skip(&lex, closepa);
            }
            *exp = exprNode(en_atomic, NULL , NULL);
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
static LEXEME *expression_primary(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL ampersand, BOOL noinline, BOOL packable)
{
    switch(lex ? lex->type : l_none)
    {
        SYMBOL *sym;
        case l_id:
               lex = variableName(lex, funcsp, atp, tp, exp, ampersand, noinline, packable);
            break;
        case l_kw:
            switch(KW(lex))
            {
                case openbr:
                    lex = expression_lambda(lex, funcsp, atp, tp, exp, noinline);
                    break;
                case classsel:
                case kw_operator:
                    lex = variableName(lex, funcsp, atp, tp, exp, ampersand, noinline, FALSE);
                    break;
                case kw_nullptr:
                    *exp = intNode(en_nullptr, 0);
                    *tp = &stdnullpointer;
                    lex = getsym();
                    break;
                case kw_this:
                    if (lambdas)
                    {
                        lambda_capture(NULL, cmThis, FALSE);
                        if (lambdas->captureThis)
                        {
                            SYMBOL *ths = search("$this", lambdas->cls->tp->syms);
                            if (ths)
                            {
                                TYPE *t1 = Alloc(sizeof(TYPE));
                                t1->type = bt_pointer;
                                t1->size = getSize(bt_pointer);
                                t1->btp = basetype(lambdas->lthis->tp)->btp;
                                *tp = t1;
                                *exp = varNode(en_auto, (SYMBOL *)basetype(funcsp->tp)->syms->table[0]->p); // this ptr
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
                    else if (getStructureDeclaration() && funcsp->parentClass)
                    {
                        getThisType(funcsp, tp);
                        *exp = varNode(en_auto, (SYMBOL *)basetype(funcsp->tp)->syms->table[0]->p); // this ptr
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
                    *exp = intNode(en_c_i,0);
                    (*exp)->type = en_c_fi;
                    LongLongToFPF(&(*exp)->v.f, 1);
                    *tp = &stdfloatimaginary;
                    (*exp)->pragmas = stdpragmas;
                    lex = getsym();
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
                    lex = expression_alloca(lex, funcsp, tp, exp, noinline);
                    return lex;
                case openpa:
                    lex = getsym();
                       lex = expression_comma(lex, funcsp, NULL, tp, exp, FALSE, noinline, FALSE, packable);
                    if (!*tp)
                        error(ERR_EXPRESSION_SYNTAX);
                    needkw(&lex, closepa);
                    break;
                case kw___func__:
                    *tp = &std__func__;
                    if (!funcsp->__func__label)
                    {
                        LCHAR buf[256], *q = buf;
                        char *p = funcsp->name;
                        STRING *string;
                        IncGlobalFlag();
                        string = (STRING *)Alloc(sizeof(STRING));
                        string->strtype = l_astr;
                        string->size = 1;
                        string->pointers = Alloc(sizeof(SLCHAR * ));
                        while (*p)
                            *q++ = *p++;
                        *q = 0;
                        string->pointers[0] = Alloc(sizeof(SLCHAR));
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
                    *exp = intNode(en_c_i,0);
                    (*exp)->type = en_c_f;
                    (*exp)->v.f.type = IFPF_IS_NAN;
                    lex = getsym();
                    *tp = &stdfloat;
                    break;
                case kw__INF:
                    *exp = intNode(en_c_i,0);
                    (*exp)->type = en_c_f;
                    (*exp)->v.f.type = IFPF_IS_INFINITY;
                    lex = getsym();
                    *tp = &stdfloat;
                    break;
                case kw_generic:
                    lex = expression_generic(lex, funcsp, tp, exp, noinline);
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
                    lex = expression_atomic_func(lex, funcsp, tp, exp, noinline);
                    break;
                default:
/*					errorstr(ERR_UNEXPECTED_KEYWORD, lex->kw->name); */
                    *tp = NULL;
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
            if (!lex->suffix || !getSuffixedNumber(lex, funcsp, tp,exp))
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
            (*exp)->pragmas = stdpragmas;
            *tp = &stdfloat;
            lex = getsym();
            break;
        case l_d:
            *exp = intNode(en_c_d, 0);
            (*exp)->v.f = lex->value.f;
            (*exp)->pragmas = stdpragmas;
            *tp = &stddouble;
            lex = getsym();
            break;
        case l_ld:
            if (!lex->suffix || !getSuffixedNumber(lex, funcsp, tp,exp))
            {
                *exp = intNode(en_c_ld, 0);
                (*exp)->v.f = lex->value.f;
                (*exp)->pragmas = stdpragmas;
                *tp = &stdlongdouble;
            }
            lex = getsym();
            break;
        case l_astr:
        case l_wstr:
        case l_ustr:
        case l_Ustr:
            lex = expression_string(lex, funcsp, tp, exp);
            break;
        case l_wchr:
            *exp = intNode(en_c_wc, lex->value.i);
            (*exp)->type = en_c_wc;
            *tp = &stdwidechar;
            if (lex->suffix)
                getSuffixedChar(lex, funcsp, tp,exp);
            lex = getsym();
            break;
        case l_achr:
            *exp = intNode(en_c_c, lex->value.i);
            (*exp)->type = en_c_c;
            *tp = &stdchar;
            if (lex->suffix)
                getSuffixedChar(lex, funcsp, tp,exp);
            lex = getsym();
            break;
        case l_uchr:
            *exp = intNode(en_c_u16, lex->value.i);
            (*exp)->type = en_c_u16;
            *tp = stdchar16tptr.btp;
            if (lex->suffix)
                getSuffixedChar(lex, funcsp, tp,exp);
            lex = getsym();
            break;
        case l_Uchr:
            *exp = intNode(en_c_u32, lex->value.i);
            (*exp)->type = en_c_u32;
            *tp = stdchar32tptr.btp;
            if (lex->suffix)
                getSuffixedChar(lex, funcsp, tp,exp);
            lex = getsym();
            break;
        default:
            *tp = NULL;
            *exp = intNode(en_c_i, 0);
            break;
    }
    return lex;
}
static EXPRESSION *nodeSizeof(TYPE *tp, EXPRESSION *exp)
{
    if (isref(tp))
        tp = basetype(tp)->btp;
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
    if (tp->size == 0)
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
        exp = NULL;
        if (isstructured(tp))
        {
            if (basetype(tp)->syms)
            {
                HASHREC *hr = basetype(tp)->syms->table[0];
                SYMBOL *cache = NULL;
                TYPE *tpx;
                while (hr)
                {
                    SYMBOL *sp = (SYMBOL *)hr->p;
                    if (ismemberdata(sp))
                        cache = sp;
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
        exp = intNode(en_c_i, tp->size + tp->arraySkew); 
    }
    return exp;			  
}
static int widelen(LCHAR *s)
{
    int rv = 0;
    while(*s++) rv++;
    return rv;
}
static LEXEME *expression_sizeof(LEXEME *lex, SYMBOL *funcsp, TYPE **tp, EXPRESSION **exp)
{
    int paren = FALSE;
    TYPE *itp;
    *exp = NULL;
    lex = getsym();
    if (cparams.prm_cplusplus && MATCHKW(lex, ellipse))
    {
        lex = getsym();
        if (MATCHKW(lex, openpa))
        {
            paren = TRUE;
            lex = getsym();
        }
        if (!ISID(lex))
        {
            error(ERR_SIZEOFELLIPSE_NEEDS_TEMPLATE_PACK);
            errskim(&lex, skim_closepa);
        }
        else
        {
            EXPRESSION *exp1 = NULL;
            lex = variableName(lex, funcsp, NULL, tp, &exp1, FALSE, FALSE, TRUE);
            if (!*tp || !(*tp)->templateParam || !(*tp)->templateParam->packed)
            {
                error(ERR_SIZEOFELLIPSE_NEEDS_TEMPLATE_PACK);
                *exp = intNode(en_c_i, 0);
            }
            else
            {
                int n = 0;
                TEMPLATEPARAM *templateParam = (*tp)->templateParam->byPack.pack;
                while (templateParam)
                {
                    n++;
                    templateParam = templateParam->next;
                }
                *tp = &stdunsigned;
                *exp = intNode(en_c_i, n);
            }
        }
    }
    else
    {
        if (MATCHKW(lex, openpa))
        {
            paren = TRUE;
            lex = getsym();
        }
        if (!paren || !startOfType(lex, FALSE))
        {
            if (paren)
            {
                lex = expression_comma(lex, funcsp, NULL, tp, exp, FALSE, FALSE, FALSE, FALSE);
            }
            else
                lex = expression_unary(lex, funcsp, NULL, tp, exp, FALSE, FALSE, FALSE);
            if (!*tp)
            {
                *exp = intNode(en_c_i, 1);
                error(ERR_EXPRESSION_SYNTAX);
            }
            else
            {
                *exp = nodeSizeof(*tp, *exp);
            }
        }
        else
        {
            lex = get_type_id(lex, tp, funcsp, FALSE);
            if (!*tp)
            {
                *exp = intNode(en_c_i, 1);
            }
            else
            {
                checkauto(*tp);
                *exp = nodeSizeof(*tp, *exp);
            }
        }
        itp = *tp;
        if (isref(itp))
            itp = basetype(itp)->btp;
            
           *tp = &stdint; /* other compilers use sizeof as a signed value so we do too... */
    }
    if (paren)
        needkw(&lex, closepa);
    return lex;
}
static LEXEME *expression_alignof(LEXEME *lex, SYMBOL *funcsp, TYPE **tp, EXPRESSION **exp)
{
    int paren = FALSE;
    lex = getsym();
    if (needkw(&lex, openpa))
    {
        lex = get_type_id(lex, tp, funcsp, FALSE);
        if (!*tp)
        {
            *exp = intNode(en_c_i, 1);
        }
        else
        {
            TYPE *itp = *tp;
            checkauto(itp);
            if (isref(itp))
                itp = (basetype(itp)->btp);
            while (itp->array)
                itp = itp->btp;
            *exp = intNode(en_c_i, getAlign(sc_global, *tp));
        }
        needkw(&lex, closepa);
    }
       *tp = &stdint;
    return lex;
}
static LEXEME *expression_ampersand(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL noinline, BOOL packable)
{
    lex = getsym();
    lex = expression_cast(lex, funcsp, atp, tp, exp, TRUE, noinline, packable);
    if (*tp)
    {
        TYPE *btp, *tp1;
        btp = basetype(*tp);
        if (cparams.prm_cplusplus && insertOperatorFunc(ovcl_unary_any, and,
                               funcsp, tp, exp, NULL,NULL, NULL, noinline))
        {
            return lex;
        }
        else if (isvoid(*tp))
            error(ERR_NOT_AN_ALLOWED_TYPE);
        else if (btp->hasbits)
            error(ERR_CANNOT_TAKE_ADDRESS_OF_BIT_FIELD);
        else if (inreg(*exp, TRUE))
                error(ERR_CANNOT_TAKE_ADDRESS_OF_REGISTER);
        else if ((!ispointer(btp) || !(btp)->array) && !isstructured(btp) &&
            !isfunction(btp) && (*exp)->type != en_memberptr)
            if ((*exp)->type != en_const)
                if (!lvalue(*exp))
                    if (cparams.prm_ansi || !castvalue(*exp))
                        error(ERR_MUST_TAKE_ADDRESS_OF_MEMORY_LOCATION);
        else
            switch ((*exp)->type)
            {
                case en_pc:
                case en_auto:
                case en_label:
                case en_global:
                case en_absolute:
                case en_threadlocal:
                {
                    SYMBOL *sp = (*exp)->v.sp;
                    if (sp->isDestructor || sp->isDestructor)
                        error(ERR_CANNOT_TAKE_ADDRESS_OF_CONSTRUCTOR_OR_DESTRUCTOR);
                    break;
                }
            }
        if ((*exp)->type == en_const)
        {
            /* if a variable propagated silently to an inline constant
             * this will restore it as a static variable in the const section
             * when we take its address
             * but the constant will still be used inline when possible
             */
            SYMBOL *sp = (*exp)->v.sp;
            if (!sp->label)
            {
                TYPE *tp, *tpb, *tpn,**tpnp = &tpn;
                BOOL done = FALSE;
                sp->label = nextLabel++;
                IncGlobalFlag();
                sp = clonesym(sp);
                tp = sp->tp;
                tpb = basetype(tp);
                do
                {
                    *tpnp = Alloc(sizeof(TYPE));
                    **tpnp = *tp;
                    tpnp = &(*tpnp)->btp;
                    if (tp != tpb)
                        tp = tp->btp;
                    else
                        done = TRUE;
                } while (!done);
                sp->tp = tpn;
                insertInitSym(sp);
                DecGlobalFlag();
            }
            else
            {
                IncGlobalFlag();
                insertInitSym(sp);
                DecGlobalFlag();
            }
            *exp = varNode(en_label, sp);
            tp1 = Alloc(sizeof(TYPE));
            tp1->type = bt_pointer;
            tp1->size = getSize(bt_pointer);
            tp1->btp = *tp;
            *tp = tp1;
        }	
        else if (!isfunction(*tp))
        {
            while(castvalue(*exp))
                *exp = (*exp)->left;
            if (!lvalue(*exp))
            {
                if (!btp->array && !btp->vla && !isstructured(btp) && basetype(btp)->type != bt_memberptr && basetype(btp)->type != bt_templateparam)
                    error(ERR_LVALUE);
            }
            else if (!isstructured(btp))
                *exp = (*exp)->left;
                
            switch ((*exp)->type)
            {
                case en_pc:
                case en_auto:
                case en_label:
                case en_global:
                case en_absolute:
                case en_threadlocal:
                    (*exp)->v.sp->addressTaken = TRUE;
                    break;
            }
            if (basetype(btp)->type != bt_memberptr)
            {
                tp1 = Alloc(sizeof(TYPE));
                tp1->type = bt_pointer;
                tp1->size = getSize(bt_pointer);
                tp1->btp = *tp;
                *tp = tp1;
            }
        }
    }
    return lex;
}
static LEXEME *expression_deref(LEXEME *lex, SYMBOL *funcsp, TYPE **tp, EXPRESSION **exp, BOOL noinline, BOOL packable)
{
/*vla */
    lex = getsym();
    lex = expression_cast(lex, funcsp, NULL, tp, exp, FALSE, noinline, packable);
    if (cparams.prm_cplusplus && insertOperatorFunc(ovcl_unary_pointer, star,
                           funcsp, tp, exp, NULL,NULL, NULL, noinline))
    {
        return lex;
    }
    if (*tp && isvoid(*tp))
        error(ERR_NOT_AN_ALLOWED_TYPE);
    else if (*tp && basetype(*tp)->type == bt_memberptr)
        error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
    if (*tp && !isfunction(*tp) && !isfuncptr(*tp))
    {
        TYPE *btp = basetype(*tp), *btp2, *btp3;;
        if (!ispointer(*tp))
        {
            error(ERR_DEREF);
        }
        else
        {
            btp3 = btp->btp;
            btp2 = basetype(btp->btp);
            if (btp2->type == bt_void)
            {
                error(ERR_DEREF);
            }
            else if (isstructured(btp2))
            {
                *tp = btp3;
            }
            else 
            {
                if (!isfunction(btp2))
                {
                    *tp = btp3;
                    deref(*tp, exp);
                    if (isvolatile(btp3))
                        (*exp)->isvolatile = TRUE;
                    if (isrestrict(btp3))
                        (*exp)->isrestrict = TRUE;
                    if (isatomic(btp3))
                    {
                        (*exp)->isatomic = TRUE;
                        if (needsAtomicLockFromType(btp3))
                            (*exp)->lockOffset = btp3->size - ATOMIC_FLAG_SPACE;
                    }
                }
            }
        }
        
        
    }
    return lex;
}
static LEXEME *expression_postfix(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL ampersand, BOOL noinline, BOOL packable)
{
    TYPE *oldType;
    BOOL done = FALSE;
    if (KWTYPE(lex, TT_POINTERQUAL | TT_LINKAGE | TT_BASETYPE | TT_STORAGE_CLASS))
    {
        lex = expression_func_type_cast(lex, funcsp, tp, exp, noinline);
    }
    else switch(KW(lex))
    {
        case kw_dynamic_cast:
            oldType = NULL;
            lex = GetCastInfo(lex, funcsp, tp, &oldType, exp, packable);
            if (!doDynamicCast(tp, oldType, exp, funcsp, noinline))
                if (!typeHasTemplateArg(*tp))
                    errortype(ERR_CANNOT_CAST_TYPE, oldType, *tp);
            break;
        case kw_static_cast:
            oldType = NULL;
            lex = GetCastInfo(lex, funcsp, tp, &oldType, exp, packable);
            if (!doStaticCast(tp, oldType, exp, funcsp, TRUE, noinline))
                if (!typeHasTemplateArg(*tp))
                    errortype(ERR_CANNOT_CAST_TYPE, oldType, *tp);
            break;
        case kw_const_cast:
            oldType = NULL;
            lex = GetCastInfo(lex, funcsp, tp, &oldType, exp, packable);
            if (!doConstCast(tp, oldType, exp, funcsp))
                if (!typeHasTemplateArg(*tp))
                    errortype(ERR_CANNOT_CAST_TYPE, oldType, *tp);
            break;
        case kw_reinterpret_cast:
            oldType = NULL;
            lex = GetCastInfo(lex, funcsp, tp, &oldType, exp, packable);
            if (!doReinterpretCast(tp, oldType, exp, funcsp, TRUE))
                if (!typeHasTemplateArg(*tp))
                    errortype(ERR_CANNOT_CAST_TYPE, oldType, *tp);
            break;
        case kw_typeid:
            lex = expression_typeid(lex, funcsp, tp, exp, packable);
            break;
        default:
            lex = expression_primary(lex, funcsp, atp, tp, exp, ampersand, noinline, packable);
            break;
    }
    if (!*tp)
        return lex;
    while (!done && lex)
    {
        enum e_kw kw;
        switch(KW(lex))
        {
            case openbr:
                lex = expression_bracket(lex, funcsp, tp, exp, noinline, packable);
                break;
            case openpa:
                lex = expression_arguments(lex, funcsp, tp, exp, noinline);
                break;
            case pointsto:
            case dot:
                lex = expression_member(lex, funcsp, tp, exp, noinline);
                break;
            case autoinc:
            case autodec:

                kw = KW(lex);
                lex = getsym();
                if (cparams.prm_cplusplus && insertOperatorFunc(ovcl_unary_postfix, kw,
                                       funcsp, tp, exp, NULL,NULL, NULL, noinline))
                {
                }
                else
                {
                    castToArithmetic(FALSE, tp, exp, kw, NULL, noinline, TRUE);
                    if (isstructured(*tp))
                        error(ERR_ILL_STRUCTURE_OPERATION);
                    else if (!lvalue(*exp) && basetype(*tp)->type != bt_templateparam)
                        error(ERR_LVALUE);
                    else
                    {
                        EXPRESSION *exp1 = NULL;
                        if (basetype(*tp)->type == bt_pointer)
                        {
                            TYPE *btp = basetype(*tp)->btp;
                            exp1 = nodeSizeof(btp, *exp);
                        }
                        else
                        {
                            if (isvoid(*tp))
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
                            if (kw== autodec)
                                error(ERR_CANNOT_USE_BOOLEAN_HERE);
                            *exp = exprNode(en_assign, *exp, intNode(en_c_bool, 1));
                        }
                        else
                            *exp = exprNode(kw == autoinc ? en_autoinc : en_autodec,
                                        *exp, exp1);
                        while (lvalue(exp1))
                            exp1 = exp1->left;
                        if (exp1->type == en_auto)
                            exp1->v.sp->altered = TRUE;
                    }
                }
                break;
            default:
                done = TRUE;
                break;
        }
    }
/*	if (isfunction(*tp))
        error(ERR_FUNCTION_ARGUMENTS_EXPECTED);
*/
    return lex;
}
LEXEME *expression_unary(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL ampersand, BOOL noinline, BOOL packable)
{
    enum e_kw kw= KW(lex);
    /* note some of the math ops are speced to do integer promotions
     * if being stored, the proposed place to store them is not known, so e.g.
     * a ~ on a unsigned char would promote to int to be evaluated, 
     * the int would be further sign
     * extended to a long long though through the assignment promotion
     */
    switch(kw)
    {
        case plus:
            lex = getsym();
            lex = expression_cast(lex, funcsp, atp, tp, exp, FALSE, noinline, packable);
            if (*tp)
            {
                if (cparams.prm_cplusplus && insertOperatorFunc(ovcl_unary_numeric, plus,
                                       funcsp, tp, exp, NULL,NULL, NULL, noinline))
                {
                }                                       
                else 
                {
                    castToArithmetic(FALSE, tp, exp, kw, NULL, noinline, TRUE);
                    if (isstructured(*tp))
                        error(ERR_ILL_STRUCTURE_OPERATION);
                    else if (isvoid(*tp))
                        error(ERR_NOT_AN_ALLOWED_TYPE);
                    else if (basetype(*tp)->type == bt_memberptr)
                        error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                    else if (basetype(*tp)->scoped)
                        error(ERR_SCOPED_TYPE_MISMATCH);
                    else if (ispointer(*tp))
                        error(ERR_ILL_POINTER_OPERATION);
                    else
                        if (atp && basetype(atp)->type < bt_int)
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
            lex = expression_cast(lex, funcsp, atp, tp, exp, FALSE, noinline, packable);
            if (*tp)
            {
                if (cparams.prm_cplusplus && insertOperatorFunc(ovcl_unary_numeric, minus,
                                       funcsp, tp, exp, NULL,NULL, NULL, noinline))
                {
                }
                else {
                    castToArithmetic(FALSE, tp, exp, kw, NULL, noinline, TRUE);
                    if (isstructured(*tp))
                        error(ERR_ILL_STRUCTURE_OPERATION);
                    else if (isvoid(*tp))
                        error(ERR_NOT_AN_ALLOWED_TYPE);
                    else if (basetype(*tp)->type == bt_memberptr)
                        error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                    else if (basetype(*tp)->scoped)
                        error(ERR_SCOPED_TYPE_MISMATCH);
                    else if (ispointer(*tp))
                        error(ERR_ILL_POINTER_OPERATION);
                    else
                        if (atp && basetype(atp)->type < bt_int)
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
                *exp = exprNode(en_uminus, *exp, NULL);
            }
            break;
        case star:
            lex = expression_deref(lex, funcsp, tp, exp, noinline, packable);
            break;
        case and:
            lex = expression_ampersand(lex, funcsp, atp,tp, exp, noinline, packable);
            break;
        case not:
            lex = getsym();
            lex = expression_cast(lex, funcsp, atp, tp, exp, FALSE, noinline, packable);
            if (*tp)
            {
                if (cparams.prm_cplusplus && insertOperatorFunc(ovcl_unary_numericptr, not,
                                       funcsp, tp, exp, NULL,NULL, NULL, noinline))
                {
                }
                else 
                {
                    castToArithmetic(FALSE, tp, exp, kw, NULL, noinline, TRUE);
                    if (isstructured(*tp))
                        error(ERR_ILL_STRUCTURE_OPERATION);
                    else if (isvoid(*tp))
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
                }
                if ((*tp)->type == bt_memberptr)
                {
                       *exp = exprNode(en_mp_as_bool, *exp, NULL);
                    (*exp)->size = (*tp)->size;
                       *exp = exprNode(en_not, *exp, NULL);
                }
                else
                {
                    *exp = exprNode(en_not, *exp, NULL);
                }
                if (cparams.prm_cplusplus)
                    *tp = &stdbool;
                else
                    *tp = &stdint;
            }
            break;
        case compl:
            lex = getsym();
            lex = expression_cast(lex, funcsp, atp, tp, exp, FALSE, noinline, packable);
            if (*tp)
            {
                if (cparams.prm_cplusplus && insertOperatorFunc(ovcl_unary_int, compl,
                                       funcsp, tp, exp, NULL,NULL, NULL, noinline))
                {
                }
                else {
                    castToArithmetic(TRUE, tp, exp, kw, NULL, noinline, TRUE);
                    if (isstructured(*tp))
                        error(ERR_ILL_STRUCTURE_OPERATION);
                    else if (iscomplex(*tp))
                        error(ERR_ILL_USE_OF_COMPLEX);
                    else if (isfloat(*tp) || isimaginary(*tp))
                        error(ERR_ILL_USE_OF_FLOATING);
                    else if (ispointer(*tp))
                        error(ERR_ILL_POINTER_OPERATION);
                    else if (isvoid(*tp))
                        error(ERR_NOT_AN_ALLOWED_TYPE);
                    else if (basetype(*tp)->type == bt_memberptr)
                        error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                    else if (basetype(*tp)->scoped)
                        error(ERR_SCOPED_TYPE_MISMATCH);
                    else
                        if (atp && basetype(atp)->type < bt_int)
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
                *exp = exprNode(en_compl, *exp, NULL);
            }
            break;
        case autoinc:
        case autodec:
            lex = getsym();
               lex = expression_cast(lex, funcsp, atp, tp, exp, FALSE, noinline, packable);
            if (*tp)
            {
                if (cparams.prm_cplusplus && insertOperatorFunc(ovcl_unary_prefix, kw,
                                       funcsp, tp, exp, NULL,NULL, NULL, noinline))
                {
                }
                else 
                {
                    castToArithmetic(FALSE, tp, exp, kw, NULL, noinline, TRUE);
                    if (kw == autodec && basetype(*tp)->type == bt_bool)
                        error(ERR_CANNOT_USE_BOOLEAN_HERE);
                    else if (isstructured(*tp))
                        error(ERR_ILL_STRUCTURE_OPERATION);
                    else if (iscomplex(*tp))
                        error(ERR_ILL_USE_OF_COMPLEX);
                    else if (isconst(*tp))
                        error(ERR_CANNOT_MODIFY_CONST_OBJECT);
                    else if (isvoid(*tp))
                        error(ERR_NOT_AN_ALLOWED_TYPE);
                    else if (basetype(*tp)->type == bt_memberptr)
                        error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                    else if (basetype(*tp)->scoped)
                        error(ERR_SCOPED_TYPE_MISMATCH);
                    else if (!lvalue(*exp) && basetype(*tp)->type != bt_templateparam)
                        error(ERR_LVALUE);
                    else if (ispointer(*tp))
                    {
                        *exp = exprNode(en_assign, *exp, exprNode(kw == autoinc ? en_add : en_sub, 
                                                                  *exp, nodeSizeof(basetype(*tp)->btp, *exp)));
                    }
                    else if (kw == autoinc && basetype(*tp)->type == bt_bool)
                    {
                        *exp = exprNode(en_assign, *exp, intNode(en_c_i, 1)); // set to true as per C++
                    }
                    else
                    {
                        *exp = exprNode(en_assign, *exp, exprNode(kw == autoinc ? en_add : en_sub, *exp, intNode(en_c_i,1)));
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
            lex = expression_new(lex, funcsp, tp, exp, FALSE, noinline, packable);
            break;
        case kw_delete:
            lex = expression_delete(lex, funcsp, tp, exp, FALSE, noinline);
            break;
        case kw_noexcept:
            lex = expression_noexcept(lex, funcsp, tp, exp);
            break;
        case classsel:
        {
            LEXEME *placeholder = lex;
            lex = getsym();
            switch(KW(lex))
            {
                case kw_new:
                    return expression_new(lex, funcsp, tp, exp, TRUE, noinline, packable);
                case kw_delete:
                    return expression_delete(lex, funcsp, tp, exp, TRUE, noinline);
            }
            lex = prevsym(placeholder);
        }
            // fallthrough
        default:
            lex = expression_postfix(lex, funcsp, atp, tp, exp, ampersand, noinline, packable);
            break;
    }
    return lex;
}
LEXEME *expression_cast(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL ampersand, BOOL noinline, BOOL packable)
{
    TYPE *throwaway;
    if (MATCHKW(lex, openpa))
    {
        lex = getsym();
        if (startOfType(lex, FALSE))
        {
            lex = get_type_id(lex, tp, funcsp, FALSE);
            (*tp)->used = TRUE;
            needkw(&lex, closepa);
            checkauto(*tp);
            if (MATCHKW(lex, begin))
            {
                INITIALIZER *init = NULL, *dest = NULL;
                SYMBOL *sp = NULL;
                if (!cparams.prm_c99 && !cparams.prm_cplusplus)
                    error(ERR_C99_STYLE_INITIALIZATION_USED);
                if (cparams.prm_cplusplus)
                {
                    sp = makeID(sc_auto, *tp, NULL, AnonymousName());
                    insert(sp, localNameSpace->syms);
                }
                lex = initType(lex, funcsp, NULL, sc_auto, &init, &dest, *tp, sp, FALSE);
                *exp = convertInitToExpression(*tp, NULL, funcsp, init, NULL, noinline);
            }
            else
            { 
                lex = expression_cast(lex, funcsp, NULL, &throwaway, exp, ampersand, noinline, packable);
//                if ((*exp)->type == en_func)
//                    *exp = (*exp)->v.func->fcall;
                if (throwaway)
                    if (isvoid(throwaway) && !isvoid(*tp))
                    {
                        error(ERR_NOT_AN_ALLOWED_TYPE);
                    }
                    else if (!cparams.prm_cplusplus && (isstructured(throwaway) && !isvoid(*tp) || basetype(throwaway)->type == bt_memberptr || basetype(*tp)->type == bt_memberptr)
                              && !comparetypes(throwaway, *tp, TRUE))
                    {
                        error(ERR_INCOMPATIBLE_TYPE_CONVERSION);
                    }
                    else if (cparams.prm_cplusplus)
                    {
                        if (!doStaticCast(tp, throwaway, exp, funcsp, FALSE, noinline) 
                            && !doReinterpretCast(tp, throwaway, exp, funcsp, FALSE))
                                errortype(ERR_CANNOT_CAST_TYPE, throwaway, *tp);
                    }
                    else
                        cast(*tp, exp);
            }
        }
        else
        {
            lex = backupsym();
            lex = expression_unary(lex, funcsp, atp, tp, exp, ampersand, noinline, packable);
        }
    }
    else
    {
        lex = expression_unary(lex, funcsp, atp, tp, exp, ampersand, noinline, packable);
    }
    return lex;
} 
static LEXEME *expression_pm(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL noinline, BOOL packable)
{
    lex = expression_cast(lex, funcsp, atp, tp, exp, FALSE, noinline, packable);
    if (*tp == NULL)
        return lex;
    while (MATCHKW(lex, dotstar) || MATCHKW(lex, pointstar))
    {
        BOOL points = FALSE;
        enum e_kw kw = KW(lex);
        enum e_node type ;
        TYPE *tp1 = NULL;
        EXPRESSION *exp1 = NULL;
          lex = getsym();
        lex = expression_cast(lex, funcsp, NULL, &tp1, &exp1, FALSE, noinline, packable);
        if (cparams.prm_cplusplus && kw == pointstar && insertOperatorFunc(ovcl_binary_any, pointstar,
                               funcsp, tp, exp, tp1, exp1, NULL, noinline))
        {
            continue;
        }
        if (MATCHKW(lex, pointstar))
        {
            points = TRUE;
            if (ispointer(*tp))
            {
                *tp = basetype(*tp);
                *tp = (*tp)->btp;
                deref(&stdpointer, exp);
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
        if (basetype(tp1)->type != bt_memberptr)
        {
            error(ERR_INCOMPATIBLE_TYPE_CONVERSION);
        }
        else
        {
            if (isstructured(*tp) && basetype(tp1)->type == bt_memberptr)
            {
                if ((*tp)->sp != tp1->sp)
                {
                    if (classRefCount(tp1->sp, (*tp)->sp) != 1)
                    {
                        errorsym2(ERR_NOT_UNAMBIGUOUS_BASE, tp1->sp, (*tp)->sp);
                    }
                    else
                    {
                        *exp = baseClassOffset(tp1->sp, (*tp)->sp, *exp);
                    }
                }
                if (exp1->type == en_memberptr)
                {
                    int lab = dumpMemberPtr(exp1->v.sp, *tp, TRUE);
                    exp1 = intNode(en_labcon, lab);
                }
                if (isfunction(basetype(tp1)->btp))
                {
                    FUNCTIONCALL *funcparams = Alloc(sizeof(FUNCTIONCALL));
                    if ((*tp)->sp->vbaseEntries)
                    {
                        EXPRESSION *ec = exprNode(en_add, exp1, intNode(en_c_i,getSize(bt_pointer) + getSize(bt_int)));
                        EXPRESSION *ec1;
                        deref(&stdint, &ec);
                        ec1 = exprNode(en_sub, ec, intNode(en_c_i,1));
                        deref(&stdint, &ec1);
                        ec = exprNode(en_cond, ec, exprNode(en_void, ec1, intNode(en_c_i, 0)));
                        *exp = exprNode(en_add, *exp, ec);
                    }
                    else
                    {
                        EXPRESSION *ec = exprNode(en_add, exp1, intNode(en_c_i, getSize(bt_pointer)));
                        deref(&stdpointer, &ec);
                        *exp = exprNode(en_add, *exp, ec);
                    }
                    funcparams->sp = tp1->btp->sp;
                    funcparams->fcall = exp1;
                    deref(&stdpointer, &funcparams->fcall);
                    funcparams->thisptr = *exp;
                    funcparams->thistp = Alloc(sizeof(TYPE));
                    funcparams->thistp->size = getSize(bt_pointer);
                    funcparams->thistp->type = bt_pointer;
                    funcparams->thistp->btp = *tp;
                    *exp = Alloc(sizeof(EXPRESSION));
                    (*exp)->type = en_func;
                    (*exp)->v.func = funcparams;
                }
                else
                {
                    if ((*tp)->sp->vbaseEntries)
                    {
                        EXPRESSION *ec = exprNode(en_add, exp1, intNode(en_c_i, getSize(bt_int)));
                        EXPRESSION *ec1;
                        deref(&stdint, &ec);
                        ec1 = exprNode(en_add, ec, intNode(en_c_i,-1));
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
                }
                *tp = basetype(tp1)->btp;
            }
        }
    }
    return lex;
}
static LEXEME *expression_times(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL noinline, BOOL packable)
{
    lex = expression_pm(lex, funcsp, atp, tp, exp, noinline, packable);
    if (*tp == NULL)
        return lex;
    while (MATCHKW(lex, star) || MATCHKW(lex, divide) || MATCHKW(lex, mod))
    {
        enum e_kw kw = KW(lex);
        enum e_node type ;
        TYPE *tp1 = NULL;
        EXPRESSION *exp1 = NULL;
        lex = getsym();
        lex = expression_pm(lex, funcsp, NULL, &tp1, &exp1, noinline, packable);
        if (!tp1)
        {
            *tp = NULL;
            return lex;
        }
        if (cparams.prm_cplusplus
            && insertOperatorFunc(kw == mod ? ovcl_binary_int : ovcl_binary_numeric, kw,
                               funcsp, tp, exp, tp1, exp1, NULL, noinline))
        {
        }
        else 
        {
            castToArithmetic(kw == mod, tp, exp, kw, tp1, noinline, TRUE);
            castToArithmetic(kw == mod, &tp1, &exp1, (enum e_kw)-1, *tp, noinline, TRUE);
            if (isstructured(*tp) || isstructured(tp1))
                error(ERR_ILL_STRUCTURE_OPERATION);
            else if (isvoid(*tp) || isvoid(tp1))
                error(ERR_NOT_AN_ALLOWED_TYPE);
            else if (basetype(*tp)->type == bt_memberptr || basetype(tp1)->type == bt_memberptr)
                error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
            else if (basetype(*tp)->scoped || basetype(tp1)->scoped)
                error(ERR_SCOPED_TYPE_MISMATCH);
            else if (ispointer(*tp) || ispointer(tp1))
                error(ERR_ILL_POINTER_OPERATION);
            else
            {
                *tp = destSize(*tp, tp1, exp, &exp1, FALSE, NULL);
                switch(kw)
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
                }
                *exp = exprNode(type, *exp, exp1);
            }
        }
    }
    return lex;
}

static LEXEME *expression_add(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL noinline, BOOL packable)
{
/* fixme add vlas */
    lex = expression_times(lex, funcsp, atp, tp, exp, noinline, packable);
    if (*tp == NULL)
        return lex;
    while (MATCHKW(lex, plus) || MATCHKW(lex, minus))
    {
        enum e_kw kw = KW(lex);
        TYPE *tp1 = NULL;
        EXPRESSION *exp1 = NULL;
        lex = getsym();
        lex = expression_times(lex, funcsp, atp, &tp1, &exp1, noinline, packable);
        if (!tp1)
        {
            *tp = NULL;
            return lex;
        }
        if (cparams.prm_cplusplus && insertOperatorFunc(ovcl_binary_numericptr, kw,
                               funcsp, tp, exp, tp1, exp1, NULL, noinline))
        {
            continue;
        }
        else {
            if (!ispointer(*tp) && !ispointer(tp1))
            {
                castToArithmetic(FALSE, tp, exp, kw, tp1, noinline, TRUE);
                castToArithmetic(FALSE, &tp1, &exp1, (enum e_kw)-1, *tp, noinline, TRUE);
            }
            if (kw == plus && ispointer(*tp) && ispointer(tp1))
                error(ERR_ILL_POINTER_ADDITION);
            else if (isvoid(*tp) || isvoid(tp1))
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
                if (ispointer(tp1) && !comparetypes(*tp, tp1, TRUE))
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
            }
        }
        if (ispointer(*tp))
        {
            EXPRESSION *ns = nodeSizeof(basetype(*tp)->btp, *exp);
            if (ispointer(tp1))
            {
                *exp = exprNode(en_sub, *exp, exp1);
                *exp = exprNode(en_udiv, *exp, ns);
                *tp = &stdint; /* ptrdiff_t */
            }
            else
            {
/*				*tp = tp1 = destSize(*tp, tp1, exp, &exp1, FALSE, NULL); */
                if (basetype(tp1)->type < bt_int)
                    cast(&stdint, &exp1);
                exp1 = exprNode(en_umul, exp1, ns);
                *exp = exprNode(kw == plus ? en_add : en_sub, *exp, exp1);
            }
        }
        else if (ispointer (tp1))
        {
            EXPRESSION *ns = nodeSizeof(basetype(tp1)->btp, *exp);
/*			*tp = tp1 = destSize(*tp, tp1, exp, &exp1, FALSE, NULL); */
            if (basetype(*tp)->type < bt_int)
                cast(&stdint, exp);
            *exp = exprNode(en_umul, *exp, ns);
            *exp = exprNode(en_add, *exp, exp1);
            *tp = tp1;
        }
        else
        {
            *tp = destSize(*tp, tp1, exp, &exp1, TRUE, atp);
            *exp = exprNode(kw == plus ? en_add : en_sub, *exp, exp1);
        }
    }
    return lex;
}
static LEXEME *expression_shift(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL noinline, BOOL inTemplateParams, BOOL packable)
{
    lex = expression_add(lex, funcsp, atp, tp, exp, noinline, packable);
    if (*tp == NULL)
        return lex;
    while (MATCHKW(lex, rightshift) && !inTemplateParams || MATCHKW(lex, leftshift))
    {
        TYPE *tp1 = NULL;
        EXPRESSION *exp1 = NULL;
        enum e_node type ; 
        enum e_kw kw = KW(lex);
        lex = getsym();
        lex = expression_add(lex, funcsp, NULL, &tp1, &exp1, noinline, packable);
        if (!tp1)
        {
            *tp = NULL;
            return lex;
        }
        if (cparams.prm_cplusplus && insertOperatorFunc(ovcl_binary_int, kw,
                               funcsp, tp, exp, tp1, exp1, NULL, noinline))
        {
        }
        else {
            castToArithmetic(TRUE, tp, exp, kw, tp1, noinline, TRUE);
            castToArithmetic(TRUE, &tp1, &exp1, (enum e_kw)-1, *tp, noinline, TRUE);
            if (isstructured(*tp) || isstructured(tp1))
                error(ERR_ILL_STRUCTURE_OPERATION);
            else if (isvoid(*tp) || isvoid(tp1))
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
static LEXEME *expression_inequality(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL noinline, BOOL inTemplateParams, BOOL packable)
{
    BOOL done = FALSE;
    lex = expression_shift(lex, funcsp, atp, tp, exp, noinline, inTemplateParams, packable);
    if (*tp == NULL)
        return lex;
    while (!done && lex)
    {	
        enum e_kw kw = KW(lex);
        enum e_node type ;
        char *opname;
        TYPE *tp1 = NULL;
        EXPRESSION *exp1 = NULL;
        switch(kw)
        {
            case gt:
                opname = lex->kw->name;
                done = inTemplateParams;
                break;
            case geq:
            case lt:
            case leq:
                opname = lex->kw->name;
                break;
            default:
                done = TRUE;
                break;
        }
        if (!done)
        {
            lex = getsym();
            lex = expression_shift(lex, funcsp, NULL, &tp1, &exp1, noinline, inTemplateParams, packable);
            if (!tp1)
            {
                *tp = NULL;
                return lex;
            }
            if (cparams.prm_cplusplus && insertOperatorFunc(ovcl_binary_numericptr, kw,
                                   funcsp, tp, exp, tp1, exp1, NULL, noinline))
            {
            }
            else
            {
                checkscope(*tp, tp1);
                castToArithmetic(FALSE, tp, exp, kw, tp1, noinline, TRUE);
                castToArithmetic(FALSE, &tp1, &exp1, (enum e_kw)-1, *tp, noinline, TRUE);
                if (isstructured(*tp) || isstructured(tp1))
                    error(ERR_ILL_STRUCTURE_OPERATION);
                else if (isvoid(*tp) || isvoid(tp1))
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
                    else if (ispointer(tp1) && !isvoidptr(*tp) && !isvoidptr(tp1) && !comparetypes(*tp, tp1, FALSE))
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
                    if (isunsigned(*tp) && !isunsigned(tp1) ||
                        isunsigned(tp1) && !isunsigned(*tp))
                        errorstr(ERR_SIGNED_UNSIGNED_MISMATCH_RELAT, opname);
                }
                *tp = destSize(*tp, tp1, exp, &exp1, TRUE, NULL);
                switch(kw)
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
                        done = TRUE;
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
static LEXEME *expression_equality(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL noinline, BOOL inTemplateParams, BOOL packable)
{
    lex = expression_inequality(lex, funcsp, atp, tp, exp, noinline, inTemplateParams, packable);
    if (*tp == NULL)
        return lex;
    while (MATCHKW(lex, eq) || MATCHKW(lex, neq))
    {
        BOOL done = FALSE;
        TYPE *tp1 = NULL;
        EXPRESSION *exp1 = NULL;
        enum e_kw kw = KW(lex);
        lex = getsym();
        lex = expression_inequality(lex, funcsp, NULL, &tp1, &exp1, noinline, inTemplateParams, packable);
        if (!tp1)
        {
            *tp = NULL;
            return lex;
        }
        if (cparams.prm_cplusplus && insertOperatorFunc(ovcl_binary_numericptr, kw,
                               funcsp, tp, exp, tp1, exp1, NULL, noinline))
        {
        }
        else
        {
            checkscope(*tp, tp1);
            castToArithmetic(FALSE, tp, exp, kw, tp1, noinline, TRUE);
            castToArithmetic(FALSE, &tp1, &exp1, (enum e_kw)-1, *tp, noinline, TRUE);
            if (isstructured(*tp) || isstructured(tp1))
                error(ERR_ILL_STRUCTURE_OPERATION);
            else if (isvoid(*tp) || isvoid(tp1))
                error(ERR_NOT_AN_ALLOWED_TYPE);
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
                else if (ispointer(tp1) && !isvoidptr(*tp) && !isvoidptr(tp1) && !comparetypes(*tp, tp1, FALSE))
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
                if (isconstzero(tp1, exp1))
                {
                       *exp = exprNode(en_mp_as_bool, *exp, NULL);
                    (*exp)->size = (*tp)->size;
                    if (kw == eq)
                        *exp = exprNode(en_not, *exp, NULL);
                    done = TRUE;
                }
                else if (comparetypes(*tp, tp1, TRUE))
                {
                    *exp = exprNode(en_mp_compare, *exp, exp1);
                    (*exp)->size = tp1->size;
                    if (kw == neq)
                        *exp = exprNode(en_not, *exp, NULL);
                    done = TRUE;
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
                       *exp = exprNode(en_mp_as_bool, *exp, NULL);
                    (*exp)->size = (*tp)->size;
                    if (kw == eq)
                        *exp = exprNode(en_not, *exp, NULL);
                    done = TRUE;
                }
                else if (comparetypes(*tp, tp1, TRUE))
                {
                    *exp = exprNode(en_mp_compare, *exp, exp1);
                    (*exp)->size = tp1->size;
                    if (kw == neq)
                        *exp = exprNode(en_not, *exp, NULL);
                    done = TRUE;
                }
                else
                {
                    error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                }
            }
            if (!done)
            {
                destSize(*tp, tp1, exp, &exp1, TRUE, NULL);
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
static LEXEME *binop(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE ** tp, EXPRESSION **exp, enum e_kw kw, enum e_node type, 
              LEXEME *(nextFunc)(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL noinline, BOOL inTemplateParams, BOOL packable), 
              BOOL noinline, BOOL inTemplateParams, BOOL packable)
{
    lex = (*nextFunc)(lex, funcsp, atp, tp, exp, noinline, inTemplateParams, packable);
    if (*tp == NULL)
        return lex;
    while (MATCHKW(lex, kw))
    {
        TYPE *tp1 = NULL;
        EXPRESSION *exp1 = NULL;
        lex = getsym();
        lex = (*nextFunc)(lex, funcsp, atp, &tp1, &exp1, noinline, inTemplateParams, packable);
        if (!tp1)
        {
            *tp = NULL;
            break;
        }
        if (cparams.prm_cplusplus 
            && insertOperatorFunc(kw == en_lor || kw == en_land ? ovcl_binary_numericptr : ovcl_binary_int, kw,
                               funcsp, tp, exp, tp1, exp1, NULL, noinline))
        {
            continue;
        }
        castToArithmetic(kw != land && kw!= lor, tp, exp, kw, tp1, noinline, TRUE);
        castToArithmetic(kw != land && kw!= lor, &tp1, &exp1, (enum e_kw)-1, *tp, noinline, TRUE);
        if (isstructured(*tp) || isstructured(tp1))
            error(ERR_ILL_STRUCTURE_OPERATION);
        else if (isvoid(*tp) || isvoid(tp1))
            error(ERR_NOT_AN_ALLOWED_TYPE);
        else if (basetype(*tp)->scoped || basetype(tp1)->scoped)
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
            *tp = destSize(*tp, tp1, exp, &exp1, TRUE, atp);
        }
        else
        {
            *tp = &stdint;
        }
        if (basetype(*tp)->type == bt_memberptr)
        {
            *exp = exprNode(en_mp_as_bool, *exp, NULL);
            (*exp)->size = (*tp)->size;
        }
        if (basetype(tp1)->type == bt_memberptr)
        {
            exp1 = exprNode(en_mp_as_bool, exp1, NULL);
            exp1->size = (tp1)->size;
        }
        *exp = exprNode(type, *exp, exp1);
    }
    return lex;
}
static LEXEME *expression_and(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL noinline, BOOL inTemplateParams, BOOL packable)
{
    return binop(lex, funcsp, atp, tp, exp, and, en_and, expression_equality, noinline, inTemplateParams, packable);
}
static LEXEME *expression_xor(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL noinline, BOOL inTemplateParams, BOOL packable)
{
    return binop(lex, funcsp, atp, tp, exp, uparrow, en_xor, expression_and, noinline, inTemplateParams, packable);
}
static LEXEME *expression_or(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL noinline, BOOL inTemplateParams, BOOL packable)
{
    return binop(lex, funcsp, atp, tp, exp, or, en_or, expression_xor, noinline, inTemplateParams, packable);
}
static LEXEME *expression_land(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL noinline, BOOL inTemplateParams, BOOL packable)
{
    return binop(lex, funcsp, atp, tp, exp, land, en_land, expression_or, noinline, inTemplateParams, packable);
}
static LEXEME *expression_lor(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL noinline, BOOL inTemplateParams, BOOL packable)
{
    return binop(lex, funcsp, atp, tp, exp, lor, en_lor, expression_land, noinline, inTemplateParams, packable);
}

static LEXEME *expression_hook(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL noinline, BOOL inTemplateParams, BOOL packable)
{
    lex = expression_lor(lex, funcsp, atp, tp, exp, noinline, inTemplateParams, packable);
    if (*tp == NULL)
        return lex;
    if (MATCHKW(lex, hook))
    {
        TYPE *tph = NULL,*tpc = NULL;
        EXPRESSION *eph=NULL, *epc = NULL;
        castToArithmetic(FALSE, tp, exp, (enum e_kw)-1, &stdint, noinline, TRUE);
        if (isstructured(*tp))
            error(ERR_ILL_STRUCTURE_OPERATION);
        else if (isvoid(*tp))
            error(ERR_NOT_AN_ALLOWED_TYPE);
        lex = getsym();
        lex = expression_comma(lex, funcsp, NULL, &tph, &eph, FALSE, noinline, inTemplateParams, packable);
        if (!tph)
        {
            *tp = NULL;
        }
        else if (MATCHKW(lex, colon))
        {
            lex = getsym();
            lex = expression_hook(lex, funcsp, NULL, &tpc, &epc, noinline, inTemplateParams, packable);
            if (!tpc)
            {
                *tp = NULL;
            }
            else
            {
                if (basetype(*tp)->type == bt_memberptr)
                {
                    *exp = exprNode(en_mp_as_bool, *exp, NULL);
                    (*exp)->size = (*tp)->size;
                }
                if (isvoidptr(tpc) && ispointer(tph) || isvoidptr(tph) &&ispointer(tpc))
                    tpc = tph = &stdpointer;
                else if (!((ispointer(tph) || isfunction(tph)) && isconstzero(tpc, epc)) && 
                         !((ispointer(tpc) || isfunction(tpc)) && isconstzero(tph,eph)))
                    if (!comparetypes(tpc, tph, FALSE))
                        if (!(isarithmetic(tpc) && isarithmetic(tph)))
                            errortype(ERR_TWO_OPERANDS_SAME_TYPE, tpc, tph);
                if (!isvoid(tpc))
                    *tp = destSize(tpc,tph,&epc,&eph, FALSE, NULL);
                else 
                    *tp = tpc;
                *exp = exprNode(en_cond, *exp, exprNode(en_void, eph, epc));
                if (isstructured(*tp))
                    *exp = exprNode(en_not_lvalue, *exp, NULL);
            }
        }
        else
        {
            error(ERR_HOOK_NEEDS_COLON);
            *tp = NULL;
        }
    }
    return lex;
}
LEXEME *expression_assign(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL selector, BOOL noinline, BOOL inTemplateParams, BOOL packable)
{
    BOOL done = FALSE;
    EXPRESSION *exp1=NULL;
    EXPRESSION *asndest = NULL;
    TYPE *tp2;
    lex = expression_hook(lex, funcsp, atp, tp, exp, noinline, inTemplateParams, packable);
    if (*tp == NULL)
        return lex;
    while (!done && lex)
    {
        enum e_kw kw = KW(lex);
        enum e_node op;
        enum ovcl selovcl;
        TYPE *tp1 = NULL;
        switch(kw)
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
                done = TRUE;
                continue;
        }
        lex = getsym();
        switch(kw)
        {
            case assign:
                if (cparams.prm_cplusplus && MATCHKW(lex, begin))
                {
                    if (isstructured(*tp))
                    {
                        SYMBOL *sp = anonymousVar(sc_auto, *tp);
                        EXPRESSION *destexp;
                        INITIALIZER *init = NULL, *dest=NULL;
                        SYMBOL *spinit = NULL;
                        destexp = exp1 = varNode(en_auto, sp);
                        tp1 = *tp;
                        spinit = anonymousVar(sc_localstatic, tp1);
                        insert(spinit, localNameSpace->syms);
                        lex = initType(lex, funcsp, NULL, sc_auto, &init, &dest, tp1, spinit, FALSE);
                        exp1 = convertInitToExpression(tp1, NULL, funcsp, init, exp1, noinline);
                    }
                    else
                    {
                        lex = getsym();
                        lex = expression_assign(lex, funcsp, *tp, &tp1, &exp1, FALSE, noinline, inTemplateParams, packable);
                        if (!needkw(&lex, end))
                        {
                            errskim(&lex, skim_end);
                            skip(&lex, end);
                        }
                    }
                }
                else
                {
                    lex = expression_assign(lex, funcsp, *tp, &tp1, &exp1, FALSE, noinline, inTemplateParams, packable);
                }
                break;
            case asplus:
            case asminus:
            case asand:
            case asor:
            case asxor:
                lex = expression_assign(lex, funcsp, *tp, &tp1, &exp1, FALSE, noinline, inTemplateParams, packable);
                break;
            default:
                lex = expression_assign(lex, funcsp, NULL, &tp1, &exp1, FALSE, noinline, inTemplateParams, packable);
                break;
        }
        if (!tp1)
        {
            *tp = NULL;
            return lex;
        }
        if (cparams.prm_cplusplus && insertOperatorFunc(selovcl, kw,
                               funcsp, tp, exp, tp1, exp1, NULL, noinline))
        {
            // unallocated var for destructor
            if (asndest)
            {
                SYMBOL *sp = anonymousVar(sc_auto, tp1);
                callDestructor(sp, &asndest, NULL, TRUE, noinline, FALSE);
                initInsert(&sp->dest, tp1, asndest, 0, TRUE);
            }
            
            continue;
        }
        if (selector)
        {
            /* a little naive... */
            error(ERR_POSSIBLE_INCORRECT_ASSIGNMENT);
            selector = FALSE;
        }
        checkscope(*tp, tp1);
        if (cparams.prm_cplusplus)
        {
            if (isarithmetic(*tp))
            {
                castToArithmetic(FALSE, &tp1, &exp1, (enum e_kw)-1, *tp, noinline, TRUE);
            }
            else if (isstructured(tp1))
            {
                cppCast(*tp, &tp1, &exp1, noinline);
            }
        }
        if (isconst(*tp))
            error(ERR_CANNOT_MODIFY_CONST_OBJECT);
        else if (isvoid(*tp) || isvoid(tp1))
            error(ERR_NOT_AN_ALLOWED_TYPE);
        else if (!isstructured(*tp) && basetype(*tp)->type != bt_memberptr && basetype(*tp)->type != bt_templateparam && !lvalue(*exp))
            error(ERR_LVALUE);
        else switch(kw)
        {
            case asand:
            case asor:
            case asxor:
            case asleftshift:
            case asrightshift:
                if (iscomplex(*tp) || iscomplex(tp1))
                    error(ERR_ILL_USE_OF_COMPLEX);
                if (isfloat(*tp) || isfloat(tp1) || isimaginary(*tp) || isimaginary(tp1))
                    error(ERR_ILL_USE_OF_FLOATING);
                /* fall through */					
            case astimes:
            case asdivide:
            case asmod:
                if (ispointer(*tp) || ispointer(tp1))
                    error(ERR_ILL_POINTER_OPERATION);
                if (isstructured(*tp) || isstructured(tp1))
                    error(ERR_ILL_STRUCTURE_OPERATION);
                if (basetype(*tp)->type == bt_memberptr || basetype(tp1)->type == bt_memberptr)
                    error(ERR_ILLEGAL_USE_OF_MEMBER_PTR);
                break;
            case asplus:
                if (ispointer(*tp))
                {
                    if (ispointer(tp1))
                        error(ERR_ILL_POINTER_ADDITION);
                    else
                    {
                        if (iscomplex(tp1))
                            error(ERR_ILL_USE_OF_COMPLEX);
                        else if (isfloat(tp1) || isimaginary(tp1))
                            error(ERR_ILL_USE_OF_FLOATING);
                        else if (isstructured(tp1))
                            error(ERR_ILL_STRUCTURE_OPERATION);
                        cast((*tp), &exp1);
                        exp1 = exprNode(en_umul, exp1, nodeSizeof(basetype(*tp)->btp, exp1));
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
                    cast((*tp), &exp1);
                    exp1 = exprNode(en_umul, exp1, nodeSizeof(basetype(*tp)->btp, exp1));
                }
                break;
            case assign:
                if (ispointer(*tp))
                {
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
                    }
                    else if (ispointer(tp1))
                    {
                        if (!comparetypes(*tp, tp1, TRUE))
                        {
                            BOOL found = FALSE;
                            if (ispointer(tp1))
                            {
                                TYPE *tpo = basetype(basetype(tp1)->btp);
                                TYPE *tpn = basetype(basetype(*tp)->btp);
                                if (isstructured(tpo) && isstructured(tpn))
                                {
                                    if (classRefCount(tpo->sp, tpn->sp) == 1)
                                    {
                                        EXPRESSION *v = Alloc(sizeof(EXPRESSION));
                                        v->type = en_c_i;
                                        v = baseClassOffset(tpo->sp, tpn->sp, v);
                                        optimize_for_constants(&v);
                                        if (v->type == en_c_i) // check for no virtual base
                                        {
                                            if (isAccessible(tpn->sp, tpn->sp, tpo->sp, funcsp, ac_public, FALSE))
                                            {
                                                *exp = exprNode(en_add, *exp, v);
                                                found = TRUE;
                                            }
                                        }
                                    }
                                    else if (classRefCount(tpn->sp, tpo->sp) == 1)
                                    {
                                        EXPRESSION *v = Alloc(sizeof(EXPRESSION));
                                        v->type = en_c_i;
                                        v = baseClassOffset(tpn->sp, tpo->sp, v);
                                        optimize_for_constants(&v);
                                        if (v->type == en_c_i) // check for no virtual base
                                        {
                                            if (isAccessible(tpo->sp, tpo->sp, tpn->sp, funcsp, ac_public, FALSE))
                                            {
                                                *exp = exprNode(en_sub, *exp, v);
                                                found = TRUE;
                                            }
                                        }
                                    }
                                }
                            }
                            if (!found)
                            {
                                if (cparams.prm_cplusplus)
                                    errortype(ERR_CANNOT_CONVERT_TYPE, tp1, *tp);
                                else if (!isvoidptr(*tp) && !isvoidptr(tp1))
                                    error(ERR_SUSPICIOUS_POINTER_CONVERSION);
                                else if (cparams.prm_cplusplus && !isvoidptr(*tp) && isvoidptr(tp1) && exp1->type != en_nullptr)
                                    error(ERR_ANSI_FORBIDS_IMPLICIT_CONVERSION_FROM_VOID);
                            }
                        }
                    }
                    else if (isfunction(tp1))
                    {
                        if (!isvoidptr(*tp) && 
                            (!isfunction(basetype(*tp)->btp) || !comparetypes(*tp, tp1, TRUE)))
                            error(ERR_SUSPICIOUS_POINTER_CONVERSION);
                    }
                    else 
                        error(ERR_INVALID_POINTER_CONVERSION);
                }
                else if (ispointer(tp1))
                {
                    if (iscomplex(*tp))
                        error(ERR_ILL_USE_OF_COMPLEX);
                    else if (isfloat(*tp) || isimaginary(*tp))
                        error(ERR_ILL_USE_OF_FLOATING);
                    else if (isint(*tp))
                        error(ERR_NONPORTABLE_POINTER_CONVERSION);
                }
                if (isstructured(*tp) && (!isstructured(tp1) || !comparetypes(*tp, tp1, TRUE)))
                    error(ERR_ILL_STRUCTURE_ASSIGNMENT);
                else if (isstructured(*tp) && !(*tp)->size)
                    errorsym(ERR_STRUCT_NOT_DEFINED, basetype(*tp)->sp);
                else if (!isstructured(*tp) && isstructured(tp1))
                    error(ERR_ILL_STRUCTURE_ASSIGNMENT);
                else if (basetype(*tp)->type == bt_memberptr)
                {
                    if (exp1->type == en_memberptr)
                    {
                        if (exp1->v.sp->parentClass != basetype(*tp)->sp && classRefCount(exp1->v.sp->parentClass, basetype(*tp)->sp) != 1)
                            error(ERR_INCOMPATIBLE_TYPE_CONVERSION);

                    }
                    else if (!isconstzero(tp1, exp) && !comparetypes(*tp, tp1, TRUE))
                    {
                        errortype(ERR_CANNOT_CONVERT_TYPE, tp1, *tp);
                    }
                }
                else if (basetype(tp1)->type == bt_memberptr)
                    errortype(ERR_CANNOT_CONVERT_TYPE, tp1, *tp);
                break;
            default:
                done = TRUE;
                break;
        }
        switch(kw)
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
                tp2 = destSize(*tp, tp1, NULL, NULL, FALSE, NULL);
                op = isunsigned(*tp) ? en_udiv : en_div;
                break;
            case asmod:
                tp2 = destSize(*tp, tp1, NULL, NULL, FALSE, NULL);
                op = isunsigned(*tp) ? en_umod : en_mod;
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
                tp2 = destSize(*tp, tp1, NULL, NULL, FALSE, NULL);
                op = isunsigned(*tp) ? en_ursh : en_rsh;
                break;
        }
        if (basetype(*tp)->type == bt_memberptr)
        {
            if ((*exp)->type == en_not_lvalue || (*exp)->type == en_func
                || (*exp)->type == en_void || (*exp)->type == en_memberptr)
            {
                if (basetype(*tp)->type != bt_templateparam)
                    error(ERR_LVALUE);
            }
            else if (exp1->type == en_memberptr)
            {
                int lbl = dumpMemberPtr(exp1->v.sp, *tp, TRUE);
                exp1 = intNode(en_labcon, lbl);
                *exp = exprNode(en_blockassign, *exp, exp1);
                (*exp)->size = (*tp)->size;
            }
            else if (isconstzero(tp1, exp1) || exp1->type == en_nullptr)
            {
                *exp = exprNode(en_blockclear, *exp, NULL);
                (*exp)->size = (*tp)->size;
            }
            else if (exp1->type == en_func && exp1->v.func->returnSP)
            {
                exp1->v.func->returnSP->allocate = FALSE;
                exp1->v.func->returnEXP = *exp;
                *exp = exp1;
            }
            else
            {
                *exp = exprNode(en_blockassign, *exp, exp1);
                (*exp)->size = (*tp)->size;
            }
        }
        else if (isstructured(*tp))
        {
            EXPRESSION *exp2 = exp1;
            if ((*exp)->type == en_not_lvalue || (*exp)->type == en_func
                || (*exp)->type == en_void)
                error(ERR_LVALUE);
            if (lvalue(exp2))
                exp2 = exp2->left;
            if (exp2->type == en_func && exp2->v.func->returnSP)
            {
                exp2->v.func->returnSP->allocate = FALSE;
                exp2->v.func->returnEXP = *exp;
                *exp = exp1;
            }
            else
            {
                *exp = exprNode(en_blockassign, *exp, exp1);
                (*exp)->size = (*tp)->size;
                if (isatomic(*tp) || isatomic(tp1))
                    (*exp)->size -= ATOMIC_FLAG_SPACE;
            }
            *exp = exprNode(en_not_lvalue, *exp, NULL);
        }
        else
        {
            if (kw == assign)
            {
                int n = natural_size(*exp);
                if (natural_size(exp1) != n)
                    cast((*tp), &exp1);
                *exp = exprNode(op, *exp, exp1);
            }
            else {
                EXPRESSION *dest = *exp;
                *exp = RemoveAutoIncDec(*exp);
                // we want to optimize the as* operations for the backend
                // but can't do the optimization for divisions
                // otherwise it is fine for the processor we are on
                if (kw == asmod || kw == asdivide)
                {
                    int n = natural_size(*exp);
                    destSize(*tp, tp1, exp, &exp1, FALSE, NULL);
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
                        cast(*tp, &exp1);
                    *exp = exprNode(op, *exp, exp1);
                    *exp = exprNode(en_assign, dest, *exp);
                }
            }
        }
    }
    return lex;
}
static LEXEME *expression_comma(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL selector, BOOL noinline, BOOL inTemplateParams, BOOL packable)
{
    lex = expression_assign(lex, funcsp, atp, tp, exp, selector, noinline, inTemplateParams, packable);
    if (*tp == NULL)
        return lex;
    while (MATCHKW(lex, comma))
    {
        EXPRESSION *exp1=NULL;
        TYPE *tp1 = NULL;
        lex = getsym();
        lex = expression_assign(lex, funcsp, atp, &tp1, &exp1, selector, noinline, inTemplateParams, packable);
        if (!tp1)
        {
            break;
        }
        if (cparams.prm_cplusplus && insertOperatorFunc(ovcl_comma, comma,
                               funcsp, tp, exp, tp1, exp1, NULL, noinline))
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
LEXEME *expression_no_comma(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, BOOL noinline, BOOL inTemplateParams)
{
    lex = expression_assign(lex, funcsp, atp, tp, exp, FALSE, noinline, inTemplateParams, FALSE);
    assignmentUsages(*exp, FALSE);
    return lex;
}
LEXEME *expression_no_check(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, 
                   BOOL selector, BOOL noinline)
{
    lex = expression_comma(lex, funcsp, atp, tp, exp, selector, noinline, FALSE, FALSE);
    return lex;
}

LEXEME *expression(LEXEME *lex, SYMBOL *funcsp, TYPE *atp, TYPE **tp, EXPRESSION **exp, 
                   BOOL selector, BOOL noinline, BOOL packable)
{
    lex = expression_comma(lex, funcsp, atp, tp, exp, selector, noinline, FALSE, packable);
    assignmentUsages(*exp, FALSE);
    return lex;
}
