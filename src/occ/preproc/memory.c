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

#include "compiler.h"

static MEMBLK* freemem;
static MEMBLK* freestdmem;
static MEMBLK* globals;
static MEMBLK* locals;
static MEMBLK* opts;
static MEMBLK* alias;
static MEMBLK* temps;
static MEMBLK* live;

static int globalFlag;
static int globalPeak, localPeak, optPeak, tempsPeak, aliasPeak, livePeak;

#define MINALLOC (4 * 1024)
#define MALIGN (4)

//#define DEBUG

void mem_summary(void)
{
    printf("Memory used:\n");
    printf("\tGlobal Peak %dK\n", (globalPeak + 1023) / 1024);
    printf("\tLocal peak %dK\n", (localPeak + 1023) / 1024);
    printf("\tOptimizer peak %dK\n", (optPeak + 1023) / 1024);
    printf("\tTemporary peak %dK\n", (tempsPeak + 1023) / 1024);
    printf("\tAlias peak %dK\n", (aliasPeak + 1023) / 1024);
    printf("\tLive peak %dK\n", (livePeak + 1023) / 1024);
    globalPeak = localPeak = optPeak = tempsPeak = aliasPeak = livePeak = 0;
}
static MEMBLK* galloc(MEMBLK** arena, int size)
{
    MEMBLK* selected;
    int allocsize = size <= MINALLOC ? MINALLOC : (size + (MINALLOC - 1)) & -MINALLOC;
    selected = NULL;
    if (allocsize == MINALLOC)
    {
        if (freestdmem)
        {
            selected = freestdmem;
            freestdmem = freestdmem->next;
            selected->left = selected->size;
        }
    }
    else
    {
        MEMBLK** free = &freemem;
        while (*free)
        {
            if (((*free)->size) >= allocsize)
            {
                selected = *free;
                *free = (*free)->next;
                selected->left = selected->size;
                break;
            }
            free = &(*free)->next;
        }
    }
    if (!selected)
    {
        selected = (MEMBLK*)malloc(allocsize + sizeof(MEMBLK) - 1);
        if (!selected)
            fatal("out of memory");
        selected->size = selected->left = allocsize;
    }
#ifdef DEBUG
    memset(selected->m, 0xc4, selected->left);
#else
    memset(selected->m, 0, selected->left);
#endif
    selected->next = *arena;
    *arena = selected;
    return selected;
}
void* memAlloc(MEMBLK** arena, int size)
{
    MEMBLK* selected = *arena;
    void* rv;
    if (!selected || selected->left < size)
    {
        selected = galloc(arena, size);
    }
    rv = (void*)(selected->m + selected->size - selected->left);
#ifdef DEBUG
    memset(rv, 0, size);
#endif
    selected->left = selected->left - ((size + MALIGN - 1) & -MALIGN);
    return rv;
}
void memFree(MEMBLK** arena, int* peak)
{
    MEMBLK* freefind = *arena;
    long size = 0;
    if (!freefind)
        return;
    while (freefind)
    {
        MEMBLK* next = freefind->next;
#ifdef DEBUG
        memset(freefind->m, 0xc4, freefind->size);
#endif
        size += freefind->size;
        if (freefind->size == MINALLOC)
        {
            freefind->next = freestdmem;
            freestdmem = freefind;
        }
        else
        {
            freefind->next = freemem;
            freemem = freefind;
        }
        freefind = next;
    }
    *arena = 0;
    if (size > *peak)
        *peak = size;
}
void* globalAlloc(int size) { return memAlloc(&globals, size); }
void globalFree(void)
{
    memFree(&globals, &globalPeak);
    globalFlag = 1;
}
void* localAlloc(int size) { return memAlloc(&locals, size); }
void localFree(void) { memFree(&locals, &localPeak); }
void* Alloc(int size)
{
    if (!globalFlag)
        return memAlloc(&locals, size);
    else
        return memAlloc(&globals, size);
}
void* oAlloc(int size) { return memAlloc(&opts, size); }
void oFree(void) { memFree(&opts, &optPeak); }
void* aAlloc(int size) { return memAlloc(&alias, size); }
void aFree(void) { memFree(&alias, &aliasPeak); }
void* tAlloc(int size) { return memAlloc(&temps, size); }
void tFree(void) { memFree(&temps, &tempsPeak); }
void* sAlloc(int size) { return memAlloc(&live, size); }
void sFree(void) { memFree(&live, &livePeak); }
void IncGlobalFlag(void) { globalFlag++; }
void DecGlobalFlag(void) { globalFlag--; }
void SetGlobalFlag(int flag) { globalFlag = flag; }
int GetGlobalFlag(void) { return globalFlag; }
char* litlate(char* name)
{
    int l;
    char* rv = Alloc((l = strlen(name)) + 1);
    memcpy(rv, name, l);
    return rv;
}
LCHAR* wlitlate(LCHAR* name)
{
    LCHAR* p = name;
    int count = 0;
    LCHAR* rv;
    while (*p)
        p++, count++;
    IncGlobalFlag();
    rv = (LCHAR*)Alloc((count + 1) * sizeof(LCHAR));
    DecGlobalFlag();
    p = name;
    count = 0;
    while (*p)
        rv[count++] = *p++;
    rv[count] = 0;
    return rv;
}