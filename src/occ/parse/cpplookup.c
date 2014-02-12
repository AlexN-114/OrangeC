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

extern COMPILER_PARAMS cparams;
extern int nextLabel;
extern NAMESPACEVALUES *globalNameSpace, *localNameSpace;
extern HASHTABLE *labelSyms;
extern TYPE stdint;
extern SYMBOL *enumSyms;
extern char *overloadNameTab[];
extern LAMBDA *lambdas;
extern STRUCTSYM *structSyms;
extern int currentErrorLine;
extern int templateNestingCount;
extern int total_errors;
#ifndef CPREPROCESSOR
extern ARCH_DEBUG *chosenDebugger;
extern FILE *listFile;
#endif
extern int noSpecializationError;
extern int instantiatingTemplate;
extern INCLUDES *includes;

HASHTABLE *CreateHashTable(int size);
#define F_WITHCONS 1
#define F_INTEGER 2
#define F_ARITHMETIC 4
#define F_STRUCTURE 8
#define F_POINTER 16

static SYMBOL *getUserConversion(int flags,
                              TYPE *tpp, TYPE *tpa, EXPRESSION *expa,
                              int *n, enum e_cvsrn *seq, SYMBOL **userFunc);
static BOOL getFuncConversions(SYMBOL *sp, FUNCTIONCALL *f, TYPE *atp, SYMBOL *parent, enum e_cvsrn arr[], int *sizes, int count, SYMBOL **userFunc);

