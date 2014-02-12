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
extern TYPE stdint;
extern NAMESPACEVALUES *localNameSpace;
extern TYPE stdpointer;
extern int startlab, retlab;
extern int total_errors;

static LIST *inlineHead, *inlineTail, *inlineVTabHead, *inlineVTabTail;
static LIST *inlineDataHead, *inlineDataTail;

static SYMBOL *inlinesp_list[MAX_INLINE_NESTING];
static int inlinesp_count;
static HASHTABLE *vc1Thunks;

static int namenumber;

void inlineinit(void)
{
    namenumber = 0;
    inlineHead = NULL;
    inlineVTabHead = NULL;
    inlineDataHead = NULL;
    vc1Thunks = CreateHashTable(1);
}
static void UndoPreviousCodegen(SYMBOL *sym)
{
    HASHTABLE *syms = sym->inlineFunc.syms;
    while (syms)
    {
        HASHREC *hr = syms->table[0];
        while (hr)
        {
            SYMBOL *sx = (SYMBOL *)hr->p;
            sx->imaddress = sx->imvalue = NULL;
            sx->imind = NULL;
            hr = hr->next;
        }
        syms = syms->next;
    }
    
}
void dumpInlines(void)
{
#ifndef PARSER_ONLY
    if (!total_errors)
    {
        BOOL done;
        LIST *vtabList;
        LIST *dataList;
        cseg();
        do
        {
            LIST *funcList = inlineHead;
            done = TRUE;
            while (funcList)
            {
                SYMBOL *sym = (SYMBOL *)funcList->data;
                if (sym->genreffed && sym->inlineFunc.stmt && !sym->didinline)
                {
                    sym->genreffed = FALSE;
                    UndoPreviousCodegen(sym);
                    startlab = nextLabel++;
                    retlab = nextLabel++;
                    genfunc(sym);
                    sym->didinline = TRUE;
                    done = FALSE;
                }
                funcList = funcList->next;
            }
            startlab = retlab = 0;
            vtabList = inlineVTabHead;
            while (vtabList)
            {
                SYMBOL *sym = (SYMBOL *)vtabList->data;
                if (sym->vtabsp->genreffed && hasVTab(sym))
                {
                    sym->vtabsp->genreffed = FALSE;
                    dumpVTab(sym);
                    done = FALSE;
                }
                vtabList = vtabList->next;
                
            }
        } while (!done);
        dataList = inlineDataHead;
        while (dataList)
        {
            SYMBOL *sym = (SYMBOL *)dataList->data;
            if (sym->genreffed)
            {
                sym->genreffed = FALSE;
                gen_virtual(sym, TRUE);
                if (sym->init)
                    dumpInit(sym, sym->init);
                else
                    genstorage(basetype(sym->tp)->size);
                gen_endvirtual(sym);
            }
            dataList = dataList->next;
        }
    }
#endif
}
void dumpvc1Thunks(void)
{
#ifndef PARSER_ONLY
    HASHREC *hr;
    cseg();
    hr = vc1Thunks->table[0];
    while (hr)
    {
        gen_virtual((SYMBOL *)hr->p, FALSE);
        gen_vc1((SYMBOL *)hr->p);
        gen_endvirtual((SYMBOL *)hr->p);
        hr = hr->next;
    }
#endif
}
static void ReferenceVTabFuncs(VTABENTRY *entries)
{
    if (entries)
    {
        VIRTUALFUNC *vf;
        ReferenceVTabFuncs(entries->next);
        vf = entries->virtuals;
        while (vf)
        {
            vf->func->genreffed = TRUE;
            vf = vf->next;
        }
    }
}
void ReferenceVTab(SYMBOL *sym)
{
    sym->genreffed = TRUE;
    ReferenceVTabFuncs(sym->vtabEntries);
}
SYMBOL *getvc1Thunk(int offset)
{
    char name[256];
    SYMBOL *rv;
    sprintf(name, "@$vc1$B0$%d$0", offset+1);
    rv = search(name, vc1Thunks);
    if (!rv)
    {
        rv = Alloc(sizeof(SYMBOL));
        rv->name = rv->errname = rv->decoratedName = litlate(name);
        rv->offset = offset;
        insert(rv, vc1Thunks);
    }
    return rv;
}
void InsertInline(SYMBOL *sp)
{
    LIST *temp = Alloc(sizeof(LIST));
    temp->data = sp;
    if (isfunction(sp->tp))
        if (inlineHead)
            inlineTail = inlineTail->next = temp;
        else
            inlineHead = inlineTail = temp;
    else
        if (inlineVTabHead)
            inlineVTabTail = inlineVTabTail->next = temp;
        else
            inlineVTabHead = inlineVTabTail = temp;
}
void InsertInlineData(SYMBOL *sp)
{
    LIST *temp = Alloc(sizeof(LIST));
    temp->data = sp;
    if (inlineDataHead)
        inlineDataTail = inlineDataTail->next = temp;
    else
        inlineDataHead = inlineDataTail = temp;
}
/*-------------------------------------------------------------------------*/

