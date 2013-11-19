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

extern int currentErrorLine;
extern LIST *externals;

int instantiatingTemplate;
BOOL inTemplateBody;
int templateNestingCount =0 ;

struct listData
{
    struct listData *next;
    TEMPLATEPARAM *args;
    LEXEME *head, *tail;
    LEXEME *bodyHead, *bodyTail;
    SYMBOL *sym;
    
} *currents;

static LEXEME *TemplateArg(LEXEME *lex, SYMBOL *funcsp, TEMPLATEPARAM *arg);

void templateInit(void)
{
    inTemplateBody = FALSE;
    templateNestingCount = 0;
    instantiatingTemplate = 0;
    currents = NULL;
}
void TemplateGetDeferred(SYMBOL *sym)
{
    if (currents)
    {
        sym->deferredTemplateHeader = currents->head;
        sym->deferredCompile = currents->bodyHead;
    }
}
void TemplateRegisterDeferred(LEXEME *lex)
{
    if (templateNestingCount && lex)
    {
        LEXEME *cur = globalAlloc(sizeof(LEXEME));
        *cur = *lex;
        if (inTemplateBody)
        {
            if (currents->bodyHead)
            {
                cur->prev = currents->bodyTail;
                currents->bodyTail = currents->bodyTail->next = cur;
            }
            else
            {
                cur->prev = NULL;
                currents->bodyHead = currents->bodyTail = cur;
            }
        }
        else
        {
            if (currents->head)
            {
                cur->prev = currents->tail;
                currents->tail = currents->tail->next = cur;
            }
            else
            {
                cur->prev = NULL;
                currents->head = currents->tail = cur;
            }
        }
    }
}
static BOOL exactMatchOnTemplateParams(TEMPLATEPARAM *old, TEMPLATEPARAM *sym)
{
    while (old && sym)
    {
        if (old->type != sym->type)
            break;
        if (old->type == kw_template)
        {
            if (!exactMatchOnTemplateParams(old->byTemplate.args, sym->byTemplate.args))
                break;
        }
        old = old->next;
        sym = sym->next;
    }
    return !(old || sym);
}
static BOOL exactMatchOnTemplateArgs(TEMPLATEPARAM *old, TEMPLATEPARAM *sym)
{
    while (old && sym)
    {
        if (old->type != sym->type)
            return FALSE;
        switch (old->type)
        {
            case kw_typename:
                if (!comparetypes(old->byClass.dflt, sym->byClass.dflt, TRUE))
                    return FALSE;
                break;
            case kw_template:
                if (old->byTemplate.dflt != sym->byTemplate.dflt)
                    return FALSE;
                break;
            case kw_int:
                if (!comparetypes(old->byNonType.tp, sym->byNonType.tp, TRUE))
                    return FALSE;
                break;
        }
        old = old->next;
        sym = sym->next;
    }
    return !old && !sym;
}
static TEMPLATEPARAM * mergeTemplateDefaults(TEMPLATEPARAM *old, TEMPLATEPARAM *sym)
{
    TEMPLATEPARAM *rv = sym;
#ifndef PARSER_ONLY
    while (old && sym)
    {
        switch (sym->type)
        {
            case kw_template:
                sym->byTemplate.args = mergeTemplateDefaults(old->byTemplate.args, sym->byTemplate.args);
                if (old->byTemplate.dflt && sym->byTemplate.dflt)
                    errorsym(ERR_MULTIPLE_DEFAULT_VALUES_IN_TEMPLATE_DECLARATION, sym->sym);
                else if (!sym->byTemplate.dflt)
                    sym->byTemplate.dflt = old->byTemplate.dflt;
                break;
            case kw_typename:
                if (old->byClass.dflt && sym->byClass.dflt)
                {
                    errorsym(ERR_MULTIPLE_DEFAULT_VALUES_IN_TEMPLATE_DECLARATION, sym->sym);
                }
                else if (!sym->byClass.dflt)
                    sym->byClass.dflt = old->byClass.dflt;
                break;
            case kw_int:
                if (old->byNonType.dflt && sym->byNonType.dflt)
                {
                    errorsym(ERR_MULTIPLE_DEFAULT_VALUES_IN_TEMPLATE_DECLARATION, sym->sym);
                }
                else if (!sym->byNonType.dflt)
                    sym->byNonType.dflt = old->byNonType.dflt;
                break;
            case kw_new: // specialization
                break;
        }
        old = old->next;
        sym = sym->next;
    }
#endif
    return rv;
}
static void checkTemplateDefaults(TEMPLATEPARAM *args)
{
    SYMBOL *last = NULL;
    while (args)
    {
        void *dflt = NULL;
        switch (args->type)
        {
            case kw_template:
                checkTemplateDefaults(args->byTemplate.args);
                dflt = args->byTemplate.dflt;
                break;
            case kw_typename:
                dflt = args->byClass.dflt;
                break;
            case kw_int:
                dflt = args->byNonType.dflt;
                break;
        }
        if (last && !dflt)
        {
            errorsym(ERR_MISSING_DEFAULT_VALUES_IN_TEMPLATE_DECLARATION, last);
            break;
        }
        if (dflt)
            last = args->sym;
        args = args->next;
    }
}
static BOOL matchTemplateSpecializationToParams(TEMPLATEPARAM *param, TEMPLATEPARAM *special, SYMBOL *sp)
{
    while (param && special)
    {
        if (param->type != special->type)
        {
            errorsym(ERR_INCORRECT_ARGS_PASSED_TO_TEMPLATE, sp);
        }
        else if (param->type == kw_template)
        {
            if (!exactMatchOnTemplateParams(param->byTemplate.args, special->byTemplate.dflt->templateParams->next))
                errorsym(ERR_INCORRECT_ARGS_PASSED_TO_TEMPLATE, sp);
        }
        else if (param->type == kw_int)
        {
            if (!comparetypes(param->byNonType.tp, special->byNonType.tp, TRUE) && (!ispointer(param->byNonType.tp) || !isconstzero(param->byNonType.tp, special->byNonType.dflt)))
                errorsym(ERR_INCORRECT_ARGS_PASSED_TO_TEMPLATE, sp);
        }
        param = param->next;
        special = special->next;
    }
    if (param)
    {
        errorsym(ERR_TOO_FEW_ARGS_PASSED_TO_TEMPLATE, sp);
    }
    else if (special)
    {
        errorsym(ERR_TOO_MANY_ARGS_PASSED_TO_TEMPLATE, sp);
    }
    return !param && !special;
}
TEMPLATEPARAM * TemplateMatching(LEXEME *lex, TEMPLATEPARAM *old, TEMPLATEPARAM *sym, SYMBOL *sp)
{
    TEMPLATEPARAM *rv = NULL;
    if (old)
    {
        if (sym->bySpecialization.types)
        {
            matchTemplateSpecializationToParams(old->next, sym->bySpecialization.types, sp);
            rv = sym;
        }
        else if (!exactMatchOnTemplateParams(old->next, sym->next))
        {
            error(ERR_TEMPLATE_DEFINITION_MISMATCH);
        }
        else 
        {
            rv = mergeTemplateDefaults(old, sym);
            checkTemplateDefaults(rv);
        }
    }
    else
    {
        checkTemplateDefaults(sym->next);
    }
    return rv;
}
static BOOL typeHasTemplateArg(TYPE *t)
{
    while (ispointer(t) || isref(t))
        t = t->btp;
    if (isfunction(t))
    {
        HASHREC *hr;
        t = basetype(t);
        if (typeHasTemplateArg(t->btp))
            return TRUE;
        hr = t->syms->table[0];
        while (hr)
        {
            if (typeHasTemplateArg(((SYMBOL *)hr->p)->tp))
                return TRUE;
            hr = hr->next;
        }
            
    }
    else if (basetype(t)->type == bt_templateparam)
        return TRUE;
    return FALSE;
}
void TemplateValidateSpecialization(TEMPLATEPARAM *arg)
{
    TEMPLATEPARAM *t = arg->bySpecialization.types;
    while (t)
    {
        if (t->type == kw_typename && typeHasTemplateArg((TYPE *)t->byClass.val))
            break;
        t = t->next;
    }
    if (!t)
    {
        error (ERR_PARTIAL_SPECIALIZATION_MISSING_TEMPLATE_PARAMETERS);
    }
}
LEXEME *GetTemplateArguments(LEXEME *lex, SYMBOL *funcsp, TEMPLATEPARAM **lst)
{
    // entered with lex set to the opening <
    lex = getsym();
    if (!MATCHKW(lex, rightshift) && !MATCHKW(lex, gt))
    {
        do
        {
            TYPE *tp = NULL;
            if (startOfType(lex))
            {        
                lex = get_type_id(lex, &tp, funcsp, FALSE);
                if (isstructured(tp) && basetype(tp)->sp->isTemplate)
                {
                    *lst = Alloc(sizeof(TEMPLATEPARAM));
                    (*lst)->type = kw_template;
                    (*lst)->byTemplate.dflt = basetype(tp)->sp;
                    lst = &(*lst)->next;
                }
                else
                {
                    *lst = Alloc(sizeof(TEMPLATEPARAM));
                    (*lst)->type = kw_typename;
                    (*lst)->byClass.dflt = tp;
                    lst = &(*lst)->next;
                }
            }
            else
            {
                EXPRESSION *exp;
                TYPE *tp;
    join:
                exp = NULL;
                tp = NULL;
                lex = expression_no_comma(lex, funcsp, NULL, &tp, &exp, TRUE, TRUE);
                if (!tp)
                {
                    error(ERR_EXPRESSION_SYNTAX);
                    
                }
                else
                {
                    *lst = Alloc(sizeof(TEMPLATEPARAM));
                    (*lst)->type = kw_int;
                    (*lst)->byNonType.dflt = exp;
                    (*lst)->byNonType.tp = tp;
                    lst = &(*lst)->next;
                }
            }
            if (MATCHKW(lex, comma))
                lex = getsym();
            else
                break;
        } while (TRUE);
    }
    if (MATCHKW(lex, rightshift))
        lex = getGTSym(lex);
    else
        needkw(&lex, gt);
    return lex;
}
SYMBOL *LookupSpecialization(SYMBOL *sym, TEMPLATEPARAM *templateParams)
{
    SYMBOL *candidate;
    LIST *lst = sym->specializations;
    while (lst)
    {
        candidate = (SYMBOL *)lst->data;
        if (exactMatchOnTemplateArgs(templateParams->bySpecialization.types, candidate->templateParams->bySpecialization.types))
            return candidate;
        lst = lst->next;
    }
    candidate = clonesym(sym);
    candidate->templateParams = templateParams;
    lst = Alloc(sizeof(LIST));
    lst->data = candidate;
    lst->next = sym->specializations;
    sym->specializations = lst;
    candidate->specialized = TRUE;
    candidate->parentTemplate = sym;
    return candidate;
}
static BOOL matchTemplatedType(TYPE *old, TYPE *sym, BOOL strict)
{
    while (1)
    {
        if (isconst(old) == isconst(sym) && isvolatile(old) == isvolatile(sym))
        {
            old = basetype(old);
            sym = basetype(sym);
            if (old->type == sym->type || isfunction(old) && isfunction(sym))
            {
                switch (old->type)
                {
                    case bt_struct:
                    case bt_class:
                    case bt_union:                
                        return old->sp == sym->sp;
                    case bt_func:
                    case bt_ifunc:
                        if (!matchTemplatedType(old->btp, sym->btp, strict))
                            return FALSE;
                        {
                            HASHREC *hro = old->syms->table[0];
                            HASHREC *hrs = sym->syms->table[0];
                            while (hro && hrs)
                            {
                                if (!matchTemplatedType(((SYMBOL *)hro->p)->tp, ((SYMBOL *)hrs->p)->tp, strict))
                                    return FALSE;
                                hro = hro->next;
                                hrs = hrs->next;
                            }
                            return !hro && !hrs;
                        }
                    case bt_pointer:
                    case bt_lref:
                    case bt_rref:
                        if (old->array == sym->array && old->size == sym->size)
                        {
                            old = old->btp;
                            sym = sym->btp;
                            break;
                        }
                        return FALSE;
                    case bt_templateparam:
                        return old->templateParam->type == sym->templateParam->type;
                    default:
                        return TRUE;
                }
            }
            else
            {
                return !strict && old->type == bt_templateparam;
            }
        }
        else
        {
            return FALSE;
        }
    }
}
static BOOL specializationFunctionsMatch(TEMPLATEPARAM *params, TEMPLATEPARAM *args)
{
    while (params && args)
    {
        if (params->type != args->type)
            return FALSE;
        params = params->next;
        args = args->next;
    }
    return !params && !args;
}
SYMBOL *LookupFunctionSpecialization(SYMBOL *overloads, SYMBOL *sp, TEMPLATEPARAM *templateParams)
{
    HASHREC *hr = overloads->tp->syms->table[0];
    while (hr)
    {
        SYMBOL *hrs = (SYMBOL *)hr->p;
        if (hrs->isTemplate)
        {
            if (templateParams->bySpecialization.types == NULL)
            {
                if (exactMatchOnTemplateParams(hrs->templateParams->next, templateParams->next))
                {
                    if (matchTemplatedType(hrs->tp, sp->tp, TRUE))
                        return hrs;
                }
            }
            else
            {
                if (specializationFunctionsMatch(hrs->templateParams->next, templateParams->bySpecialization.types))                        
                {
                    if (matchTemplatedType(hrs->tp, sp->tp, FALSE))
                    {
                        LIST *lst = hrs->specializations;
                        while (lst)
                        {
                            SYMBOL *hrs1 = (SYMBOL *)lst->data;
                            if (exactMatchOnTemplateArgs(hrs1->templateParams->bySpecialization.types, templateParams->bySpecialization.types))
                                return hrs;
                            lst = lst->next;
                        }
                        lst = Alloc(sizeof(LIST));
                        lst->data = sp;
                        lst->next = hrs->specializations;
                        hrs->specializations = lst;
                        sp->specialized = TRUE;
                        sp->parentTemplate = hrs;
                        return sp;                        
                    }
                }
            }
        }
        hr = hr->next;
    }
    return NULL;
}
static LEXEME *TemplateHeader(LEXEME *lex, SYMBOL *funcsp, TEMPLATEPARAM **args)
{
    if (needkw(&lex, lt))
    {
        while (1)
        {
            if (MATCHKW(lex, gt) || MATCHKW(lex, rightshift))
               break;
            *args = Alloc(sizeof(TEMPLATEPARAM));
            lex = TemplateArg(lex, funcsp, *args);
            if (*args)
                args = &(*args)->next;
            if (!MATCHKW(lex, comma))
                break;
            lex = getsym();
        }
        if (MATCHKW(lex, rightshift))
            lex = getGTSym(lex);
        else
            needkw(&lex, gt);
    }
    return lex;
}
static LEXEME *TemplateArg(LEXEME *lex, SYMBOL *funcsp, TEMPLATEPARAM *arg)
{
    switch (KW(lex))
    {
        TYPE *tp, *tp1;
        EXPRESSION *exp1;
        SYMBOL *sp;
        case kw_class:
        case kw_typename:
            arg->type = kw_typename;
            arg->packed = FALSE;
            lex = getsym();
            if (MATCHKW(lex, ellipse))
            {
                arg->packed = TRUE;
                lex = getsym();
            }
            if (ISID(lex) || MATCHKW(lex, classsel))
            {
                SYMBOL *sym = NULL, *strsym = NULL;
                NAMESPACEVALUES *nsv = NULL;
                BOOL qualified = MATCHKW(lex, classsel);
                
                lex = nestedSearch(lex, &sym, &strsym, &nsv, NULL, FALSE);
                if (qualified || strsym || nsv)
                {
                    lex = getsym();
                    if (sym && istype(sym))
                    {
                        if (ISID(lex))
                        {
                            tp = sym->tp;
                            sp = makeID(sc_templateparam, tp, NULL, litlate(lex->value.s.a));
                            lex = getsym();
                            goto non_type_join;
                        }
                        else
                        {
                            error(ERR_IDENTIFIER_EXPECTED);
                            break;
                        }
                    }
                    else
                    {
                        error(ERR_TYPE_NAME_EXPECTED);
                        break;
                    }
                }
                else
                {
                    TYPE *tp = Alloc(sizeof(TYPE));
                    tp->type = bt_templateparam;
                    tp->templateParam = arg;
                    arg->sym = makeID(sc_templateparam, tp, NULL, litlate(lex->value.s.a));
                    lex = getsym();
                }
            }
            else
            {
                TYPE *tp = Alloc(sizeof(TYPE));
                tp->type = bt_templateparam;
                tp->templateParam = arg;
                arg->sym = makeID(sc_templateparam, tp, NULL, "-tpl-");
            }
            if (MATCHKW(lex, assign))
            {
                if (arg->packed)
                {
                    error(ERR_CANNOT_USE_DEFAULT_WITH_PACKED_TEMPLATE_PARAMETER);
                }
                lex = getsym();
                lex = get_type_id(lex, &arg->byClass.dflt, funcsp, FALSE);
                if (!arg->byClass.dflt)
                {
                    error(ERR_CLASS_TEMPLATE_DEFAULT_MUST_REFER_TO_TYPE);
                }
            }
            if (!MATCHKW(lex, gt) && !MATCHKW(lex, leftshift) && !MATCHKW(lex, comma))
            {
                error(ERR_IDENTIFIER_EXPECTED);
            }
            break;
        case kw_template:
            arg->type = kw_template;
            lex = getsym();
            lex = TemplateHeader(lex, funcsp, &arg->byTemplate.args);
            arg->packed = FALSE;
            if (!MATCHKW(lex, kw_class))
            {
                error(ERR_TEMPLATE_TEMPLATE_PARAMETER_MUST_NAME_CLASS);
            }       
            else
            {
                lex = getsym();
            }
            if (MATCHKW(lex, ellipse))
            {
                arg->packed = TRUE;
                lex = getsym();
            }
            if (ISID(lex))
            {
                TYPE *tp = Alloc(sizeof(TYPE));
                tp->type = bt_templateparam;
                tp->templateParam = arg;
                arg->sym = makeID(sc_templateparam, tp, NULL, litlate(lex->value.s.a));
                lex = getsym();
            }
            else
            {
                TYPE *tp = Alloc(sizeof(TYPE));
                tp->type = bt_templateparam;
                tp->templateParam = arg;
                arg->sym = makeID(sc_templateparam, tp, NULL, "-tpl-");
            }
            if (MATCHKW(lex, assign))
            {
                if (arg->packed)
                {
                    error(ERR_CANNOT_USE_DEFAULT_WITH_PACKED_TEMPLATE_PARAMETER);
                }
                lex = getsym();
                if (ISID(lex))
                {
                    lex = id_expression(lex, funcsp, &arg->byTemplate.dflt, NULL, NULL, FALSE, FALSE, lex->value.s.a);
                    if (!arg->byTemplate.dflt)
                    {
                        currentErrorLine = 0;
                        errorstr(ERR_UNDEFINED_IDENTIFIER,lex->value.s.a);
                    }
                    lex = getsym();
                }
                else
                {
                    error(ERR_TEMPLATE_TEMPLATE_PARAMETER_MISSING_DEFAULT);
                }
            }
            if (!MATCHKW(lex, gt) && !MATCHKW(lex, leftshift) && !MATCHKW(lex, comma))
            {
                error(ERR_IDENTIFIER_EXPECTED);
            }
            break;
        default: // non-type
        {
            enum e_lk linkage = lk_none, linkage2 = lk_none, linkage3 = lk_none;
            BOOL defd = FALSE;
            BOOL notype = FALSE;
            arg->type = kw_int;
            arg->packed = FALSE;
            tp = NULL;
            sp = NULL;
            lex = getQualifiers(lex, &tp, &linkage, &linkage2, &linkage3);
            lex = getBasicType(lex, funcsp, &tp, NULL, funcsp ? sc_auto : sc_global, &linkage, &linkage2, &linkage3, ac_public, &notype, &defd, NULL);
            lex = getQualifiers(lex, &tp, &linkage, &linkage2, &linkage3);
            lex = getBeforeType(lex, funcsp, &tp, &sp, NULL, NULL, NULL, sc_cast, &linkage, &linkage2, &linkage3, FALSE, FALSE, FALSE); /* fixme at file scope init */
            sizeQualifiers(tp);
            if (!tp)
            {
                error(ERR_TYPE_NAME_EXPECTED);
            }
            else if (!sp)
            {
                error(ERR_IDENTIFIER_EXPECTED);
            }
            else
            {
                TYPE *tpa;
non_type_join:
                tpa = Alloc(sizeof(TYPE));
                tpa->type = bt_templateparam;
                tpa->templateParam = arg;
                sp->storage_class = sc_templateparam;
                sp->tp = tpa;
                arg->sym = sp;
                if (isarray(tp) || isfunction(tp))
                {
                    if (isarray(tp))
                        tp = tp->btp;
                    tp1 = Alloc(sizeof(TYPE));
                    tp1->type = bt_pointer;
                    tp1->size = getSize(bt_pointer);
                    tp1->btp = tp;
                    tp = tp1;
                }
                arg->byNonType.tp = tp;
                if (tp->type != bt_templateparam && !isint(tp) && !ispointer(tp) && basetype(tp)->type != bt_lref)
                {
                    error(ERR_NONTYPE_TEMPLATE_PARAMETER_INVALID_TYPE);
                }
                if (sp)
                {
                    if (MATCHKW(lex, assign))
                    {
                        tp1 = NULL;
                        exp1 = NULL;     
                        lex = getsym();
                        lex = expression_no_comma(lex, funcsp, NULL, &tp1, &exp1, TRUE, TRUE);
                        arg->byNonType.dflt = exp1;
                        if (!comparetypes(tp, tp1, TRUE))
                        {
                            if (!ispointer(tp) || !isint(tp1) || !isconstzero(tp1, exp1))
                                error(ERR_TYPE_NONTYPE_TEMPLATE_PARAMETER_DEFAULT_TYPE_MISMATCH);
                        }
                    }
                }
            }
            break;
        }
    }
    
    return lex;
}
static BOOL matchArg(TEMPLATEPARAM *param, TEMPLATEPARAM *arg)
{
    if (param->type != arg->type)
    {
        return FALSE;
    }
    else if (param->type == kw_template)
    {
        if (!exactMatchOnTemplateParams(param->byTemplate.args, arg->byTemplate.dflt->templateParams->next))
            return FALSE;
    }
    else if (param->type == kw_int)
    {
        if (!comparetypes(param->byNonType.tp, arg->byNonType.tp, TRUE) && (!ispointer(param->byNonType.tp) || !isconstzero(param->byNonType.tp, arg->byNonType.dflt)))
            return FALSE;
    }
    return TRUE;
}
BOOL TemplateIntroduceArgs(TEMPLATEPARAM *sym, TEMPLATEPARAM *args)
{
    sym = sym->next;
    while (sym && args)
    {
        if (!matchArg(sym, args))
            return FALSE;
        switch(args->type)
        {
            case kw_typename:
                sym->byClass.val = args->byClass.dflt;
                break;
            case kw_template:
                sym->byTemplate.val = args->byTemplate.dflt;
                break;
            case kw_int:
                sym->byNonType.val = args->byNonType.dflt;
                break;
        }
        sym = sym->next;
        args = args->next;
    }
    return TRUE;
}
static TYPE *SynthesizeTemplate(TYPE *tp, BOOL alt)
{
    SYMBOL *rv;
    TEMPLATEPARAM *r = NULL, **last = &r;
    TEMPLATEPARAM *p = tp->sp->templateParams->bySpecialization.types;
    if (!p)
        p = tp->sp->templateParams->next;
    while (p)
    {
        *last = Alloc(sizeof(TEMPLATEPARAM));
        **last = *p;
        last = &(*last)->next;
        p = p->next;
    }
    p = r;
    while (p)
    {
        switch(p->type)
        {
            case kw_typename:
                if (p->byClass.val->type == bt_templateparam)
                {
                    p->byClass.val = p->byClass.val->templateParam->byClass.val;
                }
                break;
            case kw_template:
                if (p->byTemplate.val->tp->type == bt_templateparam)
                {
                    p->byTemplate.val = SynthesizeTemplate( p->byTemplate.val->tp, TRUE);
                }
                break;
            case kw_int:
                if (p->byNonType.tp->type == bt_templateparam)
                {
                    p->byNonType.val = p->byNonType.tp->templateParam->byNonType.val;
                }
                break;
        }
        p = p->next;
    }
    rv = clonesym(tp->sp);
    rv->tp = Alloc(sizeof(TYPE));
    *rv->tp = *tp;
    rv->tp->sp = rv;
    rv->templateParams = Alloc(sizeof(TEMPLATEPARAM));
    rv->templateParams->bySpecialization.types = r;
    rv->templateParams->type = kw_new; // specialization
    return rv->tp;
}