LIST *tablesearchone(char *name, NAMESPACEVALUES *ns, BOOL tagsOnly)
{
    SYMBOL *rv = NULL;
    if (!tagsOnly)
        rv = search(name, ns->syms);
    if (!rv)
        rv = search(name, ns->tags);
    if (rv)
    {
        LIST *l = Alloc(sizeof(LIST));
        l->data = rv;
        return l;
    }
    return NULL;
}
static LIST *tablesearchinline(char *name, NAMESPACEVALUES *ns, BOOL tagsOnly)
{
    // main namespace
    LIST *rv = tablesearchone(name, ns, tagsOnly);
    LIST *lst = ns->inlineDirectives;
    // included inlines
    while (lst)
    {
        SYMBOL *x = lst->data;
        if (!x->visited)
        {
            LIST *rv1;
            x->visited = TRUE;
            rv1 = tablesearchinline(name, x->nameSpaceValues, tagsOnly);
            if (rv1)
            {
                while (rv1->next)
                    rv1 = rv1->next;
                rv1->next = rv;
                rv = rv1;
            }
        }
        lst = lst->next;
    }
    // enclosing ns if this one is inline
    if (ns->name && !ns->name->visited && ns->name->linkage == lk_inline)
    {
        LIST *rv1;
        rv1 = tablesearchinline(name, ns->name->nameSpaceValues, tagsOnly);
        if (rv1)
        {
            while (rv1->next)
                rv1 = rv1->next;
            rv1->next = rv;
            rv = rv1;
        }
        
    }
    return rv;
}
static LIST *namespacesearchone(char *name, NAMESPACEVALUES *ns, LIST *gather, BOOL tagsOnly, BOOL allowUsing)
{
    LIST *rv = tablesearchinline(name, ns, tagsOnly);
    if (rv)
    {
        LIST *rv1 = rv;
        while (rv->next)
            rv = rv->next;
        rv->next = gather;
        rv = rv1;
    }
    else if (allowUsing)
    {
        LIST *lst = ns->usingDirectives;
        rv = gather;
        while (lst)
        {
            SYMBOL *x = lst->data;
            if (!x->visited)
            {
                x->visited = TRUE;
                rv = namespacesearchone(name, x->nameSpaceValues, rv, tagsOnly, allowUsing);
            }
            lst = lst->next;
        }
    }
    return rv;
}
static LIST *namespacesearchInternal(char *name, NAMESPACEVALUES *ns, BOOL qualified, BOOL tagsOnly,
                                     BOOL allowUsing)
{
    LIST *lst;
    do
    {
        unvisitUsingDirectives(ns);
        lst = namespacesearchone(name, ns, NULL, tagsOnly, allowUsing);
        ns = ns->next;
    } while (!qualified && !lst && ns);
    return lst;
}
SYMBOL *namespacesearch(char *name, NAMESPACEVALUES *ns, BOOL qualified, BOOL tagsOnly)
{
    LIST *lst = namespacesearchInternal(name, ns, qualified, tagsOnly, TRUE);
    
    if (lst)
    {
        while (lst->next)
        {
            // collision
            SYMBOL *test = lst->data;
            LIST *lst1 = lst->next;
            while (lst1)
            {
                if (test != lst1->data)
                {
                    errorsym2(ERR_AMBIGUITY_BETWEEN, test, (SYMBOL *)lst1->data);
                }
                lst1 = lst1->next;
            }
            lst = lst->next;
        }
        return (SYMBOL *)lst->data;
    }
    return NULL;
}
LEXEME *nestedPath(LEXEME *lex, SYMBOL **sym, NAMESPACEVALUES **ns, 
                   BOOL *throughClass, BOOL tagsOnly)
{
    NAMESPACEVALUES *nssym = globalNameSpace;
    SYMBOL *strSym = NULL;
    BOOL qualified = FALSE;
    TEMPLATESELECTOR *templateSelector = NULL, **last = &templateSelector;
    LEXEME *placeholder = lex, *finalPos;
    BOOL hasTemplate = FALSE;
    TYPE *dependentType = NULL;
    if (sym)
        *sym = NULL;
    if (ns) *ns = NULL;
    if (MATCHKW(lex, classsel))
    {
        while (nssym->next)
            nssym = nssym->next;
        lex = getsym();
        qualified = TRUE;
    }
    finalPos = lex;
    while (ISID(lex))
    {
        char buf[512];
        SYMBOL *sp = NULL;
        strcpy(buf, lex->value.s.a);
        lex = getsym();
        if (templateSelector)
        {
            *last = Alloc(sizeof(TEMPLATESELECTOR));
            (*last)->name = litlate(buf);
            if (hasTemplate)
            {
                (*last)->isTemplate = TRUE;
                if (MATCHKW(lex, lt))
                {
                    lex = GetTemplateArguments(lex, NULL, &(*last)->templateParams);
                }
                else if (MATCHKW(lex,classsel))
                {
                    errorstr(ERR_NEED_SPECIALIZATION_PARAMETERS, buf);                    
                }
            }
            last = &(*last)->next;
            if (!MATCHKW(lex, classsel))
                break;
            lex = getsym();
            finalPos = lex;
        }
        else
        {
            BOOL hasTemplateArgs = FALSE;
            TEMPLATEPARAM *current = NULL;
            SYMBOL *currentsp = NULL;
            LEXEME *cur = lex;
            int level = -1;
            if (MATCHKW(lex, lt))
            {
                while (lex && (!MATCHKW(lex, gt) || level) && !MATCHKW(lex, semicolon))
                {
                    if (MATCHKW(lex, lt))
                    {
                        lex = getsym();
                        level ++;
                    }
                    else if (MATCHKW(lex, gt))
                    {
                        lex = getsym();
                        level --;
                    }
                    else if (MATCHKW(lex, rightshift))
                    {
                        level -- ;
                        lex = getGTSym(lex);
                    }
                    else
                    {
                        lex = getsym();
                    }
                }
                if (lex && MATCHKW(lex, gt))
                    lex = getsym();
            }
            if (!lex || !MATCHKW(lex, classsel))
                break;
            if (cur != lex)
                lex = prevsym(cur);
                            
            if (!strSym)
            {
                if (!qualified)
                {
                    sp = classsearch(buf, FALSE);
                    if (sp && sp->tp->type == bt_templateparam)
                    {
                        if (sp->tp->templateParam->packed)
                        {
                            break;
                        }
                        else
                        {
                            TEMPLATEPARAM *params = sp->tp->templateParam;
                            if (params->type == kw_typename)
                            {
                                if (params->byClass.val)
                                {
                                    sp = params->byClass.val->sp;
                                    dependentType = params->byClass.val;
                                }
                            }
                            else if (params->type == kw_template)
                            {
                                sp = params->byTemplate.val;
                            }
                            else
                                break;
                        }
                    }
                    if (sp)
                        *throughClass = TRUE;
                }
                else
                {
                    sp = NULL;
                }
                if (!sp)
                {
                    if (!qualified)
                        sp = namespacesearch(buf, localNameSpace, qualified, FALSE);
                    if (!sp)
                    {
                        sp = namespacesearch(buf, nssym, qualified, FALSE);
                    }
                }
                if (sp && sp->tp->type == bt_typedef)
                {
                    if (isstructured(sp->tp))
                    {
                        sp = basetype(sp->tp)->sp;
                        *throughClass = TRUE;
                    }
                }
            }
            else
            {
                sp = search(buf, strSym->tp->tags);
            }
            if (sp)
            {
                if (sp->templateLevel && (!sp->instantiated || MATCHKW(lex, lt)))
                {
                    hasTemplateArgs = TRUE;
                    if (MATCHKW(lex, lt))
                    {
                        lex = GetTemplateArguments(lex, NULL, &current);
                    }
                    else if (MATCHKW(lex, classsel))
                    {
                        currentsp = sp;
                        if (!noSpecializationError && !instantiatingTemplate)
                            errorsym(ERR_NEED_SPECIALIZATION_PARAMETERS, sp);
                    }
                }
                else if (hasTemplate && (basetype(sp->tp)->type != bt_templateparam || basetype(sp->tp)->templateParam->type != kw_template))
                {
                    errorsym(ERR_NOT_A_TEMPLATE, sp);
                }
            }
            if (hasTemplateArgs)
            {
                if (currentsp)
                {
                    sp = currentsp;
                }
                else
                {
                    sp = GetClassTemplate(sp, current);
                    if (sp)
                        sp = TemplateClassInstantiate(sp, current, FALSE);
                }
            }
            if (sp)
                PerformDeferredInitialization (sp->tp, NULL);
            lex = getsym();
            finalPos = lex;
            if (sp && isstructured(sp->tp))
            {
                strSym = sp;
                if (!qualified)
                    nssym = NULL;
            }
            else if (sp && (sp->storage_class == sc_namespace || sp->storage_class == sc_namespacealias))
            {
                nssym = sp->nameSpaceValues;                    
            }
            else if (sp && sp->tp->type == bt_templateparam)
            {
                *last = Alloc(sizeof(TEMPLATESELECTOR));
                (*last)->sym = strSym;
                last = &(*last)->next;
                *last = Alloc(sizeof(TEMPLATESELECTOR));
                (*last)->sym = sp;
                last = &(*last)->next;
            }
            else
            {
                if (dependentType)
                    errortype(ERR_DEPENDENT_TYPE_NOT_A_CLASS_OR_STRUCT, dependentType, NULL);
                else
                    errorstr(ERR_QUALIFIER_NOT_A_CLASS_OR_NAMESPACE , buf);
                lex = prevsym(placeholder);
                break;
            }
        }
        hasTemplate = FALSE;
        if (MATCHKW(lex, kw_template))
        {
            hasTemplate = TRUE;
            lex = getsym();
        }
        qualified = TRUE;
    }
    lex = prevsym(finalPos);
    if (templateSelector)
    {
        TYPE *tp = Alloc(sizeof(TYPE));
        tp->type = bt_templateselector;
        *sym = makeID(sc_global, tp, NULL, AnonymousName());
        (*sym)->templateSelector = templateSelector;
        tp->sp = *sym;
    }
    else if (qualified)
    {
        if (strSym && sym)
            *sym = strSym;
            
        if (ns)
            if (nssym && nssym->name)
                *ns = nssym;
            else
                *ns = NULL;
        else
            error(ERR_QUALIFIED_NAME_NOT_ALLOWED_HERE);
    }
    return lex;
}
static SYMBOL *classdata(char *name, SYMBOL *cls, SYMBOL *last, BOOL isvirtual, BOOL tagsOnly)
{
    SYMBOL *rv = NULL;
    if (!tagsOnly)
        rv = search(name, cls->tp->syms);
    if (!rv)
        rv = search(name, cls->tp->tags);
    if (rv)
    {
        rv->temp = isvirtual;
        if (!last || (last == rv || rv->mainsym && rv->mainsym == last->mainsym) && (isvirtual && isvirtual == last->temp || 
                                                                                     ismember(rv)))
        {
        }
        else
        {
            rv = (SYMBOL *)-1;
        }
    }
    else
    {
        BASECLASS *lst = cls->baseClasses;
        rv = last;
        while (lst)
        {
            rv = classdata(name, lst->cls, rv, isvirtual | lst->isvirtual, tagsOnly);
            if (rv == (SYMBOL *)-1)
                break;
            lst = lst->next;
        }
    }
    return rv;
}
SYMBOL *templatesearch(char *name, TEMPLATEPARAM *arg)
{
    while (arg)
    {
        if (arg->sym && !strcmp(arg->sym->name, name))
            return arg->sym;
        arg = arg->next;
    }
    return NULL;
}
TEMPLATEPARAM *getTemplateStruct(char *name)
{
    SYMBOL *cls = getStructureDeclaration(), *base = cls;
    while (cls)
    {
        TEMPLATEPARAM *arg = cls->templateParams;
        if (arg)
        {
            while (arg)
            {
                if (!strcmp(arg->sym->name, name))
                    return arg;
                arg = arg->next;
            }
        }
        cls = cls->parentClass;
    }
    return NULL;
}
SYMBOL *classsearch(char *name, BOOL tagsOnly)
{
    SYMBOL *rv = NULL;
    SYMBOL *cls = getStructureDeclaration();
    STRUCTSYM *s = structSyms;
    while (s && s->tmpl && !rv)
    {
        rv = templatesearch(name, s->tmpl);
        s = s->next;
    }
    if (cls && !rv)
    {
        /* optimize for the case where the final class has what we need */
        while (cls && !rv)
        {
            if (!tagsOnly)
                rv = search(name, cls->tp->syms);
            if (!rv)
                rv = search(name, cls->tp->tags);
            if (!rv && cls->baseClasses)
            {
                rv = classdata(name, cls, NULL, FALSE, tagsOnly);
                if (rv == (SYMBOL *)-1)
                {
                    rv = NULL;
                    errorstr(ERR_AMBIGUOUS_MEMBER_DEFINITION, name);
                    break;
                }
            }
            if (!rv && cls->templateParams)
                rv = templatesearch(name, cls->templateParams);
            cls = cls->parentClass;
        }
    }
    while (s && !rv)
    {
        if (s->tmpl)
            rv = templatesearch(name, s->tmpl);
        s = s->next;
    }
    return rv;
}
SYMBOL *finishSearch(char *name, SYMBOL *encloser, NAMESPACEVALUES *ns, BOOL tagsOnly, BOOL throughClass)
{
    SYMBOL *rv = NULL;
    if (!encloser && !ns)
    {
        SYMBOL *ssp;
        rv = search(name, localNameSpace->syms);
        if (!rv)
            rv = search(name, localNameSpace->tags);
        if (lambdas)
        {
            LAMBDA *srch = lambdas;
            while (srch && !rv)
            {
                rv = search(name, srch->oldSyms);
                if (!rv)
                    rv = search(name, srch->oldTags);
                srch = srch->next;
            }
        }
        if (!rv)
            rv = namespacesearch(name, localNameSpace, FALSE, tagsOnly);
        if (!rv && enumSyms)
            rv = search(name, enumSyms->tp->syms);
        if (!rv)
        {
            if (lambdas)
            {
                if (lambdas->lthis)
                {
                    rv = search(name, basetype(lambdas->lthis->tp)->btp->syms);
                    if (rv)
                        rv->throughClass = TRUE;
                }
            }
            if (!rv)
            {
                rv = classsearch(name, tagsOnly);
                if (rv)
                    rv->throughClass = TRUE;
            }
        }
        else
        {
            rv->throughClass = FALSE;
        }
        ssp = getStructureDeclaration();
        if (!rv && (!ssp || ssp->nameSpaceValues != globalNameSpace))
        {
            rv = namespacesearch(name, globalNameSpace, FALSE, tagsOnly);
            if (rv)
                rv->throughClass = FALSE;
        }
    }
    else
    {
        if (encloser)
        {
            STRUCTSYM l;
            l.str = (void *)encloser;
            addStructureDeclaration(&l);
            rv = classsearch(name, tagsOnly);
            dropStructureDeclaration();
            if (rv)
                rv->throughClass = throughClass;
        }
        else
        {
            LIST *rvl = tablesearchinline(name, ns, tagsOnly);
            if (rvl)
            {
                rv = rvl->data;
                rv->throughClass = FALSE;
            }
        }
    }
    return rv;
}
LEXEME *nestedSearch(LEXEME *lex, SYMBOL **sym, SYMBOL **strSym, NAMESPACEVALUES **nsv, 
                     BOOL *destructor, BOOL *isTemplate, BOOL tagsOnly)
{
    SYMBOL *encloser = NULL;
    NAMESPACEVALUES *ns = NULL;
    BOOL throughClass = FALSE;
    LEXEME *placeholder = lex;
    BOOL hasTemplate = FALSE;
    *sym = NULL;
    
    if (!cparams.prm_cplusplus)
    {
        if (ISID(lex))
        {
            if (tagsOnly)
                *sym = tsearch(lex->value.s.a);
            else
                *sym = gsearch(lex->value.s.a);
        }
        return lex;
    }
    lex = nestedPath(lex, &encloser, &ns, &throughClass, tagsOnly);
    if (cparams.prm_cplusplus)
    {
            
        if (MATCHKW(lex, compl))
        {
            if (destructor)
            {
                *destructor = TRUE;
            }
            else
            {
                error(ERR_CANNOT_USE_DESTRUCTOR_HERE);
            }
            lex = getsym();
        }
        else if (MATCHKW(lex, kw_template))
        {
            lex = getsym();
            if (isTemplate)
                *isTemplate = TRUE;
            hasTemplate = TRUE;
        }
    }
    if (ISID(lex))
    {
        if (encloser && encloser->tp->type == bt_templateselector)
        {
            LIST *l = encloser->templateSelector;
            while (l->next)
                l = l->next;
            *sym = makeID(sc_type, encloser->tp, NULL, (char *)l->data);
        }
        else
        {
            *sym = finishSearch(lex->value.s.a, encloser, ns, tagsOnly, throughClass);
        }
    }
    else if (destructor && *destructor)
    {
        *destructor = FALSE;
        error(ERR_CANNOT_USE_DESTRUCTOR_HERE);
    }
    if (*sym && hasTemplate)
    {
        if (!(*sym)->templateLevel && ((*sym)->tp->type != bt_templateparam || (*sym)->tp->templateParam->type != kw_template) && (*sym)->tp->type != bt_templateselector)
        {
            if ((*sym)->storage_class == sc_overloads)
            {
                HASHREC *hr = basetype((*sym)->tp)->syms->table[0];
                while (hr)
                {
                    SYMBOL *sp = (SYMBOL *)hr->p;
                    if (sp->templateLevel)
                        break;
                    hr = hr->next;
                }
                if (!hr)
                    errorsym(ERR_NOT_A_TEMPLATE, *sym);
            }
            else
            {
                errorsym(ERR_NOT_A_TEMPLATE, *sym);
            }
        }
    }
    if (encloser&& strSym)
        *strSym = encloser;
    if (nsv)
        if (ns && ns->name)
            *nsv = ns;
        else
            *nsv = NULL;
    else if (!*sym)
        lex = prevsym(placeholder);
    return lex;
}
LEXEME *getIdName(LEXEME *lex, SYMBOL *funcsp, char *buf, int *ov, TYPE **castType)
{
    buf[0] = 0;
    if (ISID(lex))
    {
        strcpy(buf, lex->value.s.a);
    }
    else if (MATCHKW(lex, kw_operator))
    {
        lex = getsym();
        if (ISKW(lex) && lex->kw->key >= kw_new && lex->kw->key <= compl)
        {
            enum e_kw kw = lex->kw->key;
            switch(kw)
            {
                case openpa:
                    lex = getsym();
                    if (!MATCHKW(lex, closepa))
                    {
                        needkw(&lex, closepa);
                        lex = backupsym();
                    }
                    break ;
                case openbr:
                    lex = getsym();
                    if (!MATCHKW(lex, closebr))
                    {
                        needkw(&lex, closebr);
                        lex = backupsym();
                    }
                    break ;
                case kw_new:
                case kw_delete:
                    lex = getsym();
                    if (!MATCHKW(lex, openbr))
                    {
                        lex = backupsym();
                    }
                    else
                    {
                        kw = kw - kw_new + compl + 1;
                        lex = getsym();
                        if (!MATCHKW(lex,closebr))
                        {
                            needkw(&lex, closebr);
                            lex = backupsym();
                        }
                    }
                    break;
            }
            strcpy(buf, overloadNameTab[*ov = kw - kw_new + CI_NEW]);
        }
        else if (startOfType(lex, FALSE)) // potential cast operator
        {
            TYPE *tp = NULL;
            lex = get_type_id(lex, &tp, funcsp, TRUE);
            if (castType)
                *castType = tp;
            strcpy(buf, overloadNameTab[*ov = CI_CAST]);
        }
        else if (lex->type == l_astr)
        {
            LEXEME *placeholder = lex;
            SLCHAR *xx = (SLCHAR *)lex->value.s.w;
            if (xx->count)
                error(ERR_OPERATOR_LITERAL_EMPTY_STRING);
            lex = getsym();
            
            if (ISID(lex))
            {
                sprintf(buf, "%s@%s", overloadNameTab[CI_LIT], lex->value.s.a);
                *ov = CI_LIT;
            }
            else
            {
                error(ERR_OPERATOR_LITERAL_NEEDS_ID); 
                prevsym(placeholder);
            }
        }
        else
        {
            if (ISKW(lex))
                errorstr(ERR_INVALID_AS_OPERATOR, lex->kw->name);
            else
                errorstr(ERR_INVALID_AS_OPERATOR,"");
            lex = backupsym();
        }        
    }
    return lex;
}
LEXEME *id_expression(LEXEME *lex, SYMBOL *funcsp, SYMBOL **sym, SYMBOL **strSym, 
                      NAMESPACEVALUES **nsv, BOOL *isTemplate, BOOL tagsOnly, BOOL membersOnly, char *idname)
{
    SYMBOL *encloser = NULL;
    NAMESPACEVALUES *ns = NULL;
    BOOL throughClass = FALSE;
    TYPE *castType = NULL;
    LEXEME *placeholder = lex;
    char buf[512];
    int ov = 0;
    BOOL hasTemplate = FALSE;
    *sym = NULL;
    
    if (!cparams.prm_cplusplus)
    {
        if (ISID(lex))
        {
            if (idname)
                strcpy(idname, lex->value.s.a);
            if (tagsOnly)
                *sym = tsearch(lex->value.s.a);
            else
            {
                SYMBOL *ssp = getStructureDeclaration();
                if (ssp)
                {
                    *sym = search(lex->value.s.a, ssp->tp->syms);
                }
                if (*sym == NULL)
                    *sym = gsearch(lex->value.s.a);
            }
        }
        return lex;
    }
    lex = nestedPath(lex, &encloser, &ns, &throughClass, tagsOnly);
    if (MATCHKW(lex, compl))
    {
        lex = getsym();
        if (ISID(lex))
        {
            if (encloser)
            {
                if (strcmp(encloser->name, lex->value.s.a))
                {
                    error(ERR_DESTRUCTOR_MUST_MATCH_CLASS);
                }
                *sym = finishSearch(overloadNameTab[CI_DESTRUCTOR], 
                                    encloser, ns, tagsOnly, throughClass);
            }
        }
        else
        {
            error(ERR_CANNOT_USE_DESTRUCTOR_HERE);
        }
    }
    else
    {
        if (MATCHKW(lex, kw_template))
        {
            if (isTemplate)
                *isTemplate = TRUE;
            hasTemplate = TRUE;
            lex = getsym();
        }
        lex = getIdName(lex, funcsp, buf, &ov, &castType);
        if (buf[0])
        {
            if (!encloser && membersOnly)
                encloser = getStructureDeclaration();
            *sym = finishSearch(ov == CI_CAST ? overloadNameTab[CI_CAST] : buf, 
                                encloser, ns, tagsOnly, throughClass);
            if (*sym && hasTemplate)
                if ((*sym)->storage_class == sc_overloads)
                {
                    HASHREC *hr = basetype((*sym)->tp)->syms->table[0];
                    while (hr)
                    {
                        SYMBOL *sp = (SYMBOL *)hr->p;
                        if (sp->templateLevel)
                            break;
                        hr = hr->next;
                    }
                    if (!hr)
                        errorsym(ERR_NOT_A_TEMPLATE, *sym);
                }
                else
                {
                    errorsym(ERR_NOT_A_TEMPLATE, *sym);
                }
        }
    }
    if (encloser&& strSym)
        *strSym = encloser;
    if (nsv)
        if (ns && ns->name)
            *nsv = ns;
        else
            *nsv = NULL;
    else if (!*sym)
        prevsym(placeholder);
    if (!*sym && idname)
    {
        strcpy(idname, buf);
    }
    return lex;
}
SYMBOL *LookupSym(char *name)
{
    SYMBOL *rv = NULL;
    if (!cparams.prm_cplusplus)
    {
        return gsearch(name);
    }
    rv = search(name, localNameSpace->syms);
    if (!rv)
        rv = search(name, localNameSpace->tags);
    if (!rv)
        rv = namespacesearch(name, localNameSpace, FALSE, FALSE);
    if (!rv)
        rv = namespacesearch(name, globalNameSpace, FALSE, FALSE);
    return rv;
}
static BOOL isFriend(SYMBOL *cls, SYMBOL *frnd)
{
    if (frnd)
    {
        LIST *l = cls->friends;
        while (l)
        {
            SYMBOL *sym = (SYMBOL *)l->data;
            if (sym == frnd)
                return TRUE;
            if (sym->templateLevel)
            {
                LIST *instants = sym->instantiations;
                while (instants)
                {
                    if ((SYMBOL *)instants->data == frnd)
                        return TRUE;
                    instants = instants->next;
                }
            }
            l = l->next;
        }
    }
    return FALSE;
}
// works by searching the tree for the base or member symbol, and stopping any
// time the access wouldn't work.  If the symbol is found it is accessible.
BOOL isAccessible(SYMBOL *derived, SYMBOL *currentBase, 
                                 SYMBOL *member, SYMBOL *funcsp, 
                                 enum e_ac minAccess, BOOL asAddress)
{
    BASECLASS *lst;
    HASHREC *hr;
    enum e_ac consideredAccess = minAccess;
    SYMBOL *ssp;
    BOOL matched;
    TYPE *tp;
    if (!cparams.prm_cplusplus)
        return TRUE;
    ssp = getStructureDeclaration();
    if (ssp)
    {
        if (ssp == member)
            return TRUE;
    }
    if (isFriend(currentBase, funcsp) || ssp && isFriend(currentBase, ssp))
        consideredAccess = ac_private;
    if (!currentBase->tp->syms)
        return FALSE;
    hr = currentBase->tp->syms->table[0];
    matched = FALSE;
    while (hr)
    {
        SYMBOL *sym = (SYMBOL *)hr->p;
        if (sym == member || sym == member->mainsym)
        {
            matched = TRUE;
            break;
        }
        if (sym->storage_class == sc_overloads && isfunction(member->tp))
        {
            HASHREC *hr1 = sym->tp->syms->table[0];
            while (hr1)
            {
                SYMBOL *sym = (SYMBOL *)hr1->p;
                if (sym == member || sym == member->mainsym)
                {
                    break;
                }
                else if (sym->instantiations)
                {
                    LIST *lst = sym->instantiations;
                    while (lst)
                    {
                        if (lst->data == member)
                            break;
                        lst = lst->next;
                    }
                    if (lst)
                    {
                        break;
                    }
                }
                hr1 = hr1->next;
            }
            if (hr1)
            {
                matched = TRUE;
                break;
            }
        }
        hr = hr->next;
    }
    if (!matched)
    {
        hr = currentBase->tp->tags->table[0];
        while (hr)
        {
            SYMBOL *sym = (SYMBOL *)hr->p;
            if (sym == member || sym == member->mainsym)
            {
                matched = TRUE;
                break;
            }
            else if (sym->instantiations)
            {
                LIST *lst = sym->instantiations;
                while (lst)
                {
                    if (lst->data == member)
                        break;
                    lst = lst->next;
                }
                if (lst)
                {
                    matched = TRUE;
                    break;
                }
            }
            hr = hr->next;
        }
    }
    if (matched)
    {
        SYMBOL *sym = member;
        BOOL test = TRUE;
        // this is to take care of a special case with member pointers
        if (asAddress && sym->access == ac_protected)
        {
            SYMBOL *xx = sym->parentClass;
            test = FALSE;
            while (xx)
            {
                if (xx == derived)
                {
                    test = TRUE;
                    break;
                }
                xx = xx->parentClass;
            }
        }
        return derived == currentBase && consideredAccess <= ac_protected 
                || sym->access >= consideredAccess && test;
    }
    lst = currentBase->baseClasses;
    while (lst)
    {
        if (lst->accessLevel == ac_private)
            consideredAccess = ac_public;
        else if (lst->accessLevel == ac_protected && minAccess == ac_public)
            consideredAccess = ac_public;
        else
            consideredAccess = minAccess;
        // we have to go through the base classes even if we know that a normal
        // lookup wouldn't work, so we can check their friends lists...
        if (isAccessible(derived, lst->cls, member, funcsp, consideredAccess, asAddress))
            return TRUE;
        lst = lst->next;
    }
    return FALSE;
}
BOOL isExpressionAccessible(SYMBOL *sym, SYMBOL *funcsp, EXPRESSION *exp, BOOL asAddress)
{
    if (sym->parentClass)
    {
        SYMBOL *ssp = getStructureDeclaration();
        BOOL throughClass = sym->throughClass;
        if (exp)
        {
            if (exp->type == en_l_p && exp->left->type == en_auto && exp->left->v.sp->thisPtr)
                throughClass = TRUE;
        }
        
        if (ssp && throughClass && (ssp == sym->parentClass || classRefCount(sym->parentClass, ssp)))
        {
            if (!isAccessible(ssp, ssp, sym, funcsp, ac_protected, asAddress))
                return FALSE;
        }
        else
        {
            if (!isAccessible(sym->parentClass, sym->parentClass, sym, funcsp, ac_public, asAddress))
                return FALSE;
        }
    }
    return TRUE;    
}
BOOL checkDeclarationAccessible(TYPE *tp, SYMBOL *funcsp)
{
    while (tp)
    {
        if (isstructured(tp) || tp->type == bt_enum)
        {
            SYMBOL *sym = basetype(tp)->sp;
            if (sym->parentClass)
            {
                SYMBOL *ssp = getStructureDeclaration();
                if (ssp && (ssp == sym->parentClass || classRefCount(sym->parentClass, ssp)))
                {
                    if (!isAccessible(ssp, ssp, sym, funcsp, ac_protected, FALSE))
                    {
                        currentErrorLine = 0;
                        errorsym(ERR_CANNOT_ACCESS, tp->sp);		
                        return FALSE;
                    }
                }
                else
                {
                    if (!isAccessible(sym->parentClass, sym->parentClass, sym, funcsp, ac_public, FALSE))
                    {
                        currentErrorLine = 0;
                        errorsym(ERR_CANNOT_ACCESS, tp->sp);		
                        return FALSE;
                    }
                }
            }
        }
        else if (isfunction(tp))
        {
            HASHREC *hr = basetype(tp)->syms->table[0];
            while (hr)
            {
                SYMBOL *sym = (SYMBOL *)hr->p;
                if (!checkDeclarationAccessible(sym->tp, funcsp))
                    return FALSE;
                hr = hr->next;
            }            
        }
        tp = tp->btp;
    }
    return TRUE;
}
static LIST *searchNS(SYMBOL *sp, SYMBOL *nssp, LIST *in)
{
    if (nssp)
    {
        NAMESPACEVALUES *ns = nssp->nameSpaceValues;
        LIST *x = namespacesearchInternal(sp->name, ns, TRUE, FALSE, FALSE);
        if (x)
        {
            LIST *rv = x;
            if (in)
            {
                while (x->next)
                    x = x->next;
                x->next = in;
            }
            return rv;
        }
    }
    return in;
}
SYMBOL *lookupSpecificCast(SYMBOL *sp, TYPE *tp)
{
    return getUserConversion(F_STRUCTURE, tp, sp->tp, NULL, NULL, NULL, NULL);
}
SYMBOL *lookupNonspecificCast(SYMBOL *sp, TYPE *tp)
{
    return getUserConversion(0, tp, sp->tp, NULL, NULL, NULL, NULL);
}
SYMBOL *lookupIntCast(SYMBOL *sp, TYPE *tp)
{
    return getUserConversion(F_INTEGER, tp, sp->tp, NULL, NULL, NULL, NULL);
}
SYMBOL *lookupArithmeticCast(SYMBOL *sp, TYPE *tp)
{
    return getUserConversion(F_ARITHMETIC, tp, sp->tp, NULL, NULL, NULL, NULL);
}
SYMBOL *lookupPointerCast(SYMBOL *sp, TYPE *tp)
{
    return getUserConversion(F_POINTER, tp, sp->tp, NULL, NULL, NULL, NULL);
}
static LIST *structuredArg(SYMBOL *sp, LIST *in, TYPE *tp)
{
    return searchNS(sp, basetype(tp)->sp->parentNameSpace, in) ;
}
static LIST *searchOneArg(SYMBOL *sp, LIST *in, TYPE *tp);
static LIST *funcArg(SYMBOL *sp, LIST *in, TYPE *tp)
{
    HASHREC **hr = tp->syms->table;
    while (*hr)
    {
        SYMBOL *sym = (SYMBOL *)(*hr)->p;
        in = searchOneArg(sp, in, sym->tp);
        hr = &(*hr)->next;
    }
    in = searchOneArg(sp, in , basetype(tp)->btp);
    return in;
}
static LIST *searchOneArg(SYMBOL *sp, LIST *in, TYPE *tp)
{
    if (ispointer(tp)|| isref(tp))
        return searchOneArg(sp, in, basetype(tp)->btp);
    if (isarithmetic(tp))
        return in;
    if (isstructured(tp))
        return structuredArg(sp, in, tp);
    if (basetype(tp)->type  == bt_enum)
        return searchNS(sp, tp->sp->parentNameSpace, in) ;
    if (isfunction(tp))
        return funcArg(sp, in, tp);
    // member pointers...
    return in;
}
static void weedToFunctions(LIST **lst)
{
    while (*lst)
    {
        SYMBOL *sp = (SYMBOL *)(*lst)->data;
        if (sp->storage_class != sc_overloads)
            *lst = (*lst)->next;
        else
            lst = &(*lst)->next;
    }
}
static const int rank[] = 
{
    0,0,0,0,0,1,1,2,2,2,2,2,2,3,4,5,6,6,7
};
enum e_cvsrn
{
    // tier 1
    CV_IDENTITY,
    CV_LVALUETORVALUE,
    CV_ARRAYTOPOINTER,
    CV_FUNCTIONTOPOINTER,
    CV_QUALS,
    CV_INTEGRALPROMOTION,
    CV_FLOATINGPROMOTION,
    CV_INTEGRALCONVERSION,
    CV_FLOATINGCONVERSION,
    CV_FLOATINGINTEGRALCONVERSION,
    CV_POINTERCONVERSION,
    CV_POINTERTOMEMBERCONVERSION,
    CV_PTRBOOLCONVERSION, 
    CV_DERIVEDFROMBASE,
    // tier 2
    CV_USER,
    // tier 3
    CV_ELLIPSIS,
    // other
    CV_PAD,
    CV_AMBIGUOUS,
    CV_NONE,
} ;
static void  GatherConversions(SYMBOL *sp, SYMBOL **spList, int n, FUNCTIONCALL *args, 
                               TYPE *atp, enum e_cvsrn **icsList, int **lenList, int argCount, SYMBOL **funcList)
{
    int i;
    for (i=0; i < n; i++)
    {
        int j;
        if (spList[i])
        {
            enum e_cvsrn arr[500][3];
            int counts[500];
            SYMBOL **funcs[200];
            BOOL t;
            memset(counts, 0, argCount * sizeof(int));
            for (j = i + 1; j < n; j++)
                if (spList[i] == spList[j])
                    spList[j] = 0;
            memset(funcs, 0, sizeof(funcs));
            t = getFuncConversions(spList[i], args, atp, sp->parentClass, arr, counts, argCount, funcs);
            if (!t)
            {
                spList[i] = NULL;
            }
            else
            {
                int n = 0;
                for (j=0; j < argCount; j++)
                    n += counts[j];
                icsList[i] = (enum e_cvsrn *)Alloc(sizeof(enum e_cvsrn) * n);
                memcpy(icsList[i], arr, n * sizeof(enum e_cvsrn));
                lenList[i] = (int *)Alloc(sizeof(int) * argCount);
                memcpy(lenList[i], counts, argCount * sizeof(int));
                funcList[i] = (SYMBOL **)Alloc(sizeof(SYMBOL *) * argCount);
                memcpy(funcList[i], funcs, argCount * sizeof(SYMBOL *));
            }
        }
    }
}
enum e_ct { conv, user, ellipses };
static BOOL ismem(EXPRESSION *exp)
{
    switch (exp->type)
    {
        case en_global:
        case en_label:
        case en_pc:
        case en_auto:
        case en_threadlocal:
            return TRUE;
        case en_add:
        case en_sub:
            return ismem(exp->left) || ismem(exp->right);
        default:
            return FALSE;
    }
}
static TYPE *toThis(TYPE *tp)
{
    TYPE *tpx;
    if (ispointer(tp))
        return tp;
    tpx = Alloc(sizeof(TYPE));
    tpx->type = bt_pointer;
    tpx->size = getSize(bt_pointer);
    tpx->btp = tp;
    return tpx;
}
static int compareConversions(SYMBOL *spLeft, SYMBOL *spRight, enum e_cvsrn *seql, enum e_cvsrn *seqr,
                              TYPE *ltype, TYPE *rtype, TYPE *atype, EXPRESSION *expa, 
                              SYMBOL *funcl, SYMBOL *funcr, 
                              int lenl, int lenr, BOOL fromUser)
{
    enum e_ct xl=conv, xr=conv;
    int rankl, rankr;
    int i,j;
    int n,q;
    // must be of same general type, types are standard conversion, user defined conversion, ellipses
    for (i=0; i < lenl; i++)
    {
        if (seql[i] == CV_ELLIPSIS)
            xl = ellipses;
        if (xl != ellipses && seql[i] == CV_USER)
            xl = user;
    }
    for (i=0; i < lenr; i++)
    {
        if (seqr[i] == CV_ELLIPSIS)
            xr = ellipses;
        if (xr != ellipses && seqr[i] == CV_USER)
            xr = user;
    }
    if (xl != xr)
    {
        if (xl < xr)
            return -1;
        else
            return 1;
    }
    if (xl == conv)
    {
        // one seq is a subseq of the other
        int l = 0, r = 0;
        for (i=0; i < lenl && i < lenr; i++)
        {
            switch (seql[l])
            {   
                case CV_LVALUETORVALUE:
                case CV_ARRAYTOPOINTER:
                case CV_FUNCTIONTOPOINTER:
                    i++;
                    continue;
            }
            switch (seqr[r])
            {   
                case CV_LVALUETORVALUE:
                case CV_ARRAYTOPOINTER:
                case CV_FUNCTIONTOPOINTER:
                    r++;
                    continue;
            }
            if (seql[l] != seql[r])
                break;
        }
        while (l < lenl && seql[l] == CV_IDENTITY)
            l++;
        while (r < lenr && seqr[r] == CV_IDENTITY)
            r++;
        if (l == lenl && r != lenr)
        {
            return -1;
        }
        else if (l != lenl && r == lenr)
        {
            return 1;
        }
        // compare ranks
        rankl = CV_IDENTITY;
        for (l=0; l < lenl; l++)
            if (rank[seql[l]] > rankl)
                rankl = rank[seql[l]];
        rankr = CV_IDENTITY;
        for (r=0; r < lenr; r++)
            if (rank[seqr[r]] > rankr)
                rankr = rank[seqr[r]];
        if (rankl < rankr)
            return -1;
        else if (rankr < rankl)
            return 1;
        else // ranks are same, do same rank comparisons
        {
            TYPE *tl = ltype, *tr = rtype, *ta = atype;
            // check if one or the other but not both converts a pointer to bool
            rankl = 0;
            for (l=0; l < lenl; l++)
                if (seql[l] == CV_PTRBOOLCONVERSION)
                    rankl = 1;
            rankr = 0;
            for (r=0; r < lenr; r++)
                if (seqr[r] == CV_PTRBOOLCONVERSION)
                    rankr = 1;
            if (rankl != rankr)
                if (rankl)
                    return 1;
                else 
                    return -1;
            if (fromUser)
            {
                // conversion from pointer to base class to void * is better than pointer
                // to derived class to void *
                if (ispointer(ta) &&basetype(basetype(ta)->btp)->type == bt_void)
                {
                    SYMBOL *second = basetype(basetype(tl)->btp)->sp;
                    SYMBOL *first = basetype(basetype(tr)->btp)->sp;
                    int v;
                    v = classRefCount(first, second);
                    if (v == 1)
                        return 1;
                    v = classRefCount(second, first);
                    if (v == 1)
                        return -1;
                }
                                
            }
            else if (ta)
            {
                // conversion to pointer to base class is better than conversion to void *
                if (ispointer(tl) && ispointer(ta) && basetype(basetype(tl)->btp)->type == bt_void)
                {
                    if (isstructured(basetype(ta)->btp))
                    {
                        if (ispointer(tr) && isstructured(basetype(tr)->btp))
                        {
                            SYMBOL *derived = basetype(basetype(ta)->btp)->sp;
                            SYMBOL *base = basetype(basetype(tr)->btp)->sp;
                            int v = classRefCount(base, derived);
                            if (v == 1)
                                return 1;
                        }
                    }
                }
                else if (ispointer(tr) && ispointer(ta) && basetype(basetype(tr)->btp)->type == bt_void)
                {
                    if (isstructured(basetype(ta)->btp))
                    {
                        if (ispointer(tl) && isstructured(basetype(tl)->btp))
                        {
                            SYMBOL *derived = basetype(basetype(ta)->btp)->sp;
                            SYMBOL *base = basetype(basetype(tl)->btp)->sp;
                            int v = classRefCount(base, derived);
                            if (v == 1)
                                return -1;
                        }
                    }
                }
            }
            // various rules for the comparison of two pairs of structures
            if (ispointer(ta) && ispointer(tr) && ispointer(tl))
            {
                ta = basetype(ta)->btp;
                tl = basetype(tl)->btp;
                tr = basetype(tr)->btp;
            }
            else
            {
                if (isref(ta))
                    ta = basetype(ta)->btp;
                if (isref(tl))
                    tl = basetype(tl)->btp;
                if (isref(tr))
                    tr = basetype(tr)->btp;
            }
            if (isstructured(ta) && isstructured(tl) && isstructured(tr))
            {
                ta = basetype(ta);
                tl = basetype(tl);
                tr = basetype(tr);
                if (fromUser)
                {
                    if (classRefCount(ta->sp, tl->sp ) == 1 && classRefCount(ta->sp, tr->sp) == 1)
                    {
                        if (classRefCount(tl->sp, tr->sp) == 1)
                        {
                            return -1;
                        }
                        else if (classRefCount(tr->sp, tl->sp) == 1)
                        {
                            return 1;
                        }
                    }
                }
                else
                {
                    if (classRefCount(tl->sp, ta->sp ) == 1 && classRefCount(tr->sp, ta->sp) == 1)
                    {
                        if (classRefCount(tl->sp, tr->sp) == 1)
                        {
                            return 1;
                        }
                        else if (classRefCount(tr->sp, tl->sp) == 1)
                        {
                            return -1;
                        }
                    }
                }
            }
            if (basetype(ta)->type == bt_memberptr && basetype(tl)->type == bt_memberptr && basetype(tr)->type == bt_memberptr)
            {
                ta = basetype(ta);
                tl = basetype(tl);
                tr = basetype(tr);
                if (fromUser)
                {
                    if (classRefCount(tl->sp, ta->sp ) == 1 && classRefCount(tr->sp, ta->sp) == 1)
                    {
                        if (classRefCount(tl->sp, tr->sp) == 1)
                        {
                            return 1;
                        }
                        else if (classRefCount(tr->sp, tl->sp) == 1)
                        {
                            return -1;
                        }
                    }
                }
                else
                {
                    if (classRefCount(ta->sp, tl->sp ) == 1 && classRefCount(ta->sp, tr->sp) == 1)
                    {
                        if (classRefCount(tl->sp, tr->sp) == 1)
                        {
                            return -1;
                        }
                        else if (classRefCount(tr->sp, tl->sp) == 1)
                        {
                            return 1;
                        }
                    }
                }
            }
        }
        // compare qualifiers at top level
        rankl = isconst(ltype) + isvolatile(ltype) * 2;
        rankr = isconst(rtype) + isvolatile(rtype) * 2;
        if (rankl != rankr)
        {
            if (comparetypes(basetype(ltype), basetype(rtype), TRUE))
            {
                int n = rankl ^ rankr;
                if ((n & rankl) && !(n & rankr))
                    return 1;
                if ((n & rankr) && !(n & rankl))
                    return -1;
            }
        }
        if (atype && isref(rtype) && isref(ltype))
        {
            // rvalue matches an rvalue reference better than an lvalue reference
            
            if (isref(rtype) && isref(ltype) && basetype(ltype)->type != basetype(rtype)->type)
            {
                int lref = expa && (lvalue(expa) && (expa->left->type != en_l_ref || !expa->left->rref));
                int rref = expa && (expa->type == en_l_ref && expa->left->rref || !lvalue(expa) && (!isstructured(rtype) || !ismem(expa)) );
                if (expa && expa->type == en_func)
                {
                    TYPE *tp = basetype(expa->v.func->sp->tp)->btp;
                    if (tp)
                    {
                        if (tp->type == bt_rref)
                            rref = TRUE;
                        if (tp->type == bt_lref)
                            lref = TRUE;
                    }
                }
                lref |= expa && isstructured(atype) && expa->type != en_not_lvalue;
                if (basetype(ltype)->type == bt_rref)
                    if (lref)
                        return 1;
                    else if (rref)
                        return -1;
                else 
                    if (basetype(rtype)->type == bt_lref)
                        if (lref)
                            return 1;
                        else if (rref)
                            return -1;
            }
            // compare qualifiers at top level
            rankl = isconst(basetype(ltype)->btp) + isvolatile(basetype(ltype)->btp) * 2;
            rankr = isconst(basetype(rtype)->btp) + isvolatile(basetype(rtype)->btp) * 2;
            if (rankl != rankr)
            {
                if (comparetypes(basetype(basetype(ltype)->btp), basetype(basetype(rtype)->btp), TRUE))
                {
                    int n = rankl ^ rankr;
                    if ((n & rankl) && !(n & rankr))
                        return 1;
                    if ((n & rankr) && !(n & rankl))
                        return -1;
                }
            }
        }
    }
    else if (xl == user)
    {
        int l=0, r=0;
        if (seql[l] == CV_DERIVEDFROMBASE && seqr[r] == CV_DERIVEDFROMBASE)
        {
            HASHREC *hr = basetype(funcl->tp)->syms->table[0];
            if (!funcl->castoperator)
                hr = hr->next;
            ltype = ((SYMBOL *)(hr->p))->tp;
            hr = basetype(funcr->tp)->syms->table[0];
            if (!funcr->castoperator)
                hr = hr->next;
            rtype = ((SYMBOL *)(hr->p))->tp;
            if (!comparetypes(ltype, rtype, TRUE))
                return 0;
        }
        if (seql[l] = CV_USER && seqr[r] == CV_USER && funcl && funcr)
        {
            int v1, v2;
            TYPE *atype1, *atype2;
            if (funcl->castoperator)
            {
                ltype = basetype(funcl->tp)->btp;
                atype1 = ((SYMBOL *)basetype(funcl->tp)->syms->table[0]->p)->tp;
            }
            else
            {
                ltype = ((SYMBOL *)basetype(funcl->tp)->syms->table[0]->p)->tp;
                atype1 = basetype(funcl->tp)->btp;
            }
            if (funcr->castoperator)
            {
                rtype = basetype(funcr->tp)->btp;
                atype2 = ((SYMBOL *)basetype(funcr->tp)->syms->table[0]->p)->tp;
            }
            else
            {
                rtype = ((SYMBOL *)basetype(funcr->tp)->syms->table[0]->p)->tp;
                atype2 = basetype(funcr->tp)->btp;
            }
            v1 = compareConversions(funcl, funcr, seql +l, seqr + r, 
                                    ltype, rtype, atype1, NULL, NULL, NULL, lenl-l, lenr-r, TRUE);
            v2 = compareConversions(funcl, funcr, seql +l, seqr + r, 
                                    ltype, rtype, atype2, NULL, NULL, NULL, lenl-l, lenr-r, TRUE);
            if (v1 != v2)
                if (v1 == 0)
                    return 1;
                else if (v2 == 0)
                    return -1;
                else
                    return 0;
            else
                return v1;                    
        }
    }
    // ellipse always returns 0;
    return 0;
}
static void SelectBestFunc(SYMBOL ** spList, enum e_cvsrn **icsList, 
                                int **lenList, FUNCTIONCALL *funcparams,
                                int argCount, int funcCount, SYMBOL ***funcList)
{
    static const enum e_cvsrn identity = CV_IDENTITY;
    char arr[500];
    int i, j;
    for (i=0; i < funcCount; i++)
    {
        for (j=i+1; j < funcCount && spList[i]; j++)
        {
            if (spList[j])
            {
                int left=0, right=0;
                int l=0,r=0;
                int k=0;
                INITLIST *args = funcparams ? funcparams->arguments : NULL;
                HASHREC *hrl = basetype(spList[i]->tp)->syms->table[0];
                HASHREC *hrr = basetype(spList[j]->tp)->syms->table[0];
                for (k=0; k < argCount; k++)
                {
                    enum e_cvsrn *seql = &icsList[i][l];
                    enum e_cvsrn *seqr = &icsList[j][r];
                    int lenl = lenList[i][k];
                    int lenr = lenList[j][k];
                    if (!lenl)
                    {
                        seql = &identity;
                        lenl = 1;
                    }
                    if (!lenr)
                    {
                        seqr = &identity;
                        lenr = 1;
                    }
                    if (k == 0 && funcparams && funcparams->thisptr)
                    {
                        TYPE *tpl, *tpr;
                        if (spList[i]->castoperator)
                        {
                            tpl = toThis(basetype(spList[i]->tp)->btp);
                        }
                        else
                        {
                            tpl = ((SYMBOL *)(hrl->p))->tp;
                            hrl = hrl->next;
                        }
                        if (spList[j]->castoperator)
                        {
                            tpr = toThis(basetype(spList[j]->tp)->btp);
                        }
                        else
                        {
                            tpr = ((SYMBOL *)(hrr->p))->tp;
                            hrr = hrr->next;
                        }
                        arr[k] = compareConversions(spList[i], spList[j], seql, seqr, tpl, tpr, 
                                                    funcparams->thistp, funcparams->thisptr,
                                                    funcList ? funcList[i][k] : NULL, 
                                                    funcList ? funcList[j][k] : NULL,
                                                    lenl, lenr, FALSE);
                    }
                    /*
                    else if (k == 1 && funcparams && funcparams->thisptr)
                    {
                        TYPE *tpl, *tpr;
                        if (spList[i]->castoperator)
                        {
                            tpl = ((SYMBOL *)(hrl->p))->tp;
                            hrl = hrl->next;
                        }
                        else
                        {
                            tpl = toThis(basetype(spList[i]->tp)->btp);
                        }
                        if (spList[j]->castoperator)
                        {
                            tpr = ((SYMBOL *)(hrr->p))->tp;
                            hrr = hrr->next;
                        }
                        else
                        {
                            tpr = toThis(basetype(spList[j]->tp)->btp);
                        }
                        arr[k] = compareConversions(spList[i], spList[j], seql, seqr, tpl, tpr, 
                                                    args ? args->tp : 0 , args ? args->exp : 0,
                                                    funcList ? funcList[i][k] : NULL, 
                                                    funcList ? funcList[j][k] : NULL,
                                                    lenl, lenr, FALSE);
                    }
                    */
                    else
                    {
                        TYPE *tpl = hrl ? ((SYMBOL *)hrl->p)->tp : NULL;
                        TYPE *tpr = hrr ? ((SYMBOL *)hrr->p)->tp : NULL;
                        if (tpl && tpr)
                            arr[k] = compareConversions(spList[i], spList[j], seql, seqr, tpl, tpr, 
                                                    args? args->tp:0, args?args->exp:0,
                                                    funcList ? funcList[i][k] : NULL, 
                                                    funcList ? funcList[j][k] : NULL,
                                                    lenl, lenr, FALSE);
                        else
                            arr[k] = 0;
                        if (hrl) hrl = hrl->next;
                        if (hrr) hrr = hrr->next;
                        if (args) args = args->next;
                    }
                    l += lenl;
                    r += lenr;
                }
                for (k=0; k < argCount; k++)
                {
                    if (arr[k] > 0)
                        right++;
                    else if (arr[k] < 0)
                        left++;
                }
                if (left && !right)
                {
                    spList[j] = NULL;
                }
                else if (right && !left)
                {
                    spList[i] = NULL;
                }
            }
        }
    }
}
static LIST *GetMemberCasts(LIST *gather, SYMBOL *sp)
{
    if (sp)
    {
        BASECLASS *bcl = sp->baseClasses;
        SYMBOL *sym = sp;
        while (sym)
        {
            // conversion of one class to another
            SYMBOL *find = search(overloadNameTab[CI_CAST], basetype(sym->tp)->syms);
            if (find)
            {
                LIST *lst = Alloc(sizeof(LIST));
                lst->data = find;
                lst->next = gather;
                gather = lst;
            }
            if (bcl)
            {
                sym = bcl->cls;
                bcl = bcl->next;
            }
            else
            {
                sym = NULL;
            }
        }
    }
    return gather;
}
static LIST *GetMemberConstructors(LIST *gather, SYMBOL *sp)
{
    BASECLASS *bcl = sp->baseClasses;
    SYMBOL *sym = sp;
    while (sym)
    {
        // conversion of one class to another
        SYMBOL *find = search(overloadNameTab[CI_CONSTRUCTOR], basetype(sym->tp)->syms);
        if (find)
        {
            LIST *lst = Alloc(sizeof(LIST));
            lst->data = find;
            lst->next = gather;
            gather = lst;
        }
        if (bcl)
        {
            sym = bcl->cls;
            bcl = bcl->next;
        }
        else
        {
            sym = NULL;
        }
    }
    return gather;
}
static void getSingleConversion(TYPE *tpp, TYPE *tpa, EXPRESSION *expa, int *n, enum e_cvsrn *seq, SYMBOL **userFunc);
static SYMBOL *getUserConversion(int flags,
                          TYPE *tpp, TYPE *tpa, EXPRESSION *expa,
                              int *n, enum e_cvsrn *seq, SYMBOL **userFunc)
{
    static BOOL infunc = FALSE;
    if (!infunc)
    {
        LIST *gather = NULL;
        if (tpp->type == bt_typedef)
            tpp = tpp->btp;
        infunc = TRUE;
        if (flags & F_WITHCONS)
        {
            TYPE *tppp = tpp;
            if (isref(tppp))
                tppp = basetype(tppp)->btp;
            if (isstructured(tppp))
            {
                gather = GetMemberConstructors(gather, basetype(tppp)->sp);
            }
        }
        gather = GetMemberCasts(gather, basetype(tpa)->sp);
        if (gather)
        {
            LIST *lst2;
            int funcs = 0;
            int i;
            SYMBOL **spList;
            enum e_cvsrn **icsList;
            int **lenList;
            int m = 0;
            SYMBOL *found1, *found2;
            FUNCTIONCALL funcparams;
            INITLIST args;
            TYPE thistp;
            EXPRESSION exp;
            lst2 = gather;
            memset(&funcparams, 0, sizeof(funcparams));
            memset(&args, 0, sizeof(args));
            memset(&thistp, 0, sizeof(thistp));
            memset(&exp, 0, sizeof(exp));
            while (lst2)
            {
                HASHREC **hr = ((SYMBOL *)lst2->data)->tp->syms->table;
                while (*hr)
                {
                    funcs++;
                    hr = &(*hr)->next;
                }
                lst2 = lst2->next;
            }
            spList = (SYMBOL **)Alloc(sizeof(SYMBOL *) * funcs);
            icsList = (enum e_cvsrn **)Alloc(sizeof(enum e_cvsrn *) * funcs);
            lenList = (int **)Alloc(sizeof(int *) * funcs);
            lst2 = gather;
            i = 0 ;
            while (lst2)
            {
                HASHREC **hr = ((SYMBOL *)lst2->data)->tp->syms->table;
                while (*hr)
                {
                    spList[i++] = (SYMBOL *)(*hr)->p;
                    hr = &(*hr)->next;
                }
                lst2 = lst2->next;
            }
            memset(&exp, 0, sizeof(exp));
            exp.type = en_not_lvalue;
            for (i=0; i < funcs; i++)
            {
                SYMBOL *candidate = spList[i];
                int j;
                int n3 = 0, n2 = 0, m1;
                enum e_cvsrn seq3[50];
                if (candidate->castoperator)
                {
                    TYPE *tpc = basetype(candidate->tp)->btp;
                    if (isref(tpc))
                        tpc = basetype(tpc)->btp;
                    if ((flags & F_INTEGER) && !isint(tpc)
                        || (flags & F_POINTER) && !ispointer(tpc) && basetype(tpc)->type != bt_memberptr
                        || (flags & F_ARITHMETIC) && !isarithmetic(tpc)
                        || (flags & F_STRUCTURE) && !isstructured(tpc))
                    {
                        seq3[n2++] = CV_NONE;
                        seq3[n2+n3++] = CV_NONE;
                    }
                    else
                    {
                        HASHREC *args = basetype(candidate->tp)->syms->table[0];
                        BOOL lref = FALSE;
                        TYPE *tpn = basetype(candidate->tp)->btp;
                        if (isref(tpn))
                        {
                            if (basetype(tpn)->type == bt_lref)
                                lref = TRUE;
                        }
                        thistp.btp = tpa;
                        thistp.type = bt_pointer;
                        thistp.size = getSize(bt_pointer);
                        getSingleConversion(((SYMBOL *)args->p)->tp, &thistp, &exp, &n2, seq3, NULL);
                        seq3[n2+ n3++] = CV_USER;
                        getSingleConversion(tpp, basetype(candidate->tp)->btp, lref ? NULL : &exp, &n3, seq3 + n2, NULL);
                    }
                }
                else
                {
                    HASHREC *args = basetype(candidate->tp)->syms->table[0];
                    if (args)
                    {
                        SYMBOL *first, *next=NULL;
                        SYMBOL *th = (SYMBOL *)args->p;
                        args = args->next;
                        first = (SYMBOL *)args->p;
                        if (args->next)
                            next = (SYMBOL *) args->next->p;
                        if (!next || next->init)
                        {
                            getSingleConversion(first->tp, tpa, expa, &n2, seq3, &exp);
                            if (n2 && seq3[n2-1] == CV_IDENTITY)
                            {
                                n2--;
                            }
                            seq3[n2+n3++] = CV_USER;
                            getSingleConversion(tpp, basetype(basetype(th->tp)->btp)->sp->tp, &exp, &n3, seq3+n2, NULL);
                        }
                        else
                        {
                            seq3[n2++] = CV_NONE;
                        }
                    }
                }
                for (j=0; j < n2+n3; j++)
                    if (seq3[j] == CV_NONE)
                        break;
                m1 = n2+n3;
                while (m1 && seq3[m1-1] == CV_IDENTITY)
                    m1--;
                if (j >= n2+n3 && m1 < 4)
                {
                    lenList[i] = Alloc(sizeof(int) * 2);
                    icsList[i] = Alloc(sizeof(enum e_cvsrn) * (n2 + n3));
                    lenList[i][0] = n2;
                    lenList[i][1] = n3;
                    memcpy(&icsList[i][0], seq3, (n2 + n3) * sizeof(enum e_cvsrn));
                }
                else
                {
                    spList[i] = NULL;
                }
            }
            funcparams.arguments = &args;
            args.tp = tpa;
            funcparams.thisptr = expa;
            funcparams.thistp = &thistp;
            thistp.btp = tpa;
            thistp.type = bt_pointer;
            thistp.size = getSize(bt_pointer);
            SelectBestFunc(spList, icsList, lenList, &funcparams, 2, funcs, NULL);
            found1 = found2 = NULL;
            
            for (i=0; i < funcs && !found1; i++)
            {
                int j;
                found1 = spList[i];
                m = i;
                for (j=i+1; j < funcs && found1 && !found2; j++)
                {
                    if (spList[j])
                    {
                        found2 = spList[j];
                    }
                }
            }
            if (found1)
            {
                if (found2)
                {
                     errorsym2(ERR_AMBIGUITY_BETWEEN, found1, found2);
                }
                else
                {
                    if (seq)
                    {
                        int l = lenList[m][0] + lenList[m][1];
                        memcpy(&seq[*n], &icsList[m][0], l * sizeof(enum e_cvsrn));
                        *n += l;
                        if (userFunc)
                            *userFunc = found1;
                    }
                    infunc = FALSE;
                    return found1;
                }
            }
        }
        infunc = FALSE;
    }
    if (seq)
        seq[(*n)++] = CV_NONE;
    return NULL;
}
static void getQualConversion(TYPE *tpp, TYPE *tpa, int *n, enum e_cvsrn *seq)
{
    BOOL hasconst = TRUE, hasvol = TRUE;
    BOOL sameconst = TRUE, samevol = TRUE;
    BOOL first = TRUE;
    while (tpa && tpp && ispointer(tpa) && ispointer(tpp))
    {
        if (isconst(tpp) != isconst(tpa))
        {
            sameconst = FALSE;
            if (isconst(tpa) && !isconst(tpp))
                break;
            if (!hasconst)
                break;
        }
        if (isvolatile(tpp) != isvolatile(tpa))
        {
            samevol = FALSE;
            if (isvolatile(tpa) && !isvolatile(tpp))
                break;
            if (!hasvol)
                break;
        }
            
        if (!first)
        {
            if (!isconst(tpp))
                hasconst = FALSE;
            if (!isvolatile(tpp))
                hasvol = FALSE;
        }
        first = FALSE;
        tpa = basetype(tpa)->btp;
        tpp = basetype(tpp)->btp;
    }
    if (!tpa && !tpp || tpa->type != bt_pointer && tpp->type != bt_pointer)
    {
        if (!sameconst || !samevol)
            seq[(*n)++] = CV_QUALS;
    }
    else
    {
            seq[(*n)++] = CV_NONE;
    }
}
static void getPointerConversion(TYPE *tpp, TYPE *tpa, EXPRESSION *exp, int *n, 
                                 enum e_cvsrn *seq)
{
    if (basetype(tpa)->btp->type == bt_void && exp && (isconstzero(&stdint, exp) || exp->type == en_nullptr))
    {
        seq[(*n)++] = CV_POINTERCONVERSION;
        return;
    }
    else 
    {
        if (basetype(tpa)->array)
            seq[(*n)++] = CV_ARRAYTOPOINTER;
        if (isfunction(basetype(tpa)->btp))
            seq[(*n)++] = CV_FUNCTIONTOPOINTER;
        if (basetype(basetype(tpp)->btp)->type == bt_void)
        {
            seq[(*n)++] = CV_POINTERCONVERSION;
        }
        else if (isstructured(basetype(tpp)->btp) && isstructured(basetype(tpa)->btp))
        {
            SYMBOL *base = basetype(basetype(tpp)->btp)->sp;
            SYMBOL *derived = basetype(basetype(tpa)->btp)->sp;
            if (base != derived)
            {
                int v = classRefCount(base, derived);
                if (v != 1)
                {
                    seq[(*n)++] = CV_NONE;
                }
                else
                {
                    seq[(*n)++]= CV_POINTERCONVERSION;
                }
            }
            else
            {
                seq[(*n)++] = CV_IDENTITY;
            }
        }
        else if (!comparetypes(tpp, tpa, TRUE))
        {
            seq[(*n)++] = CV_NONE;
        }
        getQualConversion(tpp, tpa, n, seq);
    }
}
static void getSingleConversion(TYPE *tpp, TYPE *tpa, EXPRESSION *expa, int *n, enum e_cvsrn *seq, SYMBOL **userFunc)
{
    BOOL lref;
    BOOL rref;
    tpa = basetype(tpa);
    while (expa && expa->type == en_void)
        expa = expa->right;
    if (tpp->type != tpa->type && (tpp->type == bt_void || tpa->type == bt_void))
    {
         seq[(*n)++] = CV_NONE;
         return;
    }
    lref = expa && (lvalue(expa) && (expa->left->type != en_l_ref || !expa->left->rref) || isarithmeticconst(expa)) ;
    rref = expa && (expa->type == en_l_ref && expa->left->rref || !lvalue(expa) && !isarithmeticconst(expa) && (!isstructured(tpa) || !ismem(expa)) );
    if (expa && expa->type == en_func)
    {
        TYPE *tp = basetype(expa->v.func->sp->tp)->btp;
        if (tp)
        {
            if (tp->type == bt_rref)
            {
                rref = TRUE;
                lref = FALSE;
            }
            else if (tp->type == bt_lref)
            {
                lref = TRUE;
                rref = FALSE;
            }
        }
    }
    if (isref(tpa))
    {
        if (basetype(tpa)->type == bt_rref)
        {
            rref = TRUE;
            lref = FALSE;
        }
        else if (basetype(tpa)->type == bt_lref)
        {
            lref = TRUE;
            rref = FALSE;
        }
        tpa = basetype(tpa)->btp;
    }
    if (isref(tpp))
    {
        TYPE *tppp = basetype(tpp)->btp;
        lref |= expa && isstructured(tppp) && expa->type != en_not_lvalue;
        if (isconst(tpa) && !isconst(tppp)
            || isvolatile(tpa) && !isvolatile(tppp))
            seq[(*n)++] = CV_NONE;
        if (lref && tppp->type == bt_rref)
            seq[(*n)++] = CV_LVALUETORVALUE;
        if (tpp->type == bt_rref && lref && !isfunction(tpa))
        {
            // lvalue to rvalue ref not allowed unless the lvalue is nonvolatile and const
            seq[(*n)++] = CV_NONE;
        }
        else if (tpp->type == bt_lref && rref)
        {
            // rvalue to lvalue ref not allowed unless the lvalue is a function
            if (isconst(tppp) && !isvolatile(tppp))
                seq[(*n)++] = CV_QUALS;
            else
                seq[(*n)++] = CV_LVALUETORVALUE;//CV_NONE;
        }
        tpa = basetype(tpa);
        if (isstructured(tpa))
        {
            if (isstructured(tppp))
            {
                if (basetype(tpa)->sp != basetype(tppp)->sp)
                {
                    if (classRefCount(basetype(tppp)->sp, basetype(tpa)->sp) == 1)
                    {
                        seq[(*n)++] = CV_DERIVEDFROMBASE;
                    }
                    else
                    {
                        getUserConversion(F_WITHCONS, tpp, tpa, expa, n, seq, userFunc);
                    }
                }
                else
                {
                    seq[(*n)++] = CV_IDENTITY;
                }
            }
            else
            {
                getUserConversion(0, tpp, tpa, expa, n, seq, userFunc);
            }
        }
        else if (isstructured(tppp))
        {
            getUserConversion(F_WITHCONS, tpp, tpa, expa, n, seq, userFunc);
        }
        else
        {
            getSingleConversion(tppp, tpa, expa, n, seq, userFunc);
        }
    }
    else
    {
        if (isstructured(tpa))
        {
            if (isstructured(tpp))
            {
                if (basetype(tpa)->sp == basetype(tpp)->sp)
                {
                    seq[(*n)++] = CV_IDENTITY;
                }
                else if (classRefCount(basetype(tpp)->sp, basetype(tpa)->sp) == 1)
                {
                    seq[(*n)++] = CV_DERIVEDFROMBASE;
                }
                else
                {
                    getUserConversion(F_WITHCONS, tpp, tpa, expa, n, seq, userFunc);
                }
            }
            else
            {
                getUserConversion(0, tpp, tpa, expa, n, seq, userFunc);
            }
        }
        else if (isstructured(tpp))
        {
            seq[(*n)++] = CV_NONE;
        }
        else if (isfuncptr(tpp))
        {
            tpp = basetype(tpp->btp);
            if (isfuncptr(tpa))
                tpa = basetype(tpa->btp);
            if (comparetypes(tpp, tpa, TRUE))
            {
                seq[(*n)++] = CV_IDENTITY;
            }
            else if (isint(tpa) && expa && (isconstzero(tpa, expa) || expa->type == en_nullptr))
            {
                seq[(*n)++] = CV_POINTERCONVERSION;
            }
            else
            {
                seq[(*n)++] = CV_NONE;
            }                            
        }
        else if (ispointer(tpp))
        {
            if (ispointer(tpa))
            {
                // cvqual
                getPointerConversion(tpp, tpa, expa, n, seq);
            }
            else if (isint(tpa) && expa && (isconstzero(tpa, expa) || expa->type == en_nullptr))
            {
                seq[(*n)++] = CV_POINTERCONVERSION;
            }
            else
            {
                seq[(*n)++] = CV_NONE;
            }
        }
        else if (ispointer(tpa))
        {
            if (basetype(tpp)->type == bt_bool)
            {
                seq[(*n)++] = CV_PTRBOOLCONVERSION;
            }
            else
            {
                seq[(*n)++] = CV_NONE;
            }
        }
        else if (basetype(tpp)->type == bt_memberptr)
        {
            if (basetype(tpa)->type == bt_memberptr)
            {
                if (comparetypes(basetype(tpp)->btp, basetype(tpa)->btp, TRUE))
                {
                    if (basetype(tpa)->sp != basetype(tpp)->sp)
                    {
                        if (classRefCount(basetype(tpa)->sp, basetype(tpp)->sp) == 1)
                        {
                            seq[(*n)++] = CV_POINTERTOMEMBERCONVERSION;
                        }
                        else
                        {
                            getUserConversion(F_WITHCONS, tpp, tpa, expa, n, seq, userFunc);
                        }
                    }
                    else
                    {
                        seq[(*n)++] = CV_IDENTITY;
                    }
                }
                else if (isint(tpa) && expa && (isconstzero(tpa, expa) || expa->type == en_nullptr))
                {
                    seq[(*n)++] = CV_POINTERCONVERSION;
                }
                else
                {
                    seq[(*n)++] = CV_NONE;            
                }
            }
            else if (isint(tpa) && expa && (isconstzero(tpa, expa) || expa->type == en_nullptr))
            {
                seq[(*n)++] = CV_POINTERCONVERSION;
            }
            else
            {
                seq[(*n)++] = CV_NONE;            
            }
        }
        else if (basetype(tpa)->type == bt_memberptr)
        {
             seq[(*n)++] = CV_NONE;            
        }
        else if (basetype(tpa)->type == bt_enum)
        {
            if (basetype(tpp)->type == bt_enum)
            {
                if (basetype(tpa)->sp != basetype(tpp)->sp)
                    seq[(*n)++] = CV_NONE;
            }
            else
            { 
                if ((basetype(tpp)->type == bt_int || basetype(tpp)->type == bt_unsigned) && !basetype(tpa)->scoped)
                {
                    seq[(*n)++] = CV_INTEGRALPROMOTION;
                }
                else
                {
                    seq[(*n)++] = CV_NONE;
                }
            }
        }
        else if (basetype(tpp)->type == bt_enum)
        {
            if (basetype(tpa)->type == bt_int || basetype(tpa)->type == bt_unsigned)
            {
                if (basetype(tpp)->btp->type != basetype(tpa)->type)
                {
                    seq[(*n)++] = CV_INTEGRALPROMOTION;
                }
            }
            else
            {
                seq[(*n)++] = CV_NONE;                
            }
        }
        else
        {
            if (basetype(tpp)->type != basetype(tpa)->type)
            {
                if (isint(tpa))
                    if ((basetype(tpp)->type == bt_int || basetype(tpp)->type == bt_unsigned) && basetype(tpa)->type < basetype(tpp)->type)
                        seq[(*n)++]= CV_INTEGRALPROMOTION;
                    else if (isint(tpp))
                        seq[(*n)++]= CV_INTEGRALCONVERSION;
                    else
                        seq[(*n)++]= CV_FLOATINGINTEGRALCONVERSION;
                        
                else /* floating */
                    if (basetype(tpp)->type == bt_double && basetype(tpa)->type == bt_float)
                        seq[(*n)++]= CV_FLOATINGPROMOTION;
                    else if (isfloat(tpp))
                        seq[(*n)++]= CV_FLOATINGCONVERSION;
                    else
                        seq[(*n)++]= CV_FLOATINGINTEGRALCONVERSION;
            }
            else
            {
                seq[(*n)++] = CV_IDENTITY;
            }
        }
    }
}
static void getInitListConversion(TYPE *tp, INITLIST *list, TYPE *tpp, int *n, enum e_cvsrn *seq, SYMBOL **userFunc)
{
    INITLIST *a = list->nested;
    if (isstructured(tp) || isref(tp) && isstructured(basetype(tp)->btp))
    {
        if (isref(tp))
            tp = basetype(basetype(tp)->btp);
        if (tp->sp->trivialCons)
        {
            HASHREC *structSyms = tp->syms->table[0];
            while (a && structSyms)
            {
                SYMBOL *member = (SYMBOL *)structSyms->p;
                if (ismemberdata(member))
                {
                    getSingleConversion(member->tp, a->tp, a->exp, n, seq, userFunc); 
                    if (*n > 4)
                        break;
                    a = a->next;
                }
                structSyms = structSyms->next;
            }
            if (a)
            {
                seq[(*n)++] = CV_NONE;
            }
        }
        else
        {
            SYMBOL *cons = search(overloadNameTab[CI_CONSTRUCTOR], tp->syms);
            if (!cons)
            {
                // should never happen
                seq[(*n)++] = CV_NONE;
            }            
            else
            {
                EXPRESSION exp,*expp = &exp;
                TYPE *ctype = cons->tp;
                TYPE thistp;
                FUNCTIONCALL funcparams;
                memset(&exp, 0, sizeof(exp));
                memset(&funcparams, 0, sizeof(funcparams));
                memset(&thistp, 0, sizeof(thistp));
                funcparams.arguments = a;
                exp.type = en_c_i;
                thistp.type = bt_pointer;
                thistp.btp = tp;
                thistp.size = getSize(bt_pointer);
                funcparams.thistp = &thistp;
                funcparams.thisptr = &exp;
                funcparams.ascall = TRUE;
                cons = GetOverloadedFunction(&ctype, &expp, cons, & funcparams, NULL, FALSE, TRUE);
                if (!cons)    
                {
                    seq[(*n)++] = CV_NONE;
                }
            }
        }     
    }
    else if (ispointer(tp))
    {
        TYPE *btp = tp;
        int x;
        while (isarray(btp))
            btp = basetype(btp)->btp;
        x = tp->size / btp->size;
        while (a)
        {
            getSingleConversion(btp, a->tp, a->exp, n, seq, userFunc);                
            if (*n > 4)
                break;
            if (--x < 0) // too many items...
            {
                seq[(*n)++] = CV_NONE;
                break;
            }
            a = a->next;
        }
    }
    else
    {
        a = list->nested;
        if (a && a->next)
            seq[(*n)++] = CV_NONE;
        else if (a)
            getSingleConversion(tp, a ? a->tp : tpp, a ? a->exp : NULL, n, seq, userFunc);                
    }
}
static BOOL getFuncConversions(SYMBOL *sp, FUNCTIONCALL *f, TYPE *atp, SYMBOL *parent, enum e_cvsrn arr[], int *sizes, int count, SYMBOL **userFunc)
{
    int pos = 0;
    int n = 0;
    int i;
    INITLIST *a = NULL;
    HASHREC **hr = basetype(sp->tp)->syms->table;
    HASHREC **hrt = NULL;
    enum e_cvsrn seq[100], cur;
    int m = 0,m1;
    if (f)
        a = f->arguments;
    else
        hrt = atp->syms->table;
    for (i=0; i < count; i++)
    {
        arr[i] = CV_PAD;
    }
    /* takes care of THIS pointer */
    if (sp->castoperator)
    {
        TYPE tpx;
        TYPE *tpp;
        SYMBOL *argsym = (SYMBOL *)(*hr)->p;
        m = 0;
        getSingleConversion(parent->tp, basetype(sp->tp)->btp, NULL,
                                    &m, seq, userFunc ? &userFunc[n] : NULL);
        m1 = m;
        while (m1 && seq[m1-1] == CV_IDENTITY)
            m1--;
        if (m1 > 4)
        {
            return FALSE;
        }
        for (i=0; i < m; i++)
            if (seq[i] == CV_NONE)
                return FALSE;
        memcpy(arr+pos, seq, 3 * sizeof(enum e_cvsrn));
        sizes[n++] = m;
        pos += m;
        hr = &(*hr)->next;
        tpp = argsym->tp;
        tpx.type = bt_pointer;
        tpx.size = getSize(bt_pointer);
        tpx.btp = f->arguments->tp;
        m = 0;
        getSingleConversion(tpp, &tpx, f->thisptr, &m, seq, userFunc ? &userFunc[n] : NULL);
        m1 = m;
        while (m1 && seq[m1-1] == CV_IDENTITY)
            m1--;
        if (m1 > 4)
        {
            return FALSE;
        }
        for (i=0; i < m; i++)
            if (seq[i] == CV_NONE)
                return FALSE;
        memcpy(arr+pos, seq, 3 * sizeof(enum e_cvsrn));
        sizes[n++] = m;
        pos += m;
        return TRUE;
    }
    else
    {
        if (f)
        {
            if ((f->thistp || a && a->tp) && ismember(sp))
            {
                // nonstatic function
                TYPE *argtp = sp->tp;
                if (!argtp)
                {
                    arr[n++] = CV_NONE;
                    return FALSE;
                }
                else
                {
                    TYPE tpx, tpy;
                    TYPE *tpp;
                    TYPE *tpthis = f->thistp;
                    SYMBOL *argsym = (SYMBOL *)(*hr)->p;
                    hr = &(*hr)->next;
                    tpp = argsym->tp;
                    if (!tpthis)
                    {
                        tpthis = a ? a->tp : NULL;
                        if (a)
                            a = a->next;
                    }
                    if (sp->castoperator || tpthis && f->thistp == NULL)
                    {
                        tpthis = &tpx;
                        tpx.type = bt_pointer;
                        tpx.size = getSize(bt_pointer);
                        tpx.btp = f->arguments->tp;
                    }
                    m = 0;
                    getSingleConversion(tpp, tpthis, f->thisptr, &m, seq, userFunc ? &userFunc[n] : NULL);
                    m1 = m;
                    while (m1 && seq[m1-1] == CV_IDENTITY)
                        m1--;
                    if (m1 > 4)
                    {
                        return FALSE;
                    }
                    for (i=0; i < m; i++)
                        if (seq[i] == CV_NONE)
                            return FALSE;
                    memcpy(arr+pos, seq, 3 * sizeof(enum e_cvsrn));
                    sizes[n++] = m;
                    pos += m;
                }
            }
        }
        else
        {
            if (ismember(sp))
            {
                TYPE *argtp = sp->parentClass->tp;
                if (!argtp)
                {
                    return FALSE;
                }
                else 
                {
                    getSingleConversion(argtp, a ? a->tp : ((SYMBOL *)(*hrt)->p)->tp, a ? a->exp : NULL, 
                                        &m, seq, userFunc ? &userFunc[n] : NULL);
                    if (a)
                        a = a->next;
                    else if (hrt)
                        hrt = &(*hrt)->next;
                }
            }
        }
        while (*hr && (a || hrt && *hrt))
        {
            SYMBOL *argsym = (SYMBOL *)(*hr)->p;
            if (argsym->tp->type != bt_any)
            {
                
                if (argsym->constop)
                    break;
                if (argsym->storage_class != sc_parameter)
                    return FALSE;
                if (argsym->tp->type == bt_ellipse)
                {
                    arr[n++] = CV_ELLIPSIS;
                    return TRUE;
                }
                m = 0;
                if (a && !a->tp)
                {
                    getInitListConversion(basetype(argsym->tp), a, a ? NULL : ((SYMBOL *)(*hrt)->p)->tp, &m, seq, userFunc ? &userFunc[n] : NULL);
                }
                else
                {
                    getSingleConversion(basetype(argsym->tp), a ? a->tp : ((SYMBOL *)(*hrt)->p)->tp, a ? a->exp : NULL, &m, seq,
                                    userFunc ? &userFunc[n] : NULL);
                }
                m1 = m;
                while (m1 && seq[m1-1] == CV_IDENTITY)
                    m1--;
                if (m1 > 4)
                {
                    return FALSE;
                }
                for (i=0; i < m; i++)
                    if (seq[i] == CV_NONE)
                        return FALSE;
                memcpy(arr+pos, seq, 3 * sizeof(enum e_cvsrn));
                sizes[n++] = m;
                pos += m;
            }
            hr = &(*hr)->next;
            if (a)
                a = a->next;
            else
                hrt = &(*hrt)->next;
        }
        if (*hr)
        {
            SYMBOL *sym = (SYMBOL *)(*hr)->p;
            if (sym->init || sym->deferredCompile)
            {
                return TRUE;
            }
            if (sym->tp->type == bt_ellipse)
            {
                sizes[n++] = 1;
                arr[pos++] = CV_ELLIPSIS;
                return TRUE;
            }
            if (sym->tp->type == bt_void || sym->tp->type == bt_any)
                return TRUE;
            return FALSE;
        }
        return a == NULL;
    }
}
static SYMBOL *detemplate(SYMBOL *sym, FUNCTIONCALL *args, TYPE *atp)
{
    if (sym->templateLevel)
    {
        if (atp || args)
        {
            if (!TemplateIntroduceArgs(sym->templateParams, args->templateParams))
                sym = NULL;
            else if (atp)
                sym = TemplateDeduceArgsFromType(sym, atp);
            else if (args->ascall)
                sym = TemplateDeduceArgsFromArgs(sym, args);
            else
                sym = TemplateDeduceWithoutArgs(sym);
        }
        else
        {
            sym = NULL;
        }
    }
    return sym;
}
static void WeedTemplates(SYMBOL **table, int count, FUNCTIONCALL *args, TYPE *atp)
{
    int i = count;
    if (atp || !args->astemplate)
    {
        for (i=0; i < count ;i++)
            if (table[i] && !table[i]->templateLevel)
                break;
    }
    else
    {
        for (i=0; i < count ;i++)
            if (table[i] && !table[i]->templateLevel)
                table[i] = NULL;
    }
    if (i < count)
    {
        // one or more first class citizens, don't match templates
        for (i=0; i < count ;i++)
            if (table[i] && table[i]->templateLevel)
                table[i] = NULL;
    }
    else
    {
        TemplatePartialOrdering(table, count, args, atp, FALSE);
    }
}
SYMBOL *GetOverloadedTemplate(SYMBOL *sp, FUNCTIONCALL *args)
{
    int n = 0, i;
    SYMBOL *found1 = NULL, *found2 = NULL;
    SYMBOL **spList;
    HASHREC **hr = &sp->tp->syms->table[0];
    while (*hr)
    {
        SYMBOL *sym = (SYMBOL *)(*hr)->p;
        if (sym->templateLevel && !sym->instantiated)
            n++;
        hr = &(*hr)->next;
    }
    spList = Alloc(sizeof(SYMBOL *) * n);
    hr = &sp->tp->syms->table[0];
    n = 0;
    while (*hr)
    {
        SYMBOL *sym = (SYMBOL *)(*hr)->p;
        if (sym->templateLevel && !sym->instantiated)
            spList[n++] = detemplate(sym, args, NULL);
        hr = &(*hr)->next;
    }
    WeedTemplates(spList, n, args, NULL);
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
        if (!templateNestingCount)
            errorsym(ERR_NO_TEMPLATE_MATCHES, sp);
        return NULL;
    }
    return found1;
}
SYMBOL *GetOverloadedFunction(TYPE **tp, EXPRESSION **exp, SYMBOL *sp, 
                              FUNCTIONCALL *args, TYPE *atp, BOOL toErr, 
                              BOOL maybeConversion)
{
    if (atp && ispointer(atp))
        atp = basetype(atp)->btp;
    if (atp && !isfunction(atp))
        atp = NULL;
    if (!sp || sp->storage_class == sc_overloads)
    {
        LIST *gather = NULL;
        SYMBOL **flatList;    
        SYMBOL *found1 = NULL, *found2 = NULL;
        if (!cparams.prm_cplusplus)
        {
            sp = ((SYMBOL *)sp->tp->syms->table[0]->p);
            if (sp)
            {
                *exp = varNode(en_pc, sp);
                *tp = sp->tp;
            }
            return sp;
        }
        if (sp)
        {
            if (args || atp)
            {
                if (!sp->tp || !sp->wasUsing && !sp->parentClass)
                {
                        // ok the sym is a valid candidate for argument search
                    if (args)
                    {
                        INITLIST *list = args->arguments;
                        while (list)
                        {
                            if (list->tp)
                                gather = searchOneArg(sp, gather, list->tp);
                            list = list->next;
                        }
                        if (args->thisptr)
                            gather = searchOneArg(sp, gather, args->thistp);
                    }
                    else
                    {
                        HASHREC **hr = atp->syms->table;
                        while (*hr)
                        {
                            SYMBOL *sym = (SYMBOL *)(*hr)->p;
                            if (sym->storage_class != sc_parameter)
                                break;
                            gather = searchOneArg(sp, gather, sym->tp);
                            hr = &(*hr)->next;
                        }
                    }
                }
                weedToFunctions(&gather);
            }
            if (sp->tp)
            {
                LIST *lst = Alloc(sizeof(LIST));
                lst->data = sp;
                lst->next = gather;
                gather = lst;
            }
        }
        // ok got the initial list, time for phase 2
        // which is to add any other functions that have to be added...
        // constructors, member operator '()' and so forth...
        if (gather)
        {
            // we are only doing global functions for now...
            // so nothing here...
            
        }
        if (maybeConversion)
        {
            if (args->arguments && !args->arguments->next && !args->arguments->nested) // one arg
                gather = GetMemberCasts(gather, basetype(args->arguments->tp)->sp);
        }
        // pass 3 - the actual argument-based resolution
        if (gather)
        {
            LIST *lst2;
            int n = 0;
            lst2 = gather;
            while (lst2)
            {
                HASHREC **hr = ((SYMBOL *)lst2->data)->tp->syms->table;
                while (*hr)
                {
                    SYMBOL *sym = (SYMBOL *)(*hr)->p;
                    if (!args || !args->astemplate || sym->templateLevel && !sym->instantiated)
                        n++;
                    hr = &(*hr)->next;
                }
                lst2 = lst2->next;
            }
            if (args || atp)
            {
                int i;
                SYMBOL **spList;
                SYMBOL ***funcList;
                enum e_cvsrn **icsList;
                int **lenList;
                BOOL done = FALSE;
                int argCount = 0;
                if (args)
                {
                    INITLIST *v = args->arguments;
                    while (v)
                    {
                        argCount++;
                        v = v->next;
                    }
                    if (args->thisptr)
                        argCount++;
                }
                else
                {
                    HASHREC **hr = atp->syms->table;
                    while (*hr && ((SYMBOL *)(*hr)->p)->storage_class == sc_parameter)
                    {
                        argCount++;
                        hr = &(*hr)->next;
                    }
                    if (*hr && ismember(((SYMBOL *)(*hr)->p)))
                        argCount++;
                }
                spList = (SYMBOL **)Alloc(sizeof(SYMBOL *) * n);
                icsList = (enum e_cvsrn **)Alloc(sizeof(enum e_cvsrn *) * n);
                lenList = (int **)Alloc(sizeof(int *) * n);
                funcList = (int **)Alloc(sizeof(SYMBOL **) * n);
                lst2 = gather;
                n = 0;
                while (lst2)
                {
                    HASHREC **hr = ((SYMBOL *)lst2->data)->tp->syms->table;
                    while (*hr)
                    {
                        SYMBOL *sym = (SYMBOL *)(*hr)->p;
                        if ((!args || !args->astemplate || sym->templateLevel && !sym->instantiated))
                        {
                            if (sym->templateLevel)
                            {
                                if (!sym->instantiated)
                                    spList[n++] = detemplate(sym, args, atp) ;
                            }
                            else
                            {
                                spList[n++] = sym;
                            }
                        }
                        hr = &(*hr)->next;
                    }
                    lst2 = lst2->next;
                }
                if (atp || args->ascall)
                {
                    GatherConversions(sp, spList, n, args, atp, icsList, lenList, argCount, funcList);
                    SelectBestFunc(spList, icsList, lenList, args, argCount, n, funcList);
                }
                WeedTemplates(spList, n, args, atp);
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
            }
            else
            {
                HASHREC **hr = (HASHREC **)((SYMBOL *)gather->data)->tp->syms->table;
                found1 = (SYMBOL *)(*hr)->p;
                if (n > 1)
                    found2 = (SYMBOL *)(*(HASHREC **)(*hr))->p;
            }
        }
        // any errors
        if (toErr)
        {
            if (!found1)
            {
                if (args && args->arguments && !args->arguments->next // one arg
                    && sp && sp->isConstructor) // conversion constructor
                {
                    errortype(ERR_CANNOT_CONVERT_TYPE, args->arguments->tp, sp->parentClass->tp);
                }
                else if (!sp)
                {
                    errorstr(ERR_NO_OVERLOAD_MATCH_FOUND, "unknown");
                }
                else
                {
                    SYMBOL *sym = Alloc(sizeof(SYMBOL));
                    sym->parentClass = sp->parentClass;
                    sym->name = sp->name;
                    if (atp)
                    {
                        sym->tp = atp;
                    }
                    else
                    {
                        int v = 1;
                        INITLIST *a = args->arguments;
                        sym->tp = Alloc(sizeof(TYPE));
                        sym->tp->type = bt_func;
                        sym->tp->size = getSize(bt_pointer);
                        sym->tp->btp = &stdint;
                        sym->tp->syms = CreateHashTable(1);
                        while (a)
                        {
                            SYMBOL *sym1 = Alloc(sizeof(SYMBOL));
                            char nn[10];
                            sprintf(nn, "%d", v++);
                            sym1->name = litlate(nn);
                            sym1->tp = a->tp;
                            insert(sym1, sym->tp->syms);
                            a = a->next;
                        }
                    }
                    SetLinkerNames(sym, lk_cpp);
    
                    errorsym(ERR_NO_OVERLOAD_MATCH_FOUND, sym);
                }
            }
            else if (found1 && found2)
            {
                errorsym2(ERR_AMBIGUITY_BETWEEN, found1, found2);
            }
            else if (found1->deleted)
            {
                errorsym(ERR_DELETED_FUNCTION_REFERENCED, found1);
            }
            if (found1)
            {
                if (found1->templateLevel)
                    found1 = TemplateFunctionInstantiate(found1, FALSE, FALSE);
            }
        }
        if (!toErr && found2)
        {
            sp = NULL;
        }
        else
        {
            sp = found1;
            if (sp)
            {
                *exp = varNode(en_pc, sp);
                *tp = sp->tp;
            }
        }
    }
    return sp;
}