EXPRESSION *inlineexpr(EXPRESSION *node, BOOL *fromlval)
{
    /*
     * routine takes an enode tree and replaces it with a copy of itself.
     * Used because we have to munge the block_nesting field (value.i) of each
     * sp in an inline function to force allocation of the variables
     */
    EXPRESSION *temp,  *temp1;
    FUNCTIONCALL *fp;
    int i;
    (void)fromlval;
    if (node == 0)
        return 0;
    temp = (EXPRESSION *)Alloc(sizeof(EXPRESSION));
    memcpy(temp, node, sizeof(EXPRESSION));
    switch (temp->type)
    {
        case en_c_ll:
        case en_c_ull:
        case en_c_d:
        case en_c_ld:
        case en_c_f:
        case en_c_dc:
        case en_c_ldc:
        case en_c_fc:
        case en_c_di:
        case en_c_ldi:
        case en_c_fi:
        case en_c_i:
        case en_c_l:
        case en_c_ui:
        case en_c_ul:
        case en_c_c:
        case en_c_bool:
        case en_c_uc:
        case en_c_wc:
        case en_c_u16:
        case en_c_u32:
        case en_nullptr:
            break;
        case en_global:
        case en_pc:
        case en_label:
        case en_labcon:
        case en_const:
        case en_threadlocal:
            break;
        case en_auto:
            if (temp->v.sp->inlineFunc.stmt)
            {
                // guaranteed to be an lvalue at this point
                temp = ((EXPRESSION *)(temp->v.sp->inlineFunc.stmt));
                temp = inlineexpr(temp, fromlval);
                if (fromlval)
                    *fromlval = TRUE;
            }
            break;
        case en_l_sp:
        case en_l_fp:
        case en_bits:
        case en_l_f:
        case en_l_d:
        case en_l_ld:
        case en_l_fi:
        case en_l_di:
        case en_l_ldi:
        case en_l_fc:
        case en_l_dc:
        case en_l_ldc:
        case en_l_wc:
        case en_l_c:
        case en_l_s:
        case en_l_u16:
        case en_l_u32:
        case en_l_ul:
        case en_l_l:
        case en_l_p:
        case en_l_ref:        
        case en_l_i:
        case en_l_ui:
        case en_l_uc:
        case en_l_us:
        case en_l_bool:
        case en_l_bit:
        case en_l_ll:
        case en_l_ull:
            /*
            if (node->left->type == en_auto)
            {
                memcpy(temp, (EXPRESSION *)(node->left->v.sp->inlineFunc.stmt), sizeof(EXPRESSION));
//                temp->left = (EXPRESSION *)(node->left->v.sp->inlineFunc.stmt);
            }
            else
            */
            {
                BOOL lval = FALSE;
                temp->left = inlineexpr(temp->left, &lval);
                if (lval)
                    temp = temp->left;
            }
            break;
        case en_uminus:
        case en_compl:
        case en_not:
        case en_x_f:
        case en_x_d:
        case en_x_ld:
        case en_x_fi:
        case en_x_di:
        case en_x_ldi:
        case en_x_fc:
        case en_x_dc:
        case en_x_ldc:
        case en_x_ll:
        case en_x_ull:
        case en_x_i:
        case en_x_ui:
        case en_x_c:
        case en_x_uc:
        case en_x_u16:
        case en_x_u32:
        case en_x_wc:
        case en_x_bool:
        case en_x_bit:
        case en_x_s:
        case en_x_us:
        case en_x_l:
        case en_x_ul:
        case en_x_p:
        case en_x_fp:
        case en_x_sp:
        case en_trapcall:
        case en_shiftby:
/*        case en_movebyref: */
        case en_substack:
        case en_alloca:
        case en_loadstack:
        case en_savestack:
        case en_not_lvalue:
        case en_lvalue:
        case en_literalclass:
            temp->left = inlineexpr(node->left, FALSE);
            break;
        case en_autoinc:
        case en_autodec:
        case en_add:
        case en_structadd:
        case en_sub:
/*        case en_addcast: */
        case en_lsh:
        case en_arraylsh:
        case en_rsh:
        case en_rshd:
        case en_assign:
        case en_void:
        case en_voidnz:
/*        case en_dvoid: */
        case en_arraymul:
        case en_arrayadd:
        case en_arraydiv:
        case en_mul:
        case en_div:
        case en_umul:
        case en_udiv:
        case en_umod:
        case en_ursh:
        case en_mod:
        case en_and:
        case en_or:
        case en_xor:
        case en_lor:
        case en_land:
        case en_eq:
        case en_ne:
        case en_gt:
        case en_ge:
        case en_lt:
        case en_le:
        case en_ugt:
        case en_uge:
        case en_ult:
        case en_ule:
        case en_cond:
        case en_intcall:
        case en_stackblock:
        case en_blockassign:
        case en_mp_compare:
/*		case en_array: */
            temp->right = inlineexpr(node->right, FALSE);
        case en_mp_as_bool:
        case en_blockclear:
        case en_argnopush:
        case en_thisref:
            temp->left = inlineexpr(node->left, FALSE);
            break;
        case en_atomic:
            temp->v.ad->flg = inlineexpr(node->v.ad->flg, FALSE);
            temp->v.ad->memoryOrder1 = inlineexpr(node->v.ad->memoryOrder1, FALSE);
            temp->v.ad->memoryOrder2 = inlineexpr(node->v.ad->memoryOrder2, FALSE);
            temp->v.ad->address = inlineexpr(node->v.ad->address, FALSE);
            temp->v.ad->value = inlineexpr(node->v.ad->value, FALSE);
            temp->v.ad->third = inlineexpr(node->v.ad->third, FALSE);
            break;
        case en_func:
            temp->v.func = NULL;
            fp = node->v.func;
            if (fp->sp->linkage == lk_inline)
            {
                // check for recursion
                for (i=0; i <inlinesp_count; i++)
                {
                    if (inlinesp_list[i] == fp->sp)
                    {
                        break;
                    }
                }
            }
            if (fp->sp->linkage == lk_inline && i >= inlinesp_count)
            {
                if (inlinesp_count >= MAX_INLINE_NESTING)
                {
                    diag("inline sp queue too deep");
                }
                else
                {
                    inlinesp_list[inlinesp_count++] = fp->sp;
                    temp->v.func = doinline(fp, NULL); /* discarding our allocation */
                    inlinesp_count--;
                    
                }
            }
            if (temp->v.func == NULL)
            {
                INITLIST *args = fp->arguments;
                INITLIST **p ;
                temp->v.func = Alloc(sizeof(FUNCTIONCALL));
                *temp->v.func = *fp;
                p = &temp->v.func->arguments;
                *p = NULL;
                while (args)
                {
                    *p = Alloc(sizeof(INITLIST));
                    **p = *args;
                    (*p)->exp = inlineexpr((*p)->exp, FALSE);
                    args = args->next;
                    p = &(*p)->next;
                }
                if (temp->v.func->thisptr)
                    temp->v.func->thisptr = inlineexpr(temp->v.func->thisptr, FALSE);
            }
            break;
        case en_stmt:
            temp->v.stmt = inlinestmt(temp->v.stmt);
            temp->left = inlineexpr(temp->left, FALSE);
            break;
        default:
            diag("Invalid expr type in inlineexpr");
            break;
    }
    return temp;
}


