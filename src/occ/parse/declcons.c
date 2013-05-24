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
extern ARCH_ASM *chosenAssembler;

extern char *overloadNameTab[];
extern int errorline;
extern char *errorfile;
extern NAMESPACEVALUES *globalNameSpace, *localNameSpace;
extern TYPE stdpointer, stdint, stdvoid;

static void genAsnCall(BLOCKDATA *b, SYMBOL *cls, SYMBOL *base, int offset, EXPRESSION *thisptr, EXPRESSION *other, BOOL move, BOOL isconst);
static void createDestructor(SYMBOL *sp);

void ConsDestDeclarationErrors(SYMBOL *sp, BOOL notype)
{
    if (sp->name == overloadNameTab[CI_CONSTRUCTOR])
    {
        if (!notype)
            error(ERR_CONSTRUCTOR_OR_DESTRUCTOR_NO_TYPE);
        else if (sp->storage_class == sc_virtual)
            errorstr(ERR_INVALID_STORAGE_CLASS, "virtual");
        else if (sp->storage_class == sc_static)
            errorstr(ERR_INVALID_STORAGE_CLASS, "static");
        else if (isconst(sp->tp) || isvolatile(sp->tp))
            error(ERR_CONSTRUCTOR_OR_DESTRUCTOR_NO_CONST_VOLATILE);
    }
    else if (sp->name == overloadNameTab[CI_DESTRUCTOR])
    {
        if (!notype)
            error(ERR_CONSTRUCTOR_OR_DESTRUCTOR_NO_TYPE);
        else if (sp->storage_class == sc_static)
            errorstr(ERR_INVALID_STORAGE_CLASS, "static");
        else if (isconst(sp->tp) || isvolatile(sp->tp))
            error(ERR_CONSTRUCTOR_OR_DESTRUCTOR_NO_CONST_VOLATILE);
    }
}
MEMBERINITIALIZERS *GetMemberInitializers(LEXEME *lex, SYMBOL *sym)
{
    MEMBERINITIALIZERS *first = NULL, **cur = &first ;
    if (sym->name != overloadNameTab[CI_CONSTRUCTOR])
        error(ERR_INITIALIZER_LIST_REQUIRES_CONSTRUCTOR);
    while (lex != NULL)
    {
        if (ISID(lex))
        {
            *cur = Alloc(sizeof(MEMBERINITIALIZERS));
            (*cur)->name = litlate(lex->value.s.a);
            (*cur)->line = lex->line;
            (*cur)->file = lex->file;
            lex = getsym();
            if (MATCHKW(lex, openpa))
            {
                int paren = 0;
                LEXEME **mylex = &(*cur)->initData;
                **mylex = *lex;
                mylex = &(*mylex)->next;
                lex = getsym();
                while (lex && (!MATCHKW(lex, closepa) || paren))
                {
                    if (MATCHKW(lex, openpa))
                        paren++;
                    if (MATCHKW(lex, closepa))
                        paren--;
                    if (lex->type == l_id)
                        lex->value.s.a = litlate(lex->value.s.a);
                    **mylex = *lex;
                    mylex = &(*mylex)->next;
                    lex = getsym();
                }
                if (MATCHKW(lex, closepa))
                {
                    **mylex = *lex;
                    mylex = &(*mylex)->next;
                    lex = getsym();
                }
            }
            else
            {
                error(ERR_MEMBER_INITIALIZATION_REQUIRED);
                skip(&lex, openbr);
                break;
            }
            cur = &(*cur)->next;            
        }
        else
        {
            error(ERR_MEMBER_NAME_REQUIRED);
        }
        if (!MATCHKW(lex, comma))
            break;
        lex = getsym();
    }
    return first;
}
static SYMBOL *insertFunc(SYMBOL *sp, SYMBOL *ovl)
{
    SYMBOL *funcs = search(ovl->name, sp->tp->syms);
    ovl->parent = sp;
    ovl->internallyGenned = TRUE;
    ovl->linkage = lk_inline;
    ovl->defaulted = TRUE;
    ovl->access = ovl->accessspecified = ac_public;
    if (!funcs)
    {
        TYPE *tp = (TYPE *)Alloc(sizeof(TYPE));
        tp->type = bt_aggregate;
        funcs = makeID(sc_overloads, tp, 0, ovl->name) ;
        tp->sp = funcs;
        SetLinkerNames(funcs, lk_cdecl);
        insert(funcs, sp->tp->syms);
        funcs->parent = sp;
        funcs->tp->syms = CreateHashTable(1);
        insert(ovl, funcs->tp->syms);
    }
    else if (funcs->storage_class == sc_overloads)
    {
        insertOverload(ovl, funcs->tp->syms);
    }
    else 
    {
        diag("insertFunc: invalid overload tab");
    }
    return ovl;
}
static SYMBOL *declareDestructor(SYMBOL *sp)
{
    SYMBOL *func, *sp1;
    TYPE *tp = (TYPE *)Alloc(sizeof(TYPE));
    TYPE *tp1;
    tp->type = bt_func;
    tp->btp = (TYPE *)Alloc(sizeof(TYPE));
    tp->btp->type = bt_void;
    func = makeID(sc_member, tp, NULL, overloadNameTab[CI_DESTRUCTOR]);
    func->decoratedName = func->errname = func->name;
    sp1= makeID(sc_parameter, NULL, NULL, AnonymousName());
    tp->syms = CreateHashTable(1);        
    tp->syms->table[0] = (HASHREC *)Alloc(sizeof(HASHREC));
    tp->syms->table[0]->p = sp1;
    sp1->tp = (TYPE *)Alloc(sizeof(TYPE));
    sp1->tp->type = bt_void;
    return insertFunc(sp, func);
}
static BOOL hasConstFuncs(SYMBOL *sp, int type)
{
    TYPE *tp = NULL;
    EXPRESSION *exp = NULL;
    SYMBOL *ovl = search(overloadNameTab[type], sp->tp->syms);
    FUNCTIONCALL *params = (FUNCTIONCALL *)Alloc(sizeof(FUNCTIONCALL));
    params->arguments = (ARGLIST *)Alloc(sizeof(ARGLIST));
    params->arguments->tp = (TYPE *)Alloc(sizeof(TYPE));
    params->arguments->tp->type = bt_const;
    params->arguments->tp->btp = (TYPE *)Alloc(sizeof(TYPE));
    params->arguments->tp->btp->type = bt_rref;
    params->arguments->tp->btp->btp = sp->tp;
    params->arguments->exp = params->arguments->rootexp = intNode(en_c_i, 0);
    return !!GetOverloadedFunction(&tp, &exp, ovl, &params, NULL, FALSE);
}
static BOOL constCopyConstructor(SYMBOL *sp)
{
    VBASEENTRY *e;
    BASECLASS *b;
    HASHREC *hr;
    b = sp->baseClasses;
    while (b)
    {
        if (!b->isvirtual && !hasConstFuncs(b->cls, CI_CONSTRUCTOR))
            return FALSE;
        b = b->next;
    }
    e = sp->vbaseEntries;
    while (e)
    {
        if (!hasConstFuncs(e->cls, CI_CONSTRUCTOR))
            return FALSE;
        e = e->next;
    }
    hr = sp->tp->syms->table[0];
    while (hr)
    {
        SYMBOL *cls = (SYMBOL *)hr->p;
        if (isstructured(cls->tp) && !cls->trivialCons)
            if (!hasConstFuncs(cls, CI_CONSTRUCTOR))
                return FALSE;
        hr = hr->next;
    }
    
    return TRUE;
}
static SYMBOL *declareConstructor(SYMBOL *sp, BOOL deflt, BOOL move)
{
    SYMBOL *func, *sp1;
    TYPE *tp = (TYPE *)Alloc(sizeof(TYPE));
    TYPE *tp1;
    tp->type = bt_func;
    tp->btp = (TYPE *)Alloc(sizeof(TYPE));
    tp->btp->type = bt_void;
    func = makeID(sc_member, tp, NULL, overloadNameTab[CI_CONSTRUCTOR]);
    func->decoratedName = func->errname = func->name;
    sp1= makeID(sc_parameter, NULL, NULL, AnonymousName());
    tp->syms = CreateHashTable(1);        
    tp->syms->table[0] = (HASHREC *)Alloc(sizeof(HASHREC));
    tp->syms->table[0]->p = sp1;
    sp1->tp = (TYPE *)Alloc(sizeof(TYPE));
    if (deflt)
    {
        sp1->tp->type = bt_void;
    }
    else
    {
        TYPE *tpx = sp1->tp;
        if (constCopyConstructor(sp))
        {
            tpx->type = bt_const;
            tpx->size = getSize(bt_pointer);
            tpx->btp = (TYPE *)Alloc(sizeof(TYPE));
            tpx = tpx->btp;
        }
        tpx->type = move ? bt_rref : bt_lref;
        tpx->btp = (TYPE *)Alloc(sizeof(TYPE));
        tpx = tpx->btp;
        tpx->type = basetype(sp->tp)->type;
        
    }
    return insertFunc(sp, func);
}
static BOOL constAssignmentOp(SYMBOL *sp)
{
    VBASEENTRY *e;
    BASECLASS *b;
    HASHREC *hr;
    b = sp->baseClasses;
    while (b)
    {
        if (!b->isvirtual && !hasConstFuncs(b->cls, assign - kw_new + CI_NEW))
            return FALSE;
        b = b->next;
    }
    e = sp->vbaseEntries;
    while (e)
    {
        if (!hasConstFuncs(e->cls, assign - kw_new + CI_NEW))
            return FALSE;
        e = e->next;
    }
    hr = sp->tp->syms->table[0];
    while (hr)
    {
        SYMBOL *cls = (SYMBOL *)hr->p;
        if (isstructured(cls->tp) && !cls->trivialCons)
            if (!hasConstFuncs(cls, assign - kw_new + CI_NEW))
                return FALSE;
        hr = hr->next;
    }
    
    return TRUE;
}
static SYMBOL *declareAssignmentOp(SYMBOL *sp, BOOL move)
{
    SYMBOL *func, *sp1;
    TYPE *tp = (TYPE *)Alloc(sizeof(TYPE));
    TYPE *tp1, *tpx;
    tp->type = bt_func;
    tp->btp = (TYPE *)Alloc(sizeof(TYPE));
    tp->btp->type = bt_void;
    func = makeID(sc_member, tp, NULL, overloadNameTab[assign - kw_new + CI_NEW]);
    func->decoratedName = func->errname = func->name;
    sp1= makeID(sc_parameter, NULL, NULL, AnonymousName());
    tp->syms = CreateHashTable(1);
    tp->syms->table[0] = (HASHREC *)Alloc(sizeof(HASHREC));
    tp->syms->table[0]->p = sp1;
    sp1->tp = (TYPE *)Alloc(sizeof(TYPE));
    tpx = sp1->tp;
    if (constAssignmentOp(sp))
    {
        tpx->type = bt_const;
        tpx->size = getSize(bt_pointer);
        tpx->btp = (TYPE *)Alloc(sizeof(TYPE));
        tpx = tpx->btp;
    }
    tpx->type = move ? bt_rref : bt_lref;
    tpx->btp = (TYPE *)Alloc(sizeof(TYPE));
    tpx = tpx->btp;
    tpx->type = basetype(sp->tp)->type;
    return insertFunc(sp, func);
}
static BOOL matchesDefaultConstructor(SYMBOL *sp)
{
    SYMBOL *arg1 = (SYMBOL *)sp->tp->syms->table[0]->p;
    if (arg1->tp->type == bt_void || arg1->init)
        return TRUE;
    return FALSE;
}
static BOOL matchesCopy(SYMBOL *sp, BOOL move)
{
    HASHREC *hr = sp->tp->syms->table[0];
    SYMBOL *arg1 = (SYMBOL *)hr->p;
    SYMBOL *arg2 = (SYMBOL *)hr->next->p;
    if (!arg2 || arg2->init)
    {
        if (arg1->tp->type == (move ? bt_rref : bt_lref))
        {
            TYPE *tp  = basetype(arg1->tp)->btp;
            if (isstructured(tp))
                if (basetype(tp)->sp == sp)
                    return TRUE;
        }
    }
    return FALSE;
}
static BOOL hasCopy(SYMBOL *func, BOOL move)
{
    HASHREC *hr = func->tp->syms->table[0];
    while (hr)
    {
        SYMBOL *sp = (SYMBOL *)hr->p;
        if (!sp->internallyGenned && matchesCopy(sp, move))
            return TRUE;
        hr = hr->next;
    }
    return FALSE;
}
static BOOL checkDest(SYMBOL *sp, HASHTABLE *syms)
{
    SYMBOL *dest = search(overloadNameTab[CI_DESTRUCTOR], syms);
    
    dest = (SYMBOL *)dest->tp->syms->table[0]->p;
    if (dest->deleted)
        return TRUE;
    if (!isAccessible(sp,sp, dest, NULL, ac_protected, FALSE))
        return TRUE;
    return FALSE;
}
static BOOL checkDefaultCons(SYMBOL *sp, HASHTABLE *syms)
{
    SYMBOL *cons = search(overloadNameTab[CI_DESTRUCTOR], syms);
    SYMBOL *dflt = NULL;
    HASHREC *hr = cons->tp->syms->table[0];
    while (hr)
    {
        SYMBOL *cur = (SYMBOL *)hr->p;
        if (matchesDefaultConstructor(cur))
        {
            if (dflt)
                return TRUE; // ambiguity
            dflt = cur;
        }
        hr = hr->next;
    }
    if (dflt)
    {
        if (dflt->deleted)
            return TRUE;
        if (!isAccessible(sp,sp, dflt, NULL, ac_protected, FALSE))
            return TRUE;
    }
    return FALSE;
}
static SYMBOL *getCopyCons(SYMBOL *base, BOOL move)
{
    SYMBOL *ovl = search(overloadNameTab[CI_CONSTRUCTOR], base->tp->syms);
    FUNCTIONCALL funcparams;
    ARGLIST arg;
    EXPRESSION exp;
    TYPE tp;
    TYPE *tpx = NULL;
    EXPRESSION *epx = NULL;
    memset(&funcparams, 0, sizeof(funcparams));
    memset(&arg, 0, sizeof(exp));
    memset(&exp, 0, sizeof(exp));
    exp.type = en_auto;
    exp.v.sp = base;
    tp.type = move ? bt_rref : bt_lref;
    tp.btp = base->tp;
    tp.size = getSize(bt_pointer);
    arg.exp = arg.rootexp = &exp;
    arg.tp = &tp;
    funcparams.arguments = &arg;
    return GetOverloadedFunction(&tpx, &epx, ovl, &funcparams, NULL, FALSE);
}
static SYMBOL *GetCopyAssign(SYMBOL *base, BOOL move)
{
    SYMBOL *ovl = search(overloadNameTab[assign - kw_new + CI_NEW ], base->tp->syms);
    FUNCTIONCALL funcparams;
    ARGLIST arg;
    EXPRESSION exp;
    TYPE tp;
    TYPE *tpx = NULL;
    EXPRESSION *epx = NULL;
    memset(&funcparams, 0, sizeof(funcparams));
    memset(&arg, 0, sizeof(exp));
    memset(&exp, 0, sizeof(exp));
    exp.type = en_auto;
    exp.v.sp = base;
    tp.type = move ? bt_rref : bt_lref;
    tp.btp = base->tp;
    tp.size = getSize(bt_pointer);
    arg.exp = arg.rootexp = &exp;
    arg.tp = &tp;
    funcparams.arguments = &arg;
    return GetOverloadedFunction(&tpx, &epx, ovl, &funcparams, NULL, FALSE);
}
static BOOL hasTrivialCopy(SYMBOL *sp, BOOL move)
{
    HASHREC *hr;
    SYMBOL *dflt;
    BASECLASS * base;
    if (sp->vbaseEntries || sp->vtabEntries)
        return FALSE;
    base = sp->baseClasses;
    while (base)
    {
        dflt = getCopyCons(base->cls, move);
        if (!dflt)
            return FALSE;
        if (!dflt->trivialCons)
            return FALSE;
        base = base->next;
    }
    hr = sp->tp->syms->table[0];
    while (hr)
    {
        SYMBOL *cls = (SYMBOL *)hr->p;
        dflt = getCopyCons(cls, move);
        if (!dflt)
            return FALSE;
        if (!dflt->trivialCons)
            return FALSE;
        hr = hr->next;
    }
    return TRUE;
}
static BOOL hasTrivialAssign(SYMBOL *sp, BOOL move)
{
    HASHREC *hr;
    SYMBOL *dflt;
    BASECLASS * base;
    if (sp->vbaseEntries || sp->vtabEntries)
        return FALSE;
    base = sp->baseClasses;
    while (base)
    {
        dflt = GetCopyAssign(base->cls, move);
        if (!dflt)
            return FALSE;
        if (!dflt->trivialCons)
            return FALSE;
        base = base->next;
    }
    hr = sp->tp->syms->table[0];
    while (hr)
    {
        SYMBOL *cls = (SYMBOL *)hr->p;
        dflt = getCopyCons(cls, move);
        if (!dflt)
            return FALSE;
        if (!dflt->trivialCons)
            return FALSE;
        hr = hr->next;
    }
    return TRUE;
}
static BOOL checkCopyCons(SYMBOL *sp, SYMBOL *base)
{
    SYMBOL *dflt = getCopyCons(base, FALSE);
    if (dflt)
    {
        if (dflt->deleted)
            return TRUE;
        if (!isAccessible(sp,sp, dflt, NULL, ac_protected, FALSE))
            return TRUE;
    }
    return FALSE;
}
static BOOL checkCopyAssign(SYMBOL *sp, SYMBOL *base)
{
    SYMBOL *dflt = GetCopyAssign(base, FALSE);
    if (dflt)
    {
        if (dflt->deleted)
            return TRUE;
        if (!isAccessible(sp,sp, dflt, NULL, ac_protected, FALSE))
            return TRUE;
    }
    return FALSE;
}
static BOOL checkMoveCons(SYMBOL *sp, SYMBOL *base)
{
    SYMBOL *dflt = getCopyCons(base, TRUE);
    if (dflt)
    {
        if (dflt->deleted)
            return TRUE;
        if (!isAccessible(sp,sp, dflt, NULL, ac_protected, FALSE))
            return TRUE;
    }
    return FALSE;
}
static BOOL checkMoveAssign(SYMBOL *sp, SYMBOL *base)
{
    SYMBOL *dflt = GetCopyAssign(base, TRUE);
    if (dflt)
    {
        if (dflt->deleted)
            return TRUE;
        if (!isAccessible(sp,sp, dflt, NULL, ac_protected, FALSE))
            return TRUE;
    }
    else
    {
        if (!hasTrivialAssign(sp, TRUE))
            return TRUE;
    }
    return FALSE;
}
static BOOL isDefaultDeleted(SYMBOL *sp)
{
    HASHREC *hr;
    BASECLASS *base;
    VBASEENTRY *vbase;
    if (basetype(sp->tp)->type == bt_union)
    {
        BOOL allconst = TRUE;
        hr = sp->tp->syms->table[0];
        while (hr)
        {
            SYMBOL *sp = (SYMBOL *)hr->p;
            if (!isconst(sp->tp))
                allconst = FALSE;
            if (isstructured(sp->tp))
            {
                SYMBOL *cons = search(overloadNameTab[CI_CONSTRUCTOR], sp->tp->syms);
                HASHREC *hr1 = cons->tp->syms->table[0];
                while (hr1)
                {
                    cons = (SYMBOL *)hr->p;
                    if (matchesDefaultConstructor(cons))
                        if (!cons->trivialCons)
                            return TRUE;
                    hr1 = hr1->next;
                }
            }
            hr = hr->next;
        }
        if (allconst)
            return TRUE;
    }
    hr = sp->tp->syms->table[0];
    while (hr)
    {
        SYMBOL *sp1 = (SYMBOL *)hr->p;
        TYPE *m;
        if (sp1->storage_class == sc_member)
        {
            if (isref(sp1->tp))
                if (!sp1->init)
                    return TRUE;
            if (basetype(sp1->tp)->type == bt_union)
            {
                HASHREC *hr1 = sp1->tp->syms->table[0];
                while (hr1)
                {
                    SYMBOL *member = (SYMBOL *)hr1->p;
                    if (!isconst(member->tp))
                    {
                        break;
                    }
                    hr1 = hr1->next;
                }
                if (!hr1)
                    return TRUE;
            }
            if (isstructured(sp1->tp))
            {
                if (checkDest(sp1, sp1->tp->syms))
                    return TRUE;
            }
            m = sp1->tp;
            if (isarray(m))
                m = basetype(sp1->tp)->btp;
            if (isstructured(m))
            {
                if (checkDefaultCons(sp, sp1->tp->syms))
                    return TRUE;
            }
        }        
        hr = hr->next;
    }
    
    base = sp->baseClasses;
    while (base)
    {
        if (checkDest(sp, base->cls->tp->syms))
            return TRUE;
        if (checkDefaultCons(sp, base->cls->tp->syms))
            return TRUE;
        base = base->next;
    }
    vbase = sp->vbaseEntries;
    while (vbase)
    {
        if (checkDest(sp, vbase->cls->tp->syms))
            return TRUE;
        if (checkDefaultCons(sp, vbase->cls->tp->syms))
            return TRUE;
        vbase = vbase->next;
    }
    return FALSE;
}
static BOOL isCopyConstructorDeleted(SYMBOL *sp)
{
    HASHREC *hr;
    BASECLASS *base;
    VBASEENTRY *vbase;
    if (basetype(sp->tp)->type == bt_union)
    {
        hr = sp->tp->syms->table[0];
        while (hr)
        {
            SYMBOL *sp = (SYMBOL *)hr->p;
            if (isstructured(sp->tp))
            {
                SYMBOL *cons = search(overloadNameTab[CI_CONSTRUCTOR], sp->tp->syms);
                HASHREC *hr1 = cons->tp->syms->table[0];
                while (hr1)
                {
                    cons = (SYMBOL *)hr->p;
                    if (matchesCopy(cons, FALSE))
                        if (!cons->trivialCons)
                            return TRUE;
                    hr1 = hr1->next;
                }
            }
            hr = hr->next;
        }
    }
    hr = sp->tp->syms->table[0];
    while (hr)
    {
        SYMBOL *sp1 = (SYMBOL *)hr->p;
        TYPE *m;
        if (sp1->storage_class == sc_member)
        {
            if (basetype(sp1->tp)->type == bt_rref)
                return TRUE;
            if (isstructured(sp1->tp))
            {
                if (checkDest(sp, sp1->tp->syms))
                    return TRUE;
            }
            m = sp1->tp;
            if (isarray(m))
                m = basetype(sp1->tp)->btp;
            if (isstructured(m))
            {
                if (checkCopyCons(sp, m->sp))
                    return TRUE;
            }
        }        
        hr = hr->next;
    }
    
    base = sp->baseClasses;
    while (base)
    {
        if (checkDest(sp, base->cls->tp->syms))
            return TRUE;
        if (checkCopyCons(sp, base->cls))
            return TRUE;
        base = base->next;
    }
    vbase = sp->vbaseEntries;
    while (vbase)
    {
        if (checkDest(sp, vbase->cls->tp->syms))
            return TRUE;
        if (checkCopyCons(sp, vbase->cls))
            return TRUE;
        vbase = vbase->next;
    }
    return FALSE;
    
}
static BOOL isCopyAssignmentDeleted(SYMBOL *sp)
{
    HASHREC *hr;
    BASECLASS *base;
    VBASEENTRY *vbase;
    if (basetype(sp->tp)->type == bt_union)
    {
        hr = sp->tp->syms->table[0];
        while (hr)
        {
            SYMBOL *sp = (SYMBOL *)hr->p;
            if (isstructured(sp->tp))
            {
                SYMBOL *cons = search(overloadNameTab[assign - kw_new + CI_NEW], sp->tp->syms);
                HASHREC *hr1 = cons->tp->syms->table[0];
                while (hr1)
                {
                    cons = (SYMBOL *)hr->p;
                    if (matchesCopy(cons, FALSE))
                        if (!cons->trivialCons)
                            return TRUE;
                    hr1 = hr1->next;
                }
            }
            hr = hr->next;
        }
    }
    hr = sp->tp->syms->table[0];
    while (hr)
    {
        SYMBOL *sp1 = (SYMBOL *)hr->p;
        TYPE *m;
        if (sp1->storage_class == sc_member)
        {
            if (isref(sp1->tp))
                return TRUE;
            if (!isstructured(sp1->tp) && isconst(sp1->tp))
                return TRUE;
            m = sp1->tp;
            if (isarray(m))
                m = basetype(sp1->tp)->btp;
            if (isstructured(m))
            {
                if (checkCopyAssign(sp, m->sp))
                    return TRUE;
            }
        }        
        hr = hr->next;
    }
    
    base = sp->baseClasses;
    while (base)
    {
        if (checkCopyAssign(sp, base->cls))
            return TRUE;
        base = base->next;
    }
    vbase = sp->vbaseEntries;
    while (vbase)
    {
        if (checkCopyAssign(sp, vbase->cls))
            return TRUE;
        vbase = vbase->next;
    }
    return FALSE;
    
}
static BOOL isMoveConstructorDeleted(SYMBOL *sp)
{
    HASHREC *hr;
    BASECLASS *base;
    VBASEENTRY *vbase;
    if (basetype(sp->tp)->type == bt_union)
    {
        hr = sp->tp->syms->table[0];
        while (hr)
        {
            SYMBOL *sp = (SYMBOL *)hr->p;
            if (isstructured(sp->tp))
            {
                SYMBOL *cons = search(overloadNameTab[CI_CONSTRUCTOR], sp->tp->syms);
                HASHREC *hr1 = cons->tp->syms->table[0];
                while (hr1)
                {
                    cons = (SYMBOL *)hr->p;
                    if (matchesCopy(cons, TRUE))
                        if (!cons->trivialCons)
                            return TRUE;
                    hr1 = hr1->next;
                }
            }
            hr = hr->next;
        }
    }
    hr = sp->tp->syms->table[0];
    while (hr)
    {
        SYMBOL *sp1 = (SYMBOL *)hr->p;
        TYPE *m;
        if (sp1->storage_class == sc_member)
        {
            if (basetype(sp1->tp)->type == bt_rref)
                return TRUE;
            if (isstructured(sp1->tp))
            {
                if (checkDest(sp, sp1->tp->syms))
                    return TRUE;
            }
            m = sp1->tp;
            if (isarray(m))
                m = basetype(sp1->tp)->btp;
            if (isstructured(m))
            {
                if (checkMoveCons(sp, m->sp))
                    return TRUE;
            }
        }        
        hr = hr->next;
    }
    
    base = sp->baseClasses;
    while (base)
    {
        if (checkDest(sp, base->cls->tp->syms))
            return TRUE;
        if (checkMoveCons(sp, base->cls))
            return TRUE;
        base = base->next;
    }
    vbase = sp->vbaseEntries;
    while (vbase)
    {
        if (checkDest(sp, vbase->cls->tp->syms))
            return TRUE;
        if (checkMoveCons(sp, vbase->cls))
            return TRUE;
        vbase = vbase->next;
    }
    return FALSE;
    
}
static BOOL isMoveAssignmentDeleted(SYMBOL *sp)
{
    HASHREC *hr;
    BASECLASS *base;
    VBASEENTRY *vbase;
    if (basetype(sp->tp)->type == bt_union)
    {
        hr = sp->tp->syms->table[0];
        while (hr)
        {
            SYMBOL *sp = (SYMBOL *)hr->p;
            if (isstructured(sp->tp))
            {
                SYMBOL *cons = search(overloadNameTab[assign - kw_new + CI_NEW], sp->tp->syms);
                HASHREC *hr1 = cons->tp->syms->table[0];
                while (hr1)
                {
                    cons = (SYMBOL *)hr->p;
                    if (matchesCopy(cons, TRUE))
                        if (!cons->trivialCons)
                            return TRUE;
                    hr1 = hr1->next;
                }
            }
            hr = hr->next;
        }
    }
    hr = sp->tp->syms->table[0];
    while (hr)
    {
        SYMBOL *sp1 = (SYMBOL *)hr->p;
        TYPE *m;
        if (sp1->storage_class == sc_member)
        {
            if (isref(sp1->tp))
                return TRUE;
            if (!isstructured(sp1->tp) && isconst(sp1->tp))
                return TRUE;
            m = sp1->tp;
            if (isarray(m))
                m = basetype(sp1->tp)->btp;
            if (isstructured(m))
            {
                if (checkMoveAssign(sp, m->sp))
                    return TRUE;
            }
        }        
        hr = hr->next;
    }
    
    base = sp->baseClasses;
    while (base)
    {
        if (checkMoveAssign(sp, base->cls))
            return TRUE;
        base = base->next;
    }
    vbase = sp->vbaseEntries;
    while (vbase)
    {
        if (checkMoveAssign(sp, vbase->cls))
            return TRUE;
        vbase = vbase->next;
    }
    return FALSE;
    
}
static BOOL conditionallyDeleteDefaultConstructor(SYMBOL *func)
{
    HASHREC *hr = func->tp->syms->table[0];
    while (hr)
    {
        SYMBOL *sp = (SYMBOL *)hr->p;
        if (sp->defaulted && matchesDefaultConstructor(sp))
        {
            if (isDefaultDeleted(sp))
                sp->deleted = TRUE;
        }
        hr = hr->next;
    }
    return FALSE;
}
static BOOL conditionallyDeleteCopyConstructor(SYMBOL *func, BOOL move)
{
    HASHREC *hr = func->tp->syms->table[0];
    while (hr)
    {
        SYMBOL *sp = (SYMBOL *)hr->p;
        if (sp->defaulted && matchesCopy(sp, move))
        {
            if (isCopyConstructorDeleted(sp))
                sp->deleted = TRUE;
        }
        hr = hr->next;
    }
    return FALSE;
}
static BOOL conditionallyDeleteCopyAssignment(SYMBOL *func, BOOL move)
{
    HASHREC *hr = func->tp->syms->table[0];
    while (hr)
    {
        SYMBOL *sp = (SYMBOL *)hr->p;
        if (sp->defaulted && matchesCopy(sp, move))
        {
            if (isCopyAssignmentDeleted(sp))
                sp->deleted = TRUE;
        }
        hr = hr->next;
    }
    return FALSE;
}
void createDefaultConstructors(SYMBOL *sp)
{
    SYMBOL *cons = search(overloadNameTab[CI_CONSTRUCTOR], sp->tp->syms);
    SYMBOL *dest = search(overloadNameTab[CI_DESTRUCTOR], sp->tp->syms);
    SYMBOL *asgn = search(overloadNameTab[assign - kw_new + CI_NEW], sp->tp->syms);
    if (!dest)
        declareDestructor(sp);
    if (cons)
    {
        sp->hasUserCons = TRUE;
    }
    else
    {
        SYMBOL *newcons;
        // first see if the default constructor could be trivial
        if (sp->vtabEntries == NULL && sp->vbaseEntries == NULL)
        {
            BASECLASS *base = sp->baseClasses;
            while (base)
            {
                if (!base->cls->trivialCons)
                    break;
                base = base->next;
            }
            if (!base)
            {
                HASHREC *p = sp->tp->syms->table[0];
                while (p)
                {
                    SYMBOL *pcls = (SYMBOL *)p->p;
                    if (pcls->storage_class == sc_member)
                    {
                        if (isstructured(pcls->tp))
                        {
                            if (!sp->trivialCons)
                                break;
                        }
                        else if (pcls->init) // brace or equal initializer goes here
                            break;       
                    }
                    p = p->next;
                }
                if (!p)
                {
                    sp->trivialCons = TRUE;
                }
            }
        }
        // now create the default constructor
        newcons = declareConstructor(sp, TRUE, FALSE);
        newcons->trivialCons = sp->trivialCons;
    }
    conditionallyDeleteDefaultConstructor(cons);
    // now if there is no copy constructor or assignment operator declare them
    if (!hasCopy(cons, FALSE))
    {
        SYMBOL *newcons = declareConstructor(sp, FALSE, FALSE);
        newcons->trivialCons = hasTrivialCopy(sp, FALSE);
        if (hasCopy(cons, TRUE) || hasCopy(asgn, TRUE))
            newcons->deleted = TRUE;            
    }
    conditionallyDeleteCopyConstructor(cons, FALSE);
    if (!hasCopy(asgn, FALSE))
    {
        SYMBOL *newsp = declareAssignmentOp(sp, FALSE);
        newsp->trivialCons = hasTrivialAssign(sp, FALSE);
        if (hasCopy(cons, TRUE) || hasCopy(asgn, TRUE))
            newsp->deleted = TRUE;
            
    }
    conditionallyDeleteCopyAssignment(cons,FALSE);
    // now if there is no move constructor, no copy constructor,
        // no copy assignment, no move assignment, no destructor
        // and wouldn't be defined as deleted
        // declare a move constructor and assignment operator
    if (!dest && !hasCopy(cons,FALSE ) && !hasCopy(cons,TRUE) &&
        !hasCopy(asgn, FALSE) && !hasCopy(asgn, TRUE))
    {
        BOOL b = isMoveAssignmentDeleted(sp);
        SYMBOL *newcons;
        if (!b)
        {
            newcons = declareConstructor(sp, FALSE, TRUE);
            newcons->trivialCons = hasTrivialCopy(sp, TRUE);
        }
        newcons = declareAssignmentOp(sp, TRUE);
        newcons->trivialCons = hasTrivialAssign(sp, TRUE);
        newcons->deleted = isMoveAssignmentDeleted(sp);
    }
    else
    {
        conditionallyDeleteCopyConstructor(cons,TRUE);
        conditionallyDeleteCopyAssignment(cons,TRUE);
    }
}
static void destructList(HASHREC *p, EXPRESSION **exp)
{
    while (p)
    {
        SYMBOL *sp = (SYMBOL *)p->p;
        if (sp->storage_class != sc_localstatic && sp->dest)
        {
            
            EXPRESSION *iexp = convertInitToExpression(sp->dest->basetp, sp, sp->dest, NULL);
            if (*exp)
                *exp = exprNode(en_void, iexp, *exp);
            else
                *exp = iexp;
        }
        p = p->next;
    }
}
void destructBlock(BLOCKDATA *b, HASHTABLE *syms)
{
    EXPRESSION *exp = NULL;
    destructList(syms->table[0], &exp);
    if (exp)
    {
        STATEMENT *st = stmtNode(b, st_expr);
        st->select = exp;
    }
}
static MEMBERINITIALIZERS *getInit(MEMBERINITIALIZERS *init, SYMBOL *member)
{
    while (init)
    {
        if (init->sp == member)
            return init;
        init = init->next;
    }
    return NULL;
}
static void genDataInit(BLOCKDATA *b, SYMBOL *cls, MEMBERINITIALIZERS *mi, SYMBOL *member, EXPRESSION *thisptr)
{
    INITIALIZER *x = member->init;
    MEMBERINITIALIZERS *v = getInit(mi, member);
    if (v)
    {
        LEXEME *lex;
        member->init = NULL;
        lex = SetAlternateLex(NULL, v->initData);
        lex = initialize(lex, NULL, v->sp, sc_member);
        lex = SetAlternateLex(lex, NULL);
    }
    if (member->init)
    {
        EXPRESSION *exp = convertInitToExpression(cls->tp, cls, member->init, thisptr);
        STATEMENT *st = stmtNode(b, st_expr);
        st->select = exp;
    }
    member->init = x;
}
static void genConstructorCall(BLOCKDATA *b, SYMBOL *cls, MEMBERINITIALIZERS *mi, SYMBOL *member, int memberOffs, BOOL top, EXPRESSION *thisptr)
{
    MEMBERINITIALIZERS *v = getInit(mi, member);
    if (v)
    {
        FUNCTIONCALL *funcparams = Alloc(sizeof(FUNCTIONCALL));
        EXPRESSION *exp = exprNode(en_add, thisptr, intNode(en_c_i, memberOffs));
        STATEMENT *st;
        LEXEME *lex = SetAlternateLex(NULL, v->initData);
        lex = getArgs(lex, NULL, funcparams);
        lex = SetAlternateLex(lex, NULL);
        if (!callConstructor(&cls->tp, &exp, funcparams, FALSE, NULL, top))
            errorsym(ERR_NO_APPROPRIATE_CONSTRUCTOR, cls);
            
        st = stmtNode(b, st_expr);
        st->select = exp;
    }
    else if (member->init)
    {
        EXPRESSION *iexp = convertInitToExpression(member->tp, member, member->init, thisptr);
        EXPRESSION *dexp;
        EXPRESSION *exp = exprNode(en_add, thisptr, intNode(en_c_i, memberOffs));
        FUNCTIONCALL *params = (FUNCTIONCALL *)Alloc(sizeof(FUNCTIONCALL));
        STATEMENT *st;
        params->arguments = (ARGLIST *)Alloc(sizeof(ARGLIST));
        params->arguments->tp = (TYPE *)Alloc(sizeof(TYPE));
        params->arguments->tp->type = bt_lref;
        params->arguments->tp->btp = cls->tp;
        params->arguments->exp = params->arguments->rootexp = intNode(en_c_i, 0);
        if (!callConstructor(&cls->tp, &exp, NULL, FALSE, NULL, top))
            errorsym(ERR_NO_COPY_CONSTRUCTOR, cls);
        if (member->dest)
        {
            dexp = convertInitToExpression(member->tp, member, member->dest, thisptr);
            exp = exprNode(en_void, iexp, exprNode(en_void, exp, dexp));
        }
        else
        {
            exp = exprNode(en_void, iexp, exp);
        }
        st = stmtNode(b, st_expr);
        st->select = exp;
    }
    else if (!cls->trivialCons)// default constructor
    {
        EXPRESSION *exp = exprNode(en_add, thisptr, intNode(en_c_i, memberOffs));
        STATEMENT *st;
        if (!callConstructor(&cls->tp, &exp, NULL, FALSE, NULL, top))
            errorsym(ERR_NO_DEFAULT_CONSTRUCTOR, cls);
        st = stmtNode(b, st_expr);
        st->select = exp;
    }
}
static void virtualBaseThunks(BLOCKDATA *b, SYMBOL *sp, EXPRESSION *thisptr)
{
    VBASEENTRY *entries = sp->vbaseEntries;
    EXPRESSION *first = NULL, **pos = &first;
    STATEMENT *st;
    while (entries)
    {
        EXPRESSION *left = exprNode(en_add, thisptr, intNode(en_c_i, entries->pointerOffset));
        EXPRESSION *right = exprNode(en_add, thisptr, intNode(en_c_i, entries->structOffset));
        EXPRESSION *asn;
        deref(&stdpointer, &left);
        asn = exprNode(en_assign, left, right);
        if (!*pos)
        {
            *pos = asn;
        }
        else
        {
            *pos = exprNode(en_void, *pos, asn);
            pos = &(*pos)->right;
        }
        entries = entries->next;
    }
    if (first)
    {
        st = stmtNode(b, st_expr);
        st->select = first;
    }
}
static void dovtabThunks(BLOCKDATA *b, SYMBOL *sym, EXPRESSION *thisptr)
{
    VTABENTRY *entries = sym->vtabEntries;
    EXPRESSION *first = NULL, **pos = &first;
    STATEMENT *st;
    SYMBOL *localsp;
    char buf[256];
    strcpy(buf, sym->decoratedName);
    strcat(buf, "_$vtt");
    localsp = makeID(sc_static, &stdvoid, NULL, litlate(buf));
    localsp->decoratedName = localsp->errname = localsp->name;
    while (entries)
    {
        if (!entries->isdead)
        {
            EXPRESSION *left = exprNode(en_add, thisptr, intNode(en_c_i, entries->dataOffset));
            EXPRESSION *right = exprNode(en_add, varNode(en_global, localsp), intNode(en_c_i, entries->vtabOffset));
            EXPRESSION *asn;
            deref(&stdpointer, &left);
            asn = exprNode(en_assign, left, right);
            if (!*pos)
            {
                *pos = asn;
            }
            else
            {
                *pos = exprNode(en_void, *pos, asn);
                pos = &(*pos)->right;
            }
            
        }
        entries = entries->next;
    }
    if (first)
    {
        st = stmtNode(b, st_expr);
        st->select = first;
    }
}
static void doVirtualBases(BLOCK *b, SYMBOL *sp, MEMBERINITIALIZERS *mi, VBASEENTRY *vbe, EXPRESSION *thisptr)
{
	if (vbe)
	{
	    doVirtualBases(b, sp, mi, vbe->next, thisptr);
		genConstructorCall(b, sp, mi, vbe->cls, vbe->structOffset, FALSE, thisptr);
	}
}
static void lookupInitializers(SYMBOL *cls)
{
    MEMBERINITIALIZERS *init = cls->memberInitializers;
    while (init)
    {
        errorline = init->line;
        errorfile = init->file;
        init->sp = search(init->name, cls->tp->syms);
        if (init->sp)
        {
            if (init->sp->storage_class != sc_member)
                errorsym(ERR_NEED_NONSTATIC_MEMBER, init->sp);
        }
        else
        {
            BASECLASS *bc = cls->baseClasses;
            while (bc)
            {
                if (!strcmp(bc->cls->name, init->name))
                {
                    if (init->sp)
                    {
                        errorsym2(ERR_NOT_UNAMBIGUOUS_BASE, init->sp, cls);
                    }
                }
                bc = bc->next;
            }
        }
        if (!init->sp)
        {
            errorstrsym(ERR_NOT_A_MEMBER_OR_BASE_CLASS, init->name, cls);
        }
        init = init->next;
    }
}
void thunkConstructorHead(BLOCKDATA *b, SYMBOL *sym, SYMBOL *cons)
{
    EXPRESSION *thisptr;
    BASECLASS *bc;
    HASHREC *hr;
    thisptr  = varNode(en_this, sym->parentClass);
    lookupInitializers(cons);
    if (sym->vbaseEntries)
    {
        SYMBOL *sp = makeID(sc_auto, &stdint, NULL, "__$$constop");
        EXPRESSION *val = varNode(en_auto, sp);
        int lbl = beGetLabel;
        STATEMENT *st;
        sp->decoratedName = sp->errname = sp->name;
        sp->offset = chosenAssembler->arch->retblocksize + cons->paramSize;
        insert(sp, localNameSpace->syms);
        deref(&stdint, &val);
        st = stmtNode(b, st_notselect);
        st->select = val;
        st->label = lbl;
        virtualBaseThunks(b, sym, thisptr);
        doVirtualBases(b, sym, cons->memberInitializers, sym->vbaseEntries, thisptr);
        st = stmtNode(b, st_label);
        st->label = lbl;
    }
    bc = sym->baseClasses;
    while (bc)
    {
        if (!bc->isvirtual)
            genConstructorCall(b, sym, cons->memberInitializers, bc->cls, bc->offset, FALSE, thisptr);
        bc = bc->next;
    }
    dovtabThunks(b, sym, thisptr);
    hr = sym->tp->syms->table[0];
    while (hr)
    {
        SYMBOL *sp = (SYMBOL *)hr->p;
        if (sp->storage_class == sc_member)
        {
            if (isstructured(sp->tp))
            {
                genConstructorCall(b, sym, cons->memberInitializers, sp, sp->offset,TRUE, thisptr);
            }
            else
            {
                genDataInit(b, sym, cons->memberInitializers, sp, thisptr);
            }
        }
        hr = hr->next;
    }
}
static void createConstructor(SYMBOL *sp, SYMBOL *consfunc)
{
    BLOCKDATA b;
    memset(&b, 0, sizeof(BLOCKDATA));
    b.type = begin;
    AllocateLocalContext(&b, consfunc);
    thunkConstructorHead(&b, sp, consfunc);
    consfunc->inlineFunc.stmt = stmtNode(NULL, st_block);
    consfunc->inlineFunc.stmt->lower = b.head;
    consfunc->inlineFunc.stmt->blockTail = b.tail;
    InsertInline(consfunc);
    FreeLocalContext(&b, consfunc);
}
static void asnVirtualBases(BLOCKDATA *b, SYMBOL *sp, VBASEENTRY *vbe, 
                            EXPRESSION *thisptr, EXPRESSION *other, BOOL move, BOOL isconst)
{
	if (vbe)
	{
	    asnVirtualBases(b, sp, vbe->next, thisptr, other, move, isconst);
		genAsnCall(b, sp, vbe->cls, vbe->structOffset, thisptr, other, move, isconst);
	}
}
static void genAsnData(BLOCKDATA *b, SYMBOL *cls, SYMBOL *member, EXPRESSION *thisptr, EXPRESSION *other)
{
    EXPRESSION *left = exprNode(en_add, thisptr, intNode(en_c_i, member->offset));
    EXPRESSION *right = exprNode(en_add, other, intNode(en_c_i, member->offset));
    STATEMENT *st;
    (void)cls;
    deref(member->tp, &left);
    deref(member->tp, &right);
    left = exprNode(en_assign, left, right);
    st = stmtNode(b, st_expr);
    st->select = left;
}
static void genAsnCall(BLOCKDATA *b, SYMBOL *cls, SYMBOL *base, int offset, EXPRESSION *thisptr, EXPRESSION *other, BOOL move, BOOL isconst)
{
    if (base->trivialCons)
    {
        EXPRESSION *left = exprNode(en_add, thisptr, intNode(en_c_i, offset));
        EXPRESSION *right = exprNode(en_add, other, intNode(en_c_i, offset));
        STATEMENT *st;
        left = exprNode(en_blockassign, left, right);
        left->size = base->tp->size;
        st = stmtNode(b, st_expr);
        st->select = left;
    }
    else
    {
        EXPRESSION *left = exprNode(en_add, thisptr, intNode(en_c_i, offset));
        EXPRESSION *right = exprNode(en_add, other, intNode(en_c_i, offset));
        EXPRESSION *exp = NULL;
        STATEMENT *st;
        FUNCTIONCALL *params = (FUNCTIONCALL *)Alloc(sizeof(FUNCTIONCALL));
        TYPE *tp = (TYPE *)Alloc(sizeof(TYPE));
        SYMBOL *asn1;
        SYMBOL *cons = search(overloadNameTab[CI_CONSTRUCTOR], base->tp->syms);
        if (isconst)
        {
            TYPE *tp1;
            tp1 = (TYPE *)Alloc(sizeof(TYPE));
            tp1->type = move ? bt_rref : bt_lref;
            tp1->btp = cls->tp;
            tp->type = bt_const;
            tp->btp = tp1;
        }
        else
        {
            tp->type = move ? bt_rref : bt_lref;
            tp->btp = cls->tp;
        }
        params->arguments = (ARGLIST *)Alloc(sizeof(ARGLIST));
        params->arguments->tp = tp;
        params->arguments->exp = right;
        params->thisptr = left;
        asn1 = GetOverloadedFunction(&tp, &exp, cons, params, NULL, TRUE);
            
        if (asn1)
        {
            if (!isAccessible(base,base, asn1, NULL, ac_protected, FALSE))
            {
                errorsym(ERR_CANNOT_ACCESS, asn1);
            }
            if (asn1->defaulted && !asn1->inlineFunc.stmt)
                createAssignment(cls, asn1);
            if (asn1->linkage == lk_inline)
            {
                exp = doinline(params, asn1);
            }
            else
            {
                asn1->genreffed = TRUE;
            }
            st = stmtNode(b, st_expr);
            st->select = exp;
        }
    }
}
static void thunkAssignments(BLOCKDATA *b, SYMBOL *sym, BOOL move, BOOL isconst)
{
    EXPRESSION *thisptr = varNode(en_this, sym);
    BASECLASS *base;
    HASHREC *hr;
    SYMBOL *othersym = makeID(sc_auto, sym->tp, NULL, "__$$other");
    EXPRESSION *other = varNode(en_auto, othersym);
    sym->decoratedName = sym->errname = sym->name;
    sym->offset = chosenAssembler->arch->retblocksize + getSize(bt_pointer);
    if (sym->vbaseEntries)
    {
        asnVirtualBases(b, sym, sym->vbaseEntries, thisptr, other, move, isconst);
    }
    base = sym->baseClasses;
    while (base)
    {
        if (!base->isvirtual)
        {
            genAsnCall(b, sym, base->cls, base->offset, thisptr, other, move, isconst);
        }
        base = base->next;
    }
    hr = sym->tp->syms->table[0];
    while (hr)
    {
        SYMBOL *sp = (SYMBOL *)hr->p;
        if (sp->storage_class == sc_member)
        {
            if (isstructured(sp->tp))
            {
                genAsnCall(b, sym, sp, sp->offset, thisptr, other, move, isconst);
            }
            else
            {
                genAsnData(b, sym, sp, thisptr, other);
            }
        }
        hr = hr->next;
    }
}
void createAssignment(SYMBOL *sym, SYMBOL *asnfunc)
{
    // if we get here we are just assuming it is a builtin assignment operator
    // because we only get here for 'default' functions and that is the only one
    // that can be defaulted...
    BLOCKDATA b;
    BOOL move = basetype(((SYMBOL *)asnfunc->tp->syms->table[0]->p)->tp)->type == bt_rref;
    BOOL isConst = isconst(((SYMBOL *)asnfunc->tp->syms->table[0]->p)->tp);
    memset(&b, 0, sizeof(BLOCKDATA));
    b.type = begin;
    AllocateLocalContext(&b, asnfunc);
    thunkAssignments(&b, sym, move, isConst);
    asnfunc->inlineFunc.stmt = stmtNode(NULL, st_block);
    asnfunc->inlineFunc.stmt->lower = b.head;
    asnfunc->inlineFunc.stmt->blockTail = b.tail;
    InsertInline(asnfunc);
    FreeLocalContext(&b, asnfunc);
}
static void genDestructorCall(BLOCKDATA *b, SYMBOL *sp, EXPRESSION *base, int offset, BOOL top)
{
    SYMBOL *dest = search(overloadNameTab[CI_DESTRUCTOR], sp->tp->syms);
    EXPRESSION *exp = exprNode(en_add, base, intNode(en_c_i, offset)) ;
    STATEMENT *st;
    dest = (SYMBOL *)dest->tp->syms->table[0]->p;
    if (dest->defaulted && !dest->inlineFunc.stmt)
    {
        createDestructor(sp);
    }
    callDestructor(sp, &exp, NULL, top);
    st = stmtNode(b, st_expr);
    st->select = exp;
}
static void undoVars(BLOCKDATA *b, HASHREC *vars, EXPRESSION *base)
{
	if (vars)
	{
		SYMBOL *s = (SYMBOL *)vars->p;
		undoVars(b, vars->next, base);
		if (s->storage_class == sc_member && isstructured(s->tp))
			genDestructorCall(b, (SYMBOL *)s, base, s->offset, TRUE);
	}
}
static void undoBases(BLOCKDATA *b, BASECLASS *bc, EXPRESSION *base)
{
	if (bc)
	{
		undoBases(b, bc->next, base);
		if (!bc->isvirtual)
		{
			genDestructorCall(b, bc->cls, base, bc->offset, FALSE);
		}
	}
}
void thunkDestructorTail(BLOCKDATA *b, SYMBOL *sp)
{
    EXPRESSION *thisptr;
    VBASEENTRY *vbe = sp->vbaseEntries;    
    thisptr  = varNode(en_this, sp);
    undoVars(b, sp->tp->syms->table[0], thisptr);
    undoBases(b, sp->baseClasses, thisptr);
    if (vbe)
    {
        SYMBOL *sp = makeID(sc_auto, &stdint, NULL, "__$$desttop");
        EXPRESSION *val = varNode(en_auto, sp);
        int lbl = beGetLabel;
        STATEMENT *st;
        sp->decoratedName = sp->errname = sp->name;
        sp->offset = chosenAssembler->arch->retblocksize + getSize(bt_pointer);
        insert(sp, localNameSpace->syms);
        deref(&stdint, &val);
        st = stmtNode(b, st_notselect);
        st->select = val;
        st->label = lbl;
        while (vbe)
        {
            genDestructorCall(b, vbe->cls, thisptr, vbe->structOffset, FALSE);
            vbe = vbe->next;
        }
        st = stmtNode(b, st_label);
        st->label = lbl;
    }
}
static void createDestructor(SYMBOL *sp)
{
    SYMBOL *dest = search(overloadNameTab[CI_DESTRUCTOR], sp->tp->syms);
    BLOCKDATA b;
    memset(&b, 0, sizeof(BLOCKDATA));
    b.type = begin;
    dest = (SYMBOL *)dest->tp->syms->table[0]->p;
    AllocateLocalContext(&b, dest);
    thunkDestructorTail(&b, sp);
    dest->inlineFunc.stmt = stmtNode(NULL, st_block);
    dest->inlineFunc.stmt->lower = b.head;
    dest->inlineFunc.stmt->blockTail = b.tail;
    InsertInline(dest);
    FreeLocalContext(&b, dest);
}
static void makeArrayConsDest(TYPE **tp, EXPRESSION **exp, SYMBOL *func, BOOL forward, EXPRESSION *count)
{
    EXPRESSION *size = intNode(en_c_i, (*tp)->size + (*tp)->arraySkew);
    EXPRESSION *efunc = varNode(en_pc, func);
    FUNCTIONCALL *params = (FUNCTIONCALL *)Alloc(sizeof(FUNCTIONCALL));
    SYMBOL *asn1;
    ARGLIST *arg1 = (ARGLIST *)Alloc(sizeof(ARGLIST)); // func
    ARGLIST *arg2 = (ARGLIST *)Alloc(sizeof(ARGLIST)); // size
    ARGLIST *arg3 = (ARGLIST *)Alloc(sizeof(ARGLIST)); // count
    ARGLIST *arg4 = (ARGLIST *)Alloc(sizeof(ARGLIST)); // forward
    SYMBOL *ovl = search("__arrCall", globalNameSpace->syms);
    params->arguments = arg1;
    arg1->next = arg2;
    arg2->next = arg3;
    arg3->next = arg4;
    
    arg1->exp = arg1->rootexp = efunc;
    arg1->tp = &stdpointer;
    arg2->exp = arg2->rootexp = size;
    arg2->tp = &stdint;
    arg3->exp = arg3->rootexp = count;
    arg3->tp = &stdint;
    arg4->exp = arg4->rootexp = intNode(en_c_i, forward);
    arg4->tp = &stdint;
    
    asn1 = GetOverloadedFunction(tp, exp, ovl, params, NULL, TRUE);
    if (!asn1)
    {
        diag("makeArrayConsDest: Can't call array iterator");
    }
    else
    {
        asn1->genreffed = TRUE;
    }
    
}
void callDestructor(SYMBOL *sp, EXPRESSION **exp, EXPRESSION *arrayElms, BOOL top)
{
    SYMBOL *dest = search(overloadNameTab[CI_DESTRUCTOR], sp->tp->syms);
    SYMBOL *dest1;
    TYPE *tp = NULL;
    FUNCTIONCALL *params = (FUNCTIONCALL *)Alloc(sizeof(FUNCTIONCALL));
    params->arguments = (ARGLIST *)Alloc(sizeof(ARGLIST));
    params->arguments->tp = (TYPE *)Alloc(sizeof(TYPE));
    if (!*exp)
    {
        diag("callDestructor: no this pointer");
    }
    params->thisptr= *exp;
    params->thistp = sp->tp;
    if (sp->vbaseEntries)
    {
        params->arguments->tp->type = bt_int;
        params->arguments->tp->size = getSize(bt_int);
    }
    else
    {
        params->arguments->tp->type = bt_void;
    }
    params->arguments->exp = params->arguments->rootexp = intNode(en_c_i, top);
    dest1 = GetOverloadedFunction(tp, exp, dest, &params, NULL, TRUE);
    if (dest1 && dest1->defaulted && !dest1->inlineFunc.stmt)
        createDestructor(sp);
    if (!isAccessible(sp,sp, dest1, NULL, ac_protected, FALSE))
    {
        errorsym(ERR_CANNOT_ACCESS, dest1);
    }
    if (arrayElms)
    {
        makeArrayConsDest(tp, exp, dest1, FALSE, arrayElms);
        dest1->genreffed = TRUE;
    }
    else if (dest1->linkage == lk_inline)
    {
        *exp = doinline(params, dest1);
    }
    else
    {
        dest1->genreffed = TRUE;
    }
}
BOOL callConstructor(TYPE **tp, EXPRESSION **exp, FUNCTIONCALL *params, BOOL checkcopy, EXPRESSION *arrayElms, BOOL top)
{
    SYMBOL *sp = basetype(*tp)->sp;
    SYMBOL *cons = search(overloadNameTab[CI_CONSTRUCTOR], sp->tp->syms);
    SYMBOL *cons1;
    if (checkcopy)
    {
        SYMBOL *copy = getCopyCons(sp, TRUE);
        SYMBOL *dest = search(overloadNameTab[CI_DESTRUCTOR], sp->tp->syms);
        dest = (SYMBOL *)dest->tp->syms->table[0]->p;
        if (!isAccessible(sp,sp, copy, NULL, ac_protected, FALSE))
        {
            errorsym(ERR_CANNOT_ACCESS, copy);
        }
        if (!isAccessible(sp,sp, dest, NULL, ac_protected, FALSE))
        {
            errorsym(ERR_CANNOT_ACCESS, dest);
        }
    }
    if (!params)
    {
        params = (FUNCTIONCALL *)Alloc(sizeof(FUNCTIONCALL));
        params->arguments = (ARGLIST *)Alloc(sizeof(ARGLIST));
        params->arguments->tp = (TYPE *)Alloc(sizeof(TYPE));
        if (sp->vbaseEntries)
        {
            params->arguments->tp->type = bt_int;
            params->arguments->exp = params->arguments->rootexp = intNode(en_c_i, top);
        }
        else
        {
            params->arguments->tp->type = bt_void;
            params->arguments->exp = params->arguments->rootexp = intNode(en_c_i, 0);
        }
    }
    else if (sp->vbaseEntries)
    {
        ARGLIST **arg = &params->arguments;
        while (*arg)
            arg = &(*arg)->next;
        (*arg) = (ARGLIST *)Alloc(sizeof(ARGLIST));
        (*arg)->tp = (TYPE *)Alloc(sizeof(TYPE));
        (*arg)->tp->type = bt_int;
        (*arg)->exp = (*arg)->rootexp = intNode(en_c_i, top);
    }
    cons1 = GetOverloadedFunction(tp, exp, cons, params, NULL, TRUE);
        
    if (cons1)
    {
        if (!isAccessible(sp,sp, cons1, NULL, ac_protected, FALSE))
        {
            errorsym(ERR_CANNOT_ACCESS, cons1);
        }
        if (cons1->defaulted && !cons1->inlineFunc.stmt)
            createConstructor(sp, cons1);
        if (arrayElms)
        {
            makeArrayConsDest(tp, exp, cons1, TRUE, arrayElms);
            cons1->genreffed = TRUE;
        }
        else if (cons1->linkage == lk_inline)
        {
            *exp = doinline(params, cons1);
        }
        else
        {
            cons1->genreffed = TRUE;
        }
        return TRUE;
    }
    return FALSE;
}