static TYPE *SynthesizeType(TYPE *tp, BOOL alt)
{
    TYPE *rv = NULL, **last = &rv;
    while (1)
    {
        switch(tp->type)
        {
            case bt_pointer:
                *last = Alloc(sizeof(TYPE));
                **last = *tp;
                last = &(*last)->btp;
                if (isarray(tp) && tp->etype)
                {
                    tp->etype = SynthesizeType(tp->etype, alt);
                }
                tp = tp->btp;
                break;
            case bt_templateselector:
            {
                SYMBOL *ts = (SYMBOL *)tp->sp->templateSelector->next->data;
                LIST *find = tp->sp->templateSelector->next->next;
                tp = ts->tp;
                if (tp->templateParam->type == kw_typename)
                {
                    SYMBOL *sp;
                    tp = alt ? tp->templateParam->byClass.temp : tp->templateParam->byClass.val;
                    sp = tp->sp;
                    while (find && sp)
                    {
                        if (!isstructured(tp))
                            break;
                        
                        sp = search(find->data, tp->syms);
                        if (sp)
                            tp = sp->tp;
                        find = find->next;
                    }
                    if (!search)
                    {
                        *last = tp;
                        return rv;
                    }
                }
                return NULL;
                
            }         
            case bt_const:
            case bt_volatile:
            case bt_lref:
            case bt_rref:
            case bt_restrict:
            case bt_far:
            case bt_near:
            case bt_seg:
                *last = Alloc(sizeof(TYPE));
                **last = *tp;
                last = &(*last)->btp;
                tp = tp->btp;
                break;
            case bt_memberptr:
                
                *last = Alloc(sizeof(TYPE));
                **last = *tp;
                last = &(*last)->btp;
                if (tp->sp->tp->type == bt_templateparam)
                {
                    tp->sp = basetype(tp->sp->tp->templateParam->byClass.val)->sp;
                }
                tp = tp->btp;
                break;
            case bt_func:
            case bt_ifunc:
            {
                HASHREC *hr = tp->syms->table[0], **store;
                *last = Alloc(sizeof(TYPE));
                **last = *tp;
                (*last)->syms = CreateHashTable(1);
                while (hr)
                {
                    SYMBOL *clone = clonesym((SYMBOL *)hr->p);
                    insert(clone, (*last)->syms);
                    clone->tp = SynthesizeType(clone->tp, alt);
                    hr = hr->next;
                }
                last = &(*last)->btp;
                tp = tp->btp;
                break;
            }
            case bt_templateparam:
                if (tp->templateParam->type == kw_typename)
                {
                    *last = alt ? tp->templateParam->byClass.temp : tp->templateParam->byClass.val;
                    return rv;
                }
                else
                {
                    return NULL;
                }
            default:
                *last = tp;
                return rv;
        }
    }
}
static SYMBOL *SynthesizeResult(SYMBOL *sym)
{
    SYMBOL *rsv;
    TEMPLATEPARAM *param = sym->templateParams->next;
    while (param)
    {
        while (param)
        {
            switch(param->type)
            {
                case kw_typename:
                    if (!param->byClass.val)
                    {
                        if (!param->byClass.dflt)
                            return NULL;
                        param->byClass.val = param->byClass.dflt;
                    }
                    break;
                case kw_template:
                    if (!param->byTemplate.val)
                    {
                        if (!param->byTemplate.dflt)
                            return NULL;
                        param->byTemplate.val = param->byTemplate.dflt;
                    }
                    break;
                case kw_int:
                    if (!param->byNonType.val)
                    {
                        if (!param->byNonType.dflt)
                            return FALSE;
                        param->byNonType.val = param->byNonType.dflt;
                    }
                    break;
            }
            param = param->next;
        }
    }
    rsv = clonesym(sym);
    rsv->parentTemplate = sym;
    rsv->tp = SynthesizeType(sym->tp, FALSE);
    return rsv;
}
static TYPE *removeTLQuals(TYPE *A)
{
    /*
    TYPE *x = A, *rv=NULL, **last = &rv;
    while (ispointer(x))
        x = basetype(x)->btp;
    if (!isconst(x) && !isvolatile(x))
        return A;
    x = A;
    while (ispointer(x))
    {
        *last = Alloc(sizeof(TYPE));
        **last = x;
        last = &(*last)->next;
        x = x->btp; // no basetype to get CV quals
    }
    x = basetype(x); // ignore CV quals
    *last = x;
    return rv;
    */
    return basetype(A);
}
static TYPE *rewriteNonRef(TYPE *A)
{
    if (isarray(A))
    {
        TYPE *x = Alloc(sizeof(TYPE));
        x->type = bt_pointer;
        x->size = getSize(bt_pointer);
        while (isarray(A))
            A = basetype(A)->btp;
        x->btp = A;
        A = x;
    } 
    else if (isfunction(A))
    {
        TYPE *x = Alloc(sizeof(TYPE));
        x->type = bt_pointer;
        x->size = getSize(bt_pointer);
        A=basetype(A);
        x->btp = A;
        return x;
    }
    return removeTLQuals(A);
}
static BOOL hastemplate(EXPRESSION *exp)
{
    if (!exp)
        return FALSE;
    if (exp->type == en_templateparam)
        return TRUE;
    return hastemplate(exp->left)  || hastemplate(exp->right);
}
static void clearoutDeduction(TYPE *tp)
{
    while (1)
    {
        switch(tp->type)
        {
            case bt_pointer:
                if (isarray(tp) && tp->etype)
                {
                    clearoutDeduction(tp->etype);
                }
                tp = tp->btp;
                break;
            case bt_templateselector:
                clearoutDeduction(((SYMBOL *)tp->sp->templateSelector->next->data)->tp);
                return;
            case bt_const:
            case bt_volatile:
            case bt_lref:
            case bt_rref:
            case bt_restrict:
            case bt_far:
            case bt_near:
            case bt_seg:
                tp = tp->btp;
                break;
            case bt_memberptr:
                clearoutDeduction(tp->sp->tp);
                tp = tp->btp;
                break;
            case bt_func:
            case bt_ifunc:
            {
                HASHREC *hr = tp->syms->table[0];
                while (hr)
                {
                    clearoutDeduction(((SYMBOL *)hr->p)->tp);
                    hr = hr->next;
                }
                tp = tp->btp;
                break;
            }
            case bt_templateparam:
                tp->templateParam->byClass.temp = NULL;
                return ;
            default:
                return;
        }
    }
}
static void ClearArgValues(TEMPLATEPARAM *params)
{
    while (params)
    {
        params->byClass.val = params->byClass.temp = NULL;
        params = params->next;
    }
}
static BOOL Deduce(TYPE *P, TYPE *A, BOOL change);
static BOOL DeduceFromTemplates(TYPE *P, TYPE *A, BOOL change)
{
    TEMPLATEPARAM *TP = P->sp->templateParams->next;
    TEMPLATEPARAM *TA = A->sp->templateParams->bySpecialization.types;
    if (P->sp != A->sp)
        return FALSE;
    while (TP && TA)
    {
        if (TP->type != TA->type)
            return FALSE;
        switch (TP->type)
        {
            case kw_typename:
            {
                TYPE **tp = change ? &TP->byClass.val : &TP->byClass.temp;
                if (*tp)
                {
                    if (!comparetypes(*tp, TA->byClass.val, TRUE))
                        return FALSE;
                }
                *tp = TA->byClass.val;
                return TRUE;
            }
            case kw_template:
            {
                TEMPLATEPARAM *paramT = TP->sym->templateParams;
                TEMPLATEPARAM *paramA = TA->sym->templateParams;
                while (paramT && paramA)
                {
                    if (paramT->type != paramA->type)
                        return FALSE;
                        
                    paramT = paramT->next;
                    paramA = paramA->next;
                }
                if (paramT || paramA)
                    return FALSE;
                if (!TP->byTemplate.val)
                    TP->byTemplate.val = TA->byTemplate.val;
                else if (!DeduceFromTemplates(TP->byTemplate.val, TA->byTemplate.val, change))
                    return FALSE;
                break;
            }
            case kw_int:
            {
                TYPE **tp = TP->byNonType.tp;
                EXPRESSION **exp = change ? &TP->byNonType.val : &TP->byNonType.val;
                if (*exp)
                {
                    if (!comparetypes(*tp, TA->byNonType.tp, TRUE))
                        return FALSE;
#ifndef PARSER_ONLY
                    if (!equalnode(*exp, TA->byNonType.val))
                        return FALSE;
#endif
                }
                *tp = TA->byNonType.tp;
                *exp = TA->byNonType.val;
                break;
            }
        }
        TP = TP->next;
        TA = TA->next;
    }
    return !TP && !TA;
}
static BOOL Deduce(TYPE *P, TYPE *A, BOOL change)
{
    while (1)
    {
        if (isconst(A) && !isconst(P) || isvolatile(A) && !isvolatile(P))
            return FALSE;
        A = basetype(A);
        P = basetype(P);
        if (isstructured(P) && P->sp->isTemplate)
            return DeduceFromTemplates(P, A, change);
        if (A->type != P->type && P->type != bt_templateparam)
            return FALSE;
        switch(P->type)
        {
            case bt_pointer:
                if (isarray(P) && P->etype)
                {
                    if (hastemplate(P->esize))
                        return FALSE;
                }
                P = P->btp;
                A = A->btp;
                break;
            case bt_templateselector:
                return FALSE;
            case bt_lref:
            case bt_rref:
            case bt_restrict:
            case bt_far:
            case bt_near:
            case bt_seg:
                P = P->btp;
                A = A->btp;
                break;
            case bt_memberptr:
                return Deduce(P->sp->tp, A->sp->tp, change) && Deduce(P->btp, A->btp, change);                
            case bt_func:
            case bt_ifunc:
            {
                HASHREC *hrp = P->syms->table[0];
                HASHREC *hra = A->syms->table[0];
                clearoutDeduction(P);
                if (!Deduce(P->btp, A->btp, FALSE))
                    return FALSE;
                
                while (hra && hrp)
                {
                    if (!Deduce(((SYMBOL *)hra->p)->tp, ((SYMBOL *)hra->p)->tp, FALSE))
                        return FALSE;
                    hrp = hrp->next;
                    hra = hra->next;
                }
                if (hra)
                    return FALSE;
                if (hrp && !((SYMBOL *)hrp->p)->init)
                    return FALSE;
                hrp = P->syms->table[0];
                hra = A->syms->table[0];
                clearoutDeduction(P);
                Deduce(P->btp, A->btp, TRUE);
                
                while (hra && hrp)
                {
                    Deduce(((SYMBOL *)hra->p)->tp, ((SYMBOL *)hra->p)->tp, TRUE);
                    hrp = hrp->next;
                    hra = hra->next;
                }
                return TRUE;
            }
            case bt_templateparam:
                if (P->templateParam->type == kw_typename)
                {
                    TYPE **tp = change ? &P->templateParam->byClass.val : &P->templateParam->byClass.temp;
                    if (*tp)
                    {
                        if (!P->templateParam->initialized && !comparetypes(*tp, A, TRUE))
                            return FALSE;
                    }
                    else
                    {
                        *tp = A;
                    }
                    return TRUE;
                }
                else
                {
                    return FALSE;
                }
            default:
                
                return TRUE;
        }
    }
}
static int eval(EXPRESSION *exp)
{
    optimize_for_constants(&exp);
    if (IsConstantExpression(exp, FALSE))
        return exp->v.i;
    return 0;
}
static BOOL ValidExp(EXPRESSION **exp_in)
{
    BOOL rv = TRUE;
    EXPRESSION *exp = *exp_in;
    if (exp->left)
        rv &= ValidExp(&exp->left);
    if (exp->right)
        rv &= ValidExp(&exp->right);
    if (exp->type == en_templateparam)
        if (!exp->v.sp->templateParams->byClass.val)
            return FALSE;
    return rv;
}
static BOOL ValidArg(TYPE *tp)
{
    while (1)
    {
        switch(tp->type)
        {
            case bt_pointer:
                if (isarray(tp))
                {
                    while (isarray(tp))
                    {
                        tp = basetype(tp)->btp;
                        if (tp->etype)
                        {
                            int n = eval(tp->esize);
                            if (n <= 0)
                                return FALSE;
                        }
                    }
                    if (tp->type == bt_templateparam)
                    {
                        if (tp->templateParam->type != kw_typename)
                            return FALSE;
                        tp = tp->templateParam->byClass.val;
                    }
                    if (tp->type == bt_void || isfunction(tp) || isref(tp) || isstructured(tp) && tp->sp->isabstract)
                        return FALSE;
                }
                if (ispointer(tp))
                {
                    while (ispointer(tp))
                        tp = tp->btp;
                    if (tp->type == bt_templateparam)
                    {
                        if (tp->templateParam->type != kw_typename)
                            return FALSE;
                        tp = tp->templateParam->byClass.val;
                    }
                    if (isref(tp))
                        return FALSE;
                }
                return TRUE;
            case bt_templateselector:
            {
                SYMBOL *ts = (SYMBOL *)tp->sp->templateSelector->next->data;
                LIST *find = tp->sp->templateSelector->next->next;
                tp = ts->tp;
                if (tp->templateParam->type == kw_typename)
                {
                    SYMBOL *sp;
                    tp = tp->templateParam->byClass.val;
                    sp = tp->sp;
                    while (find && sp)
                    {
                        if (!isstructured(tp))
                            break;
                        
                        sp = search(find->data, tp->syms);
                        if (sp)
                            tp = sp->tp;
                        find = find->next;
                    }
                    return !search && sp && istype(sp->tp) ;
                }
                return FALSE;                
                break;
            }
            case bt_lref:
            case bt_rref:
                tp = basetype(tp)->btp;
                if (tp->type == bt_templateparam)
                {
                    if (tp->templateParam->type != kw_typename)
                        return FALSE;
                    tp = tp->templateParam->byClass.val;
                }
                if (isref(tp))
                    return FALSE;
                break;
            case bt_memberptr:
                tp = tp->sp->tp;
                if (tp->type == bt_templateparam)
                {
                    if (tp->templateParam->type != kw_typename)
                        return FALSE;
                    tp = tp->templateParam->byClass.val;
                }
                if (!isstructured(tp))
                    return FALSE;
                tp = tp->btp;
                break;
            case bt_const:
            case bt_volatile:
            case bt_restrict:
            case bt_far:
            case bt_near:
            case bt_seg:
                tp = tp->btp;
                break;
            case bt_func:
            case bt_ifunc:
            {
                HASHREC *hr = tp->syms->table[0];
                while (hr)
                {
                    if (!ValidArg(((SYMBOL *)hr->p)->tp))
                        return FALSE;
                    hr = hr->next;
                }
                tp = tp->btp;
                if (tp->type == bt_templateparam)
                {
                    if (tp->templateParam->type != kw_typename)
                        return FALSE;
                    tp = tp->templateParam->byClass.val;
                }
                if (isfunction(tp) || isarray(tp) || isstructured(tp) && tp->sp->isabstract)
                    return FALSE;                
                break;
            }
            case bt_templateparam:
                if (tp->templateParam->type != kw_typename)
                    return FALSE;
                return ValidArg(tp->templateParam->byClass.val);
            case bt_void:
                return FALSE;
            default:
                return TRUE;
        }
    }
}
static BOOL ValidateArgsSpecified(TEMPLATEPARAM *params, SYMBOL *func, INITLIST *args)
{
    BOOL usesParams = !!args;
    INITLIST *check = args;
    HASHREC *hr = basetype(func->tp)->syms->table[0];
    while (params && (!usesParams || args))
    {
        if (!params->byClass.val)
            params->byClass.val = params->byClass.dflt;
        if (!params->byClass.val)
            return FALSE;
         if (args)
            args = args->next;
        params = params->next;
    }
    if (hr)
    {
        args = check;
        while (args && hr)
        {
            args = args->next;
            hr = hr->next;
        }
        while (hr)
        {
            SYMBOL *sp = (SYMBOL *)hr->p;
            if (sp->init && !ValidExp(sp->init->exp))
                return FALSE;
            hr = hr->next;
        }
    }
    if (!ValidArg(basetype(func->tp)->btp))
        return FALSE;
    hr = basetype(func->tp)->syms->table[0];
    while (hr && (!usesParams || check))
    {
        if (!ValidArg(((SYMBOL *)hr->p)->tp))
            return FALSE;
        if (check)
            check = check->next;
        hr = hr->next;
    }
    return TRUE;
}
static BOOL TemplateDeduceFromArg(TYPE *orig, TYPE *sym, EXPRESSION *exp)
{
    TYPE *P=orig, *A=sym;
    if (!isref(P))
    {
        A = rewriteNonRef(A);
    }
    P = removeTLQuals(P);
    if (isref(P))
        P = basetype(P)->btp;
    if (basetype(orig)->type == bt_rref)
    {
        if (lvalue(exp))
        {
            TYPE *x = Alloc(sizeof(TYPE));
            if (isref(A))
                A = basetype(A)->btp;
            x->type = bt_lref;
            x->size = getSize(bt_pointer);
            x->btp = A;
        }
    }
    if (Deduce(P, A, TRUE))
        return TRUE;
    if (isfuncptr(P) || isref(P) && isfunction(basetype(P)->btp))
    {
        if (exp->type == en_func)
        {
            if (exp->v.sp->storage_class == sc_overloads)
            {
                HASHREC *hr = basetype(exp->v.sp->tp)->syms->table[0];
                SYMBOL *candidate = FALSE;
                while (hr)
                {
                    SYMBOL *sym = (SYMBOL *)hr->p;
                    if (sym->isTemplate)
                        return FALSE;
                    hr = hr->next;
                }
                // no templates, we can try each function one at a time
                hr = basetype(exp->v.sp->tp)->syms->table[0];
                while (hr)
                {
                    SYMBOL *sym = (SYMBOL *)hr->p;
                    clearoutDeduction(P);
                    if (Deduce(P, sym, FALSE))
                        if (candidate)
                            return FALSE;
                        else
                            candidate = sym;
                    hr = hr->next;
                }
                if (candidate)
                    return Deduce(P, candidate, TRUE);
            }
        }       
    }
    return FALSE;
}
SYMBOL *TemplateDeduceArgsFromArgs(SYMBOL *sym, FUNCTIONCALL *args)
{
    TEMPLATEPARAM *params = sym->templateParams->next;
    HASHREC *templateArgs = basetype(sym->tp)->syms->table[0];
    INITLIST *symArgs = args->arguments;
    TEMPLATEPARAM *initial = args->templateParams;
    ClearArgValues(params);
    while (initial && params)
    {
        if (initial->type != params->type)
            return NULL;
        params->byClass.val = initial->byClass.dflt;
        params->initialized = TRUE;
        initial = initial->next;
        params = params->next;
    }
    params = sym->templateParams->bySpecialization.types;
    initial = args->templateParams;
    while (initial && params)
    {
        if (initial->type != params->type)
            return NULL;
        switch(initial->type)
        {
            case kw_typename:
                if (!comparetypes(initial->byClass.dflt, params->byClass.dflt, TRUE))
                    return FALSE;
                break;
            case kw_template:
                if (!exactMatchOnTemplateParams(initial->byTemplate.dflt, params->byTemplate.dflt->templateParams->next))
                    return FALSE;
                break;
            case kw_int:
                if (!comparetypes(initial->byNonType.tp, params->byNonType.tp, TRUE) && (!ispointer(params->byNonType.tp) || !isconstzero(initial->byNonType.tp, params->byNonType.dflt)))
                    return FALSE;
                break;
        }
    }
    while (templateArgs && symArgs)
    {
        TemplateDeduceFromArg(((SYMBOL *)(templateArgs->p))->tp, symArgs->tp, symArgs->exp);
        templateArgs = templateArgs->next;
        symArgs = symArgs->next;
    }
    if (ValidateArgsSpecified(sym->templateParams->next, sym, NULL))
    {
        return SynthesizeResult(sym);
    }
    return NULL;
}
static BOOL TemplateDeduceFromType(TYPE* P, TYPE *A)
{
    return Deduce(P, A, TRUE);    
}
SYMBOL *TemplateDeduceWithoutArgs(SYMBOL *sym)
{
    if (ValidateArgsSpecified(sym->templateParams->next, sym, NULL))
    {
        return SynthesizeResult(sym);
    }
    return NULL;
}
static BOOL TemplateDeduceFromConversionType(TYPE* orig, TYPE *tp)
{
    TYPE *P = orig, *A = tp;
    if (isref(P))
        P = basetype(P)->btp;
    if (!isref(A))
    {
        P = rewriteNonRef(P);
    }
    A = removeTLQuals(A);
    if (TemplateDeduceFromType(P, A))
        return TRUE;
    if (ispointer(P))
    {
        BOOL doit = FALSE;
        while (ispointer(P) && ispointer(A))
        {
            if (isconst(P) && !isconst(A) || isvolatile(P) && !isvolatile(A))
                return FALSE;
            P = basetype(P)->btp;
            A = basetype(A)->btp;
        }
        P = basetype(P);
        A = basetype(A);
        if (doit && TemplateDeduceFromType(P, A))
            return TRUE;
    }
    return FALSE;
}
SYMBOL *TemplateDeduceArgsFromType(SYMBOL *sym, TYPE *tp)
{
    ClearArgValues(sym->templateParams);
    if (sym->castoperator)
    {
        TemplateDeduceFromConversionType(basetype(sym->tp)->btp, basetype(tp)->btp);
        if (ValidateArgsSpecified(sym->templateParams->next, sym, NULL))
            return SynthesizeResult(sym);
    }
    else
    {
        HASHREC *templateArgs = basetype(tp)->syms->table[0];
        HASHREC *symArgs = basetype(sym->tp)->syms->table[0];
        while (templateArgs && symArgs)
        {
            TemplateDeduceFromType(((SYMBOL *)symArgs->p)->tp, ((SYMBOL *)templateArgs->p)->tp);
            templateArgs = templateArgs->next;
            symArgs = symArgs->next;
        }
        if (ValidateArgsSpecified(sym->templateParams->next, sym, NULL))
        {
            return SynthesizeResult(sym);
        }
    }
    return NULL;
}
int TemplatePartialDeduceFromType(TYPE *orig, TYPE *sym)
{
    TYPE *P = orig, *A=sym;
    int which = -1;
    if (isref(P))
        P= basetype(P)->btp;
    if (isref(A))
        A= basetype(A)->btp;
    if (isref(orig) && isref(sym))
    {
        BOOL p=FALSE, a = FALSE;
        if (isconst(P) && !isconst(A) || isvolatile(P) && !isvolatile(A))
            p = TRUE;
        if (isconst(A) && !isconst(P) || isvolatile(A) && !isvolatile(P))
            a = TRUE;
        if (a && !p)
            which = 1;
    }
    A = removeTLQuals(A);
    P = removeTLQuals(P);
    if (!Deduce(P, A, TRUE))
        return 0;
    return which;
}
int TemplatePartialDeduce(TYPE *origl, TYPE *origr, TYPE *syml, TYPE *symr)
{
    int n = TemplatePartialDeduceFromType(origl, symr);
    int m = TemplatePartialDeduceFromType(origr, syml);
    if (n && m)
    {
        if (basetype(origl)->type == bt_lref)
        {
            if (basetype(origr)->type != bt_lref)
                return -1;
        }
        else if (basetype(origr)->type == bt_lref)
        {
            return 1;
        }
        if (n > 0 && m <= 0)
            return -1;
        else if (m > 0 && n <= 0)
            return 1;
        return 0;
    }
    if (n)
        return 1;
    if (m)
        return -1;
    return 100;
}
int TemplatePartialDeduceArgsFromType(SYMBOL *syml, SYMBOL *symr, TYPE *tpl, TYPE *tpr, FUNCTIONCALL *fcall)
{
    int which = 0;
    ClearArgValues(syml->templateParams);
    ClearArgValues(symr->templateParams);
    if (isstructured(syml->tp))
    {
        which = TemplatePartialDeduce(syml->tp, symr->tp, tpl, tpr);
    }
    else if (syml->castoperator)
    {
        which = TemplatePartialDeduce(basetype(syml->tp)->btp, basetype(symr->tp)->btp, basetype(tpl)->btp, basetype(tpr)->btp);
    }
    else
    {
        int arr[200], n=0,i;
        HASHREC *tArgsl = basetype(tpl)->syms->table[0];
        HASHREC *sArgsl = basetype(syml->tp)->syms->table[0];
        HASHREC *tArgsr = basetype(tpr)->syms->table[0];
        HASHREC *sArgsr = basetype(symr->tp)->syms->table[0];
        BOOL usingargs = fcall && fcall->ascall;
        INITLIST *args = fcall->arguments;
        while (tArgsl && tArgsr && sArgsl && sArgsr && (!usingargs || args))
        {
            arr[n++] = TemplatePartialDeduce(((SYMBOL *)sArgsl->p)->tp, ((SYMBOL *)sArgsr->p)->tp,
                                  ((SYMBOL *)tArgsl->p)->tp, ((SYMBOL *)tArgsr->p)->tp);
            if (args)
                args = args->next;
            tArgsl = tArgsl->next;
            sArgsl = sArgsl->next;
            tArgsr = tArgsr->next;
            sArgsr = sArgsr->next;
        }
        for (i=0; i < n; i++)
            if (arr[i] == 100)
                return 0;
        for (i=0; i < n; i++)
            if (!which)
                which = arr[i];
            else if (which && which != arr[i])
                return 0;
    }
    return which;
}
void TemplatePartialOrdering(SYMBOL **table, int count, FUNCTIONCALL *funcparams, TYPE *atype)
{
    int i,j, n = 47, c = 0;
    for (i=0; i < count; i++)
        if (table[i])
            c++;
    if (c > 1)
    {
        LIST *types = NULL, *exprs = NULL, *classes = NULL;
        TYPE **typetab = Alloc(sizeof(TYPE *) * count);
        for (i=0; i < count; i++)
        {
            if (table[i] && table[i]->isTemplate)
            {
                SYMBOL *sym = table[i]->parentTemplate;
                TEMPLATEPARAM *params = sym->templateParams->next;
                LIST *typechk = types, *exprchk = exprs, *classchk = classes;
                while(params)
                {
                    switch(params->type)
                    {
                        case kw_typename:
                            if (typechk)
                            {
                                params->byClass.temp = (EXPRESSION *)typechk->data;
                                typechk = typechk->next;
                                
                            }
                            else
                            {
                                LIST *lst = Alloc(sizeof(LIST));
                                TYPE *tp = Alloc(sizeof(TYPE));
                                tp->type = bt_class;
                                tp->sp = params->sym;
                                params->byClass.temp = tp;
                                lst->data = tp;
                                lst->next = types;
                                types = lst;                        
                            }
                            break;
                        case kw_template:
                            params->byTemplate.temp = params->sym;
                            break;
                        case kw_int:
                            if (exprchk)
                            {
                                params->byNonType.temp = (EXPRESSION *)exprchk->data;
                                exprchk = exprchk->next;
                                
                            }
                            else
                            {
                                LIST *lst = Alloc(sizeof(LIST));
                                EXPRESSION *exp = intNode(en_c_i, 47);
                                params->byNonType.temp = exp;
                                lst->data = exp;
                                lst->next = exprs;
                                exprs = lst; 
                            }
                            break;
                    }
                    params = params->next;
                }
                if (isstructured(sym->tp))
                    typetab[i] = SynthesizeTemplate(sym->tp, TRUE);
                else
                    typetab[i] = SynthesizeType(sym->tp, TRUE);
            }
        }
        for (i=0; i < count-1; i++)
        {
            if (table[i])
            {
                for (j=i+1; table[i] && j < count; j++)
                {
                    if (table[j])
                    {
                        int which = TemplatePartialDeduceArgsFromType(table[i]->parentTemplate, table[j]->parentTemplate, typetab[i], typetab[j], funcparams);
                        if (which < 0)
                        {
                            table[j] = 0;
                        }
                        else if (which > 0)
                        {
                            table[i] = 0;
                        }
                    }
                }
            }
        }
    }
}
static BOOL TemplateInstantiationMatch(SYMBOL *orig, SYMBOL *sym)
{
    if (orig->parentTemplate == sym->parentTemplate)
    {
        TEMPLATEPARAM *porig = orig->templateParams;
        TEMPLATEPARAM *psym = sym->templateParams;
        while (porig && psym)
        {
            switch (porig->type)
            {
                case kw_typename:
                {
                    TYPE *torig =porig->byClass.val;
                    TYPE *tsym = psym->byClass.val;
                    if (!comparetypes(torig, tsym, TRUE))
                        return FALSE;
                    if (isref(torig))
                        torig = basetype(torig)->btp;
                    if (isref(porig))
                        porig = basetype(porig)->btp;
                    break;
                }
                case kw_template:
                    if (porig->byTemplate.val != psym->byTemplate.val)
                        return FALSE;
                    break;
                case kw_int:
#ifndef PARSER_ONLY
                    if (!equalnode(porig->byNonType.val, psym->byNonType.val))
                        return FALSE;
#endif
                    break;
            }
            porig = porig->next;
            psym = psym->next;
        }
        return TRUE;
    }
    return FALSE;
}
SYMBOL *TemplateClassInstantiate(SYMBOL *sym, TEMPLATEPARAM *args)
{
    TEMPLATEPARAM *dflts = sym->parentTemplate->templateParams->next;
    TEMPLATEPARAM *orig = sym->templateParams->bySpecialization.types;
    LIST *instants = sym->parentTemplate->instantiations;
    TEMPLATEPARAM *params = NULL, **last = &params, *parse;
    LEXEME *lex;
    SYMBOL *cls  = clonesym(sym);
    while (orig)
    {
        if (args)
        {
            orig->byClass.val = args->byClass.dflt;
            orig->byNonType.tp = args->byNonType.tp;
            args = args->next;
        }
        else
        {
            orig->byClass.val = dflts->byClass.dflt;
            orig->byNonType.tp = dflts->byNonType.tp;
        }
        orig = orig->next;
        dflts = dflts->next;
    }
    while (instants)
    {
        if (TemplateInstantiationMatch(instants->data, sym))
            return (SYMBOL *)instants->data;
        instants = instants->next;
    }
    parse = sym->templateParams;
    while (parse)
    {
        *last = Alloc(sizeof(TEMPLATEPARAM));
        **last = *parse;
        last = &(*last)->next;
        parse = parse->next;
    }
    cls->templateParams = params;
    instants = Alloc(sizeof(LIST));
    instants->data = cls;
    instants->next = sym->parentTemplate->instantiations;
    sym->parentTemplate->instantiations = instants;
    sym->instantiated = TRUE;
    SetLinkerNames(cls, lk_cdecl);
    lex = sym->deferredCompile;
    if (lex)
    {
        instantiatingTemplate++;
        lex = SetAlternateLex(sym->deferredCompile);
        SetAlternateLex(NULL);
        instantiatingTemplate --;
        sym->genreffed = TRUE;
        sym->storage_class = sc_global;
    }
    else
    {
        errorsym(ERR_TEMPLATE_CANT_INSTANTIATE_NOT_DEFINED, sym->parentTemplate);
    }
    return cls;
}
SYMBOL *TemplateFunctionInstantiate(SYMBOL *sym)
{
    LIST *instants = sym->parentTemplate->instantiations;
    TEMPLATEPARAM *params = NULL, **last = &params, *parse;
    LEXEME *lex;
    while (instants)
    {
        if (TemplateInstantiationMatch(instants->data, sym))
            return (SYMBOL *)instants->data;
        instants = instants->next;
    }
    parse = sym->templateParams;
    while (parse)
    {
        *last = Alloc(sizeof(TEMPLATEPARAM));
        **last = *parse;
        last = &(*last)->next;
        parse = parse->next;
    }
    sym->templateParams = params;
    instants = Alloc(sizeof(LIST));
    instants->data = sym;
    instants->next = sym->parentTemplate->instantiations;
    sym->parentTemplate->instantiations = instants;
    sym->instantiated = TRUE;
    SetLinkerNames(sym, lk_cdecl);
    lex = sym->deferredCompile;
    if (lex)
    {
        STRUCTSYM s;
        s.tmpl = sym->templateParams;
        addTemplateDeclaration(&s);

        sym->storage_class = sc_global;
        sym->linkage = lk_inline;
        instantiatingTemplate++;
        backFillDeferredInitializersForFunction(sym, sym);
        lex = SetAlternateLex(sym->deferredCompile);
        if (MATCHKW(lex, kw_try) || MATCHKW(lex, colon))
        {
            BOOL viaTry = MATCHKW(lex, kw_try);
            int old = GetGlobalFlag();
            if (viaTry)
            {
                sym->hasTry = TRUE;
                lex = getsym();                                
            }
            if (MATCHKW(lex, colon))
            {
                lex = getsym();                                
                sym->memberInitializers = GetMemberInitializers(&lex, sym);
            }
        }
        lex = body(lex, sym);
        SetAlternateLex(NULL);
        dropStructureDeclaration();
        instantiatingTemplate --;
        sym->genreffed = TRUE;
    }
    else
    {
        sym->storage_class = sc_external;
        InsertExtern(sym);
    }
    return sym;
}
static BOOL ValidateClassTemplate(SYMBOL *sp, TEMPLATEPARAM *unspecialized, TEMPLATEPARAM *args)
{
    TEMPLATEPARAM *spsyms = sp->templateParams->bySpecialization.types;
    while (spsyms && (args || unspecialized))
    {
        if (args)
        {
            if (spsyms->type != args->type)
                return FALSE;
        }
        else 
        {   
            if (spsyms->type != unspecialized->type)
                return FALSE;
        }
        spsyms = spsyms->next;
        if (args)
            args = args->next;
        if (unspecialized)
            unspecialized = unspecialized->next;
    }
    return !spsyms && !args && !unspecialized;
}
static SYMBOL *GetClassTemplate(SYMBOL *sp, TEMPLATEPARAM *args)
{
    int n = 1, i=0;
    TEMPLATEPARAM *unspecialized = sp->templateParams->next;
    SYMBOL *found1 = NULL, *found2 = NULL;
    SYMBOL **spList;
    LIST *l = sp->specializations;
    while (l)
    {
        n++;
        l = l->next;
    }
    if (n == 1)
        return sp;
    spList = Alloc(sizeof(SYMBOL *) * n);
    spList[i++]  = ValidateClassTemplate(sp, unspecialized, args);
    l = sp->specializations;
    while (i < n)
        spList[i++] = ValidateClassTemplate((SYMBOL *)l->data, unspecialized, args);
    TemplatePartialOrdering(spList, n, NULL, NULL);
    for (i=0; i < n && !found1; i++)
    {
        int j;
        found1 = spList[i];
        for (j=i+1; j < n && found1 && !found2; j++)
        {
            if (spList[j])
            {
                found2 = spList[j];
            }
        }
    }
    if (!found1 || found2)
    {
        errorsym(ERR_NO_TEMPLATE_MATCHES, sp);
        return NULL;
    }
    return found1;
}
LEXEME *TemplateDeclaration(LEXEME *lex, SYMBOL *funcsp, enum e_ac access, BOOL isExtern)
{
    lex = getsym();
    if (MATCHKW(lex, lt))
    {
        int max = 0;
        TEMPLATEPARAM *ta = Alloc(sizeof(TEMPLATEPARAM)); // for the specialization list
        STRUCTSYM s;
        TYPE *tp = NULL;
        struct listData l;
        lex = backupsym(1);
        if (isExtern)
            error(ERR_DECLARE_SYNTAX);
        ta->type = kw_new; // by specialization
        
        s.tmpl = ta;
        addTemplateDeclaration(&s);
        if (templateNestingCount == 0)
        {
            l.args = ta;
            l.next = currents;
            l.sym = NULL;
            l.head = l.tail = NULL;
            l.bodyHead = l.bodyTail = NULL;
            currents = &l;
        }
        while (MATCHKW(lex, kw_template)) 
        {
            max++;
            lex = getsym();
            ta->next = NULL;
            lex = TemplateHeader(lex, funcsp, &ta->next); 
        }
        if (lex)
        {
            templateNestingCount++;
            lex = declare(lex, funcsp, &tp, ta, sc_template, lk_none, NULL, TRUE, FALSE, FALSE, access);
            templateNestingCount--;
            if (!templateNestingCount)
            {
                if (!tp)
                {
                    error(ERR_TEMPLATES_MUST_BE_CLASSES_OR_FUNCTIONS);
                }
                else if (!isfunction(tp) && !isstructured(tp) )
                {
                    if (!l.sym || !l.sym->parentClass || l.sym->storage_class == sc_member || l.sym->storage_class == sc_virtual)
                    {
                        error(ERR_TEMPLATES_MUST_BE_CLASSES_OR_FUNCTIONS);
                    }
                }
                FlushLineData("", INT_MAX);
            }
        }
        dropStructureDeclaration();
        currents = currents->next;
        if (max > 1)
            error(ERR_TOO_MANY_TEMPLATE_PARAMETER_SETS_SPECIFIED);
    }
    else if (ISID(lex) || MATCHKW(lex, kw_operator))
    {
        SYMBOL *sym = NULL;
        lex = id_expression(lex, funcsp, &sym, NULL, NULL, FALSE,FALSE, lex->value.s.a);
        if (!sym)
        {
            error(ERR_IDENTIFIER_EXPECTED);
        }
        else if (!MATCHKW(lex, lt))
        {
            errorsym(ERR_TOO_FEW_ARGS_PASSED_TO_TEMPLATE, sym);
        }
        else
        {
            TEMPLATEPARAM *lst = NULL;
            INITLIST *args;
            lex = GetTemplateArguments(lex, funcsp, &lst); 
            if (sym->storage_class == sc_overloads)
            {
                if (MATCHKW(lex, openpa))
                {
                    FUNCTIONCALL *funcparams = Alloc(sizeof(FUNCTIONCALL));
                    SYMBOL *instance;
                    lex = getArgs(lex, funcsp, funcparams, closepa);
                    funcparams->templateParams = lst;
                    instance = GetOverloadedTemplate(sym, funcparams);
                    if (instance)
                    {
                        if (isExtern)
                        {
                            instance->storage_class = sc_external;
                            InsertExtern(instance);
                        }
                        else
                        {
                            TemplateFunctionInstantiate(instance);
                        }
                    }
                    else
                    {
                        errorsym(ERR_NOT_A_TEMPLATE, instance);
                    }
                }
                else
                {
                    needkw(&lex, openpa);
                }
            }
            else if (isstructured(sym->tp) && sym->isTemplate)
            {
                SYMBOL *instance = GetClassTemplate(sym, lst);
                if (instance)
                {
                    if (!isExtern)
                    {
                        TemplateClassInstantiate(instance, lst);
                    }
                }    
                else
                {
                     errorsym(ERR_NOT_A_TEMPLATE, instance);
                }                
            }
            else
            {
                errorsym(ERR_NOT_A_TEMPLATE, sym);
            }
        }
    }
    else
    {
        error(ERR_DECLARE_SYNTAX);
    }
    return lex;
}