/*-------------------------------------------------------------------------*/

STATEMENT *inlinestmt(STATEMENT *block)
{
    STATEMENT *out = NULL, **outptr = &out;
    while (block != NULL)
    {
        *outptr = (STATEMENT *)Alloc(sizeof(STATEMENT));
        memcpy(*outptr, block, sizeof(STATEMENT));
        (*outptr)->next = NULL;
        switch (block->type)
        {
            case st__genword:
                break;
            case st_try:
            case st_catch:
                (*outptr)->lower = inlinestmt(block->lower);
                (*outptr)->blockTail = inlinestmt(block->blockTail);
                break;
            case st_return:
            case st_expr:
            case st_declare:
                (*outptr)->select = inlineexpr(block->select, FALSE);
                break;
            case st_goto:
            case st_label:
                break;
            case st_select:
            case st_notselect:
                (*outptr)->select = inlineexpr(block->select, FALSE);
                break;
            case st_switch:
                (*outptr)->select = inlineexpr(block->select, FALSE);
                (*outptr)->lower = inlinestmt(block->lower);
                break;
            case st_block:
                (*outptr)->lower = inlinestmt(block->lower);
                (*outptr)->blockTail = inlinestmt(block->blockTail);
                break;
            case st_passthrough:
                if (block->lower)
                    if (chosenAssembler->inlineAsmStmt)
                        block->lower = (*chosenAssembler->inlineAsmStmt)(block->lower);
                break;
            case st_datapassthrough:
                break;
            case st_line:
            case st_varstart:
            case st_dbgblock:
                break;
            default:
                diag("Invalid block type in inlinestmt");
                break;
        }
        outptr = &(*outptr)->next;
        block = block->next;
    }
    return out;
}
static void inlineResetReturn(STATEMENT *block, TYPE *rettp, EXPRESSION *retnode)
{
    EXPRESSION *exp;
    if (isstructured(rettp))
    {
        diag("structure in inlineResetReturn");
    }
    else
    {
        exp = block->select;
        cast(rettp, &exp);
        exp = exprNode(en_assign, retnode, exp);
    }
    block->type = st_expr;
    block->select = exp;
}
static EXPRESSION *newReturn(TYPE *tp)
{
    EXPRESSION *exp ;
    if (!isstructured(tp) && !isvoid(tp))
    {
        exp = varNode(en_auto, anonymousVar(sc_auto, tp));
        deref(tp, &exp);
    }
    else
        exp = intNode(en_c_i, 0);
    return exp;
}
static void reduceReturns(STATEMENT *block, TYPE *rettp, EXPRESSION *retnode)
{
    while (block != NULL)
    {
        switch (block->type)
        {
            case st__genword:
                break;
            case st_try:
            case st_catch:
                reduceReturns(block->lower, rettp, retnode);
                break;
            case st_return:
                inlineResetReturn(block, rettp, retnode);
                break;
            case st_goto:
            case st_label:
                break;
            case st_expr:
/*			case st_functailexpr: */
            case st_declare:
            case st_select:
            case st_notselect:
                break;
            case st_switch:
                reduceReturns(block->lower, rettp, retnode);
                break;
            case st_block:
                reduceReturns(block->lower, rettp, retnode);
                /* skipping block tail as it will have no returns  */
                break;
            case st_passthrough:
            case st_datapassthrough:
                break;
            case st_line:
            case st_varstart:
            case st_dbgblock:
                break;
            default:
                diag("Invalid block type in reduceReturns");
                break;
        }
        block = block->next;
    }
}
static EXPRESSION *scanReturn(STATEMENT *block, TYPE *rettp)
{
    EXPRESSION *rv = NULL;
    while (block != NULL && !rv)
    {
        switch (block->type)
        {
            case st__genword:
                break;
            case st_try:
            case st_catch:
                rv = scanReturn(block->lower, rettp);
                break;
            case st_return:
                rv = block->select;
                cast(rettp, &rv);
                block->type = st_expr;
                block->select = rv;
                return rv;
            case st_goto:
            case st_label:
                break;
            case st_expr:
/*			case st_functailexpr: */
            case st_declare:
            case st_select:
            case st_notselect:
                break;
            case st_switch:
                rv = scanReturn(block->lower, rettp);
                break;
            case st_block:
                rv = scanReturn(block->lower, rettp);
                /* skipping block tail as it will have no returns  */
                break;
            case st_passthrough:
            case st_datapassthrough:
                break;
            case st_line:
            case st_varstart:
            case st_dbgblock:
                break;
            default:
                diag("Invalid block type in scanReturn");
                break;
        }
        block = block->next;
    }
    return rv;
}

/*-------------------------------------------------------------------------*/
static BOOL sideEffects(EXPRESSION *node)
{
    BOOL rv = FALSE;
    if (node == 0)
        return rv;
    switch (node->type)
    {
        case en_c_ll:
        case en_c_ull:
        case en_c_d:
        case en_c_ld:
        case en_c_f:
        case en_c_dc:
        case en_c_ldc:
        case en_c_fc:
        case en_c_di:
        case en_c_ldi:
        case en_c_fi:
        case en_c_i:
        case en_c_l:
        case en_c_ui:
        case en_c_ul:
        case en_c_c:
        case en_c_bool:
        case en_c_uc:
        case en_c_wc:
        case en_c_u16:
        case en_c_u32:
        case en_nullptr:
            rv = FALSE;
            break;
        case en_global:
        case en_pc:
        case en_threadlocal:
        case en_label:
        case en_labcon:
        case en_const:
        case en_auto:
            rv = FALSE;
            break;
        case en_l_sp:
        case en_l_fp:
        case en_bits:
        case en_l_f:
        case en_l_d:
        case en_l_ld:
        case en_l_fi:
        case en_l_di:
        case en_l_ldi:
        case en_l_fc:
        case en_l_dc:
        case en_l_ldc:
        case en_l_wc:
        case en_l_c:
        case en_l_s:
        case en_l_u16:
        case en_l_u32:
        case en_l_ul:
        case en_l_l:
        case en_l_p:
        case en_l_ref:        
        case en_l_i:
        case en_l_ui:
        case en_l_uc:
        case en_l_us:
        case en_l_bool:
        case en_l_bit:
        case en_l_ll:
        case en_l_ull:
        case en_literalclass:
            rv = sideEffects(node->left);
            break;
        case en_uminus:
        case en_compl:
        case en_not:
        case en_x_f:
        case en_x_d:
        case en_x_ld:
        case en_x_fi:
        case en_x_di:
        case en_x_ldi:
        case en_x_fc:
        case en_x_dc:
        case en_x_ldc:
        case en_x_ll:
        case en_x_ull:
        case en_x_i:
        case en_x_ui:
        case en_x_c:
        case en_x_uc:
        case en_x_u16:
        case en_x_u32:
        case en_x_wc:
        case en_x_bool:
        case en_x_bit:
        case en_x_s:
        case en_x_us:
        case en_x_l:
        case en_x_ul:
        case en_x_p:
        case en_x_fp:
        case en_x_sp:
        case en_shiftby:
/*        case en_movebyref: */
        case en_not_lvalue:
        case en_lvalue:
            rv = sideEffects(node->left);
            break;
        case en_substack:
        case en_alloca:
        case en_loadstack:
        case en_savestack:
        case en_assign:
        case en_autoinc:
        case en_autodec:
        case en_trapcall:
            rv = TRUE;
            break;
        case en_add:
        case en_sub:
/*        case en_addcast: */
        case en_lsh:
        case en_arraylsh:
        case en_rsh:
        case en_rshd:
        case en_void:
        case en_voidnz:
/*        case en_dvoid: */
        case en_arraymul:
        case en_arrayadd:
        case en_arraydiv:
        case en_structadd:
        case en_mul:
        case en_div:
        case en_umul:
        case en_udiv:
        case en_umod:
        case en_ursh:
        case en_mod:
        case en_and:
        case en_or:
        case en_xor:
        case en_lor:
        case en_land:
        case en_eq:
        case en_ne:
        case en_gt:
        case en_ge:
        case en_lt:
        case en_le:
        case en_ugt:
        case en_uge:
        case en_ult:
        case en_ule:
        case en_cond:
        case en_intcall:
        case en_stackblock:
        case en_blockassign:
        case en_mp_compare:
/*		case en_array: */
            rv = sideEffects(node->right);
        case en_mp_as_bool:
        case en_blockclear:
        case en_argnopush:
        case en_thisref:
            rv |= sideEffects(node->left);
            break;
        case en_atomic:
            rv = sideEffects(node->v.ad->flg);
            rv |= sideEffects(node->v.ad->memoryOrder1);
            rv |= sideEffects(node->v.ad->memoryOrder2);
            rv |= sideEffects(node->v.ad->address);
            rv |= sideEffects(node->v.ad->value);
            rv |= sideEffects(node->v.ad->third);
            break;
        case en_func:
            rv = TRUE;
            break;
        case en_stmt:
            rv = TRUE;
            break;
        default:
            diag("sideEffects");
            break;
    }
    return rv;
}
static void setExp(SYMBOL *sx, EXPRESSION *exp, STATEMENT ***stp)
{
    if (!sx->altered && !sx->addressTaken && !sideEffects(exp))
    {
        // well if the expression is too complicated it gets evaluated over and over
        // but maybe the backend can clean it up again...
        sx->inlineFunc.stmt = (STATEMENT *)exp;
    }
    else
    {
        EXPRESSION *tnode = varNode(en_auto, anonymousVar(sc_auto, sx->tp));
        sx->inlineFunc.stmt = (STATEMENT *)tnode;
        deref(sx->tp, &tnode);
        tnode = exprNode(en_assign, tnode, exp);
        **stp = Alloc(sizeof(STATEMENT));
        memset(**stp, 0 , sizeof(STATEMENT));
        (**stp)->type = st_expr;
        (**stp)->select = tnode;
        *stp = &(**stp)->next;
    }
}
static STATEMENT *SetupArguments(FUNCTIONCALL *params)
{
        
    STATEMENT *st = NULL, **stp = &st;
    INITLIST *al = params->arguments;
    HASHREC *hr = params->sp->inlineFunc.syms->table[0];
    if (ismember(params->sp))
    {
        SYMBOL *sx = (SYMBOL *)hr->p;
        setExp(sx, params->thisptr, &stp);
        hr = hr->next;
    }
    while (al && hr)
    {
        SYMBOL *sx = (SYMBOL *)hr->p;
        setExp(sx, al->exp, &stp);
        al = al->next;
        hr = hr->next;
    }
    return st;
}
/*-------------------------------------------------------------------------*/

void SetupVariables(SYMBOL *sp)
/* Copy all the func args into the xsyms table.
 * This copies the function parameters twice...
 */
{
    HASHTABLE *syms = sp->inlineFunc.syms;
    while (syms)
    {
        HASHREC *hr = syms->table[0];
        while (hr)
        {
            SYMBOL *sx = (SYMBOL *)hr->p;
            if (sx->storage_class == sc_auto)
            {
                SYMBOL *sxnew = anonymousVar(sc_auto, sx->tp);
                EXPRESSION *ev = varNode(en_auto, sxnew);
                deref(sx->tp, &ev);
                sx->inlineFunc.stmt = (STATEMENT *)ev;
            }
            hr = hr->next;
        }
        syms = syms->next;
    }
}
/*-------------------------------------------------------------------------*/

EXPRESSION *doinline(FUNCTIONCALL *params, SYMBOL *funcsp)
{
    STATEMENT *stmt = NULL, **stp = &stmt, *stmt1;
    EXPRESSION *newExpression;
    BOOL allocated = FALSE;
    if (!isfunction(params->functp))
        return NULL;
    if (params->sp->linkage != lk_inline)
        return NULL;
    if (params->sp->noinline)
        return NULL;
    if (!params->sp->inlineFunc.syms)
        return NULL;
    if (!params->sp->inlineFunc.stmt)
    {
        // recursive...
        params->sp->linkage = lk_cdecl;
        return NULL;
    }
    if (!localNameSpace->syms)
    {
        allocated = TRUE;
        AllocateLocalContext(NULL, NULL);
    }
    stmt1 = SetupArguments(params);
    if (stmt1)
    {
        // this will kill the ret val but we don't care since we've modified params
        stmt = Alloc(sizeof(STATEMENT));
        stmt->type = st_block;
        stmt->lower = stmt1;
    }
    SetupVariables(params->sp);

    while (*stp)
        stp = &(*stp)->next;
    *stp = inlinestmt(params->sp->inlineFunc.stmt);
    newExpression = exprNode(en_stmt, NULL, NULL);
    newExpression->v.stmt = stmt;
    
    if (params->sp->retcount == 1)
    {
        /* optimization for simple inline functions that only have
         * one return statement, don't save to an intermediate variable
         */
        scanReturn(stmt, basetype(params->sp->tp)->btp);
    }
    else
    {
        newExpression->left = newReturn(basetype(params->sp->tp)->btp);
        reduceReturns(stmt, params->sp->tp->btp, newExpression->left);
    }
    optimize_for_constants(&newExpression->left);
    if (allocated)
    {
        FreeLocalContext(NULL, NULL);
    }
    if (newExpression->type == en_stmt)
        if (newExpression->v.stmt->type == st_block)
            if (!newExpression->v.stmt->lower)
                newExpression = intNode(en_c_i, 0); // noop if there is no body
    return newExpression;
}
static BOOL IsEmptyBlocks(STATEMENT *block)
{
    BOOL rv = TRUE;
    while (block != NULL && rv)
    {
        switch (block->type)
        {
            case st_line:
            case st_varstart:
            case st_dbgblock:
                break;
            case st__genword:
            case st_try:
            case st_catch:
            case st_return:
            case st_goto:
            case st_expr:
            case st_declare:
            case st_select:
            case st_notselect:
            case st_switch:
            case st_passthrough:
            case st_datapassthrough:
                rv = FALSE;
                break;
            case st_label:
                break;
            case st_block:
                rv = IsEmptyBlocks(block->lower) && block->blockTail == NULL;
                break;
            default:
                diag("Invalid block type in IsEmptyBlocks");
                break;
        }
        block = block->next;
    }
    return rv;
}
BOOL IsEmptyFunction(FUNCTIONCALL *params, SYMBOL *funcsp)
{
    STATEMENT *st;
    if (!isfunction(params->functp))
        return FALSE;
    if (!params->sp->inlineFunc.stmt)
        return FALSE;
    st = params->sp->inlineFunc.stmt;
    while (st && st->type == st_expr)
    {
        st = st->next;
    }
    if (!st)
        return TRUE;
    return TRUE || IsEmptyBlocks(st);
    
}
EXPRESSION *EvaluateConstFunction(FUNCTIONCALL *params, SYMBOL *funcsp)
{
    static SYMBOL *curfunc;
    STATEMENT *stmt = NULL, **stp = &stmt;
    EXPRESSION *newExpression;
    BOOL allocated = FALSE;
    if (!isfunction(params->functp))
        return NULL;
    if (!params->sp->inlineFunc.syms)
        return NULL;

    AllocateLocalContext(NULL, NULL);
    stmt = SetupArguments(params);
    SetupVariables(params->sp);

    while (*stp)
        stp = &(*stp)->next;
    *stp = inlinestmt(params->sp->inlineFunc.stmt);
    newExpression = exprNode(en_stmt, NULL, NULL);
    newExpression->v.stmt = stmt;
    
    if (params->sp->retcount == 1)
    {
        /* optimization for simple inline functions that only have
         * one return statement, don't save to an intermediate variable
         */
        scanReturn(stmt, basetype(params->sp->tp)->btp);
        optimize_for_constants(&newExpression);
        if (!IsConstantExpression(newExpression, FALSE))
        {
            newExpression = NULL;
            error(ERR_CONSTANT_FUNCTION_EXPECTED);
        }
    }
    else if (params->sp->retcount == 0)
    {
        optimize_for_constants(&newExpression);
        if (!IsConstantExpression(newExpression, FALSE))
        {
            newExpression = NULL;
            error(ERR_CONSTANT_FUNCTION_EXPECTED);
        }
    }
    else
    {
        newExpression = NULL;
        error(ERR_CONSTANT_FUNCTION_EXPECTED);
    }
    FreeLocalContext(NULL, NULL);
    return newExpression;
}