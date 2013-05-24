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
#include "RegExp.h"
#include <limits.h>

BYTE RegExpMatch::wordChars[256/8];
bool RegExpMatch::initted;

void RegExpMatch::Init(bool caseSensitive)
{
    if (!initted)
    {
        memset(wordChars,0, sizeof(wordChars));
        for (int i='A'; i <= 'Z'; i++)
            wordChars[i/8] |= 1 << (i & 7);
        if (caseSensitive)
            for (int i='a'; i <= 'z'; i++)
                wordChars[i/8] |= 1 << (i & 7);
        for (int i='0'; i <= '9'; i++)
            wordChars[i/8] |= 1 << (i & 7);
        wordChars['_'/8] |= 1 << ('_' & 7);
        initted = true;
    }
}
const char * RegExpMatch::SetClass(const char *name)
{
    int i;
    if (!strcmp(name, "alpha"))
    {
        for (i=0; i < 128; i++)
            if (isalpha(i))
                SetBit(i);
        name += 5;
    }
    else if (!strcmp(name, "upper"))
    {
        for (i=0; i < 128; i++)
            if (isupper(i))
                SetBit(i);
        name += 5;
    }
    else if (!strcmp(name, "lower"))
    {
        for (i=0; i < 128; i++)
            if (islower(i))
                SetBit(i);
        name += 5;
    }
    else if (!strcmp(name, "digit"))
    {
        for (i=0; i < 128; i++)
            if (isdigit(i))
                SetBit(i);
        name += 5;
    }
    else if (!strcmp(name, "alnum"))
    {
        for (i=0; i < 128; i++)
            if (isalnum(i))
                SetBit(i);
        name += 5;
    }
    else if (!strcmp(name, "xdigit"))
    {
        for (i=0; i < 128; i++)
            if (isxdigit(i))
                SetBit(i);
        name += 6;
    }
    else if (!strcmp(name, "space"))
    {
        for (i=0; i < 128; i++)
            if (isspace(i))
                SetBit(i);
        name += 5;
    }
    else if (!strcmp(name, "print"))
    {
        for (i=0; i < 128; i++)
            if (isprint(i))
                SetBit(i);
        name += 5;
    }
    else if (!strcmp(name, "punct"))
    {
        for (i=0; i < 128; i++)
            if (ispunct(i))
                SetBit(i);
        name += 5;
    }
    else if (!strcmp(name, "graph"))
    {
        for (i=0; i < 128; i++)
            if (isgraph(i))
                SetBit(i);
        name += 5;
    }
    else if (!strcmp(name, "cntrl"))
    {
        for (i=0; i < 128; i++)
            if (iscntrl(i))
                SetBit(i);
        name += 5;
    }
    else if (!strcmp(name, "blank"))
    {
        for (i=0; i < 128; i++)
            if (i == ' ' || i == '\t')
                SetBit(i);
        name += 5;
    }
    else
        invalid = true;
    return name;
}
void RegExpMatch::SetSet(const char **p, bool caseSensitive)
{
    bool not = false;
    BYTE last[256/8];
    const char *str = *p;
    memcpy(last, matches, sizeof(last));
    memset(matches, 0, sizeof(matches));
    if (*str == '^')
    {
        not = true;
        str++;
    }
    if (*str == '-')
    {
        SetBit('-');
        str++;	
    }
    while (!invalid && *str && *str != ']')
    {
        if (*str == '[' && str[1] == ':')
        {
            str = SetClass(str+2);
            if (*str != ':' || str[1] == ']')
                invalid = true;	
            str += 2;
        }
        if (*str == '\\')
        {
            str++;
            if (!*str)
            {
                invalid = true;
            }
            else
                SetBit(*str);
            str++;
        }
        else
            if (str[1] == '-')
            {
                if (str[0] > str[2])
                    invalid = true;
                else
                {
                    SetRange(str[0], str[2], caseSensitive);
                    str += 3;
                }
            }
            else
                SetChar(*str++, caseSensitive);
    }
    if (*str != ']')
        invalid = true;
    else
        str++;
    if (!invalid && not)
    {
        for (int i = 32/8; i < 128/8; i++)
            matches[i] = ~matches[i] &last[i];
    }
    *p = str;
}
bool RegExpMatch::MatchRange(RegExpContext &context ,const char *str)
{
    if (IsSet(M_START))
    {
        if (context.matchCount < 10)
        {
            context.matchStack[context.matchStackTop++] = context.matchCount;
            context.matchOffsets[context.matchCount][0] = str - context.beginning;
            context.matchOffsets[context.matchCount++][1] = 0;
            return true;
        }
    }
    if (IsSet(M_END))
    {
        if (context.matchStackTop)
        {
            context.matchOffsets[context.matchStack[--context.matchStackTop]][1] = str - context.beginning;
            return true;
        }
    }
    return false;
}
int RegExpMatch::MatchOne(RegExpContext &context, const char *str)
{
    if (MatchRange(context, str))
        return 0;
    if (*str && IsSet(*str))
        return 1;
    if (IsSet(M_MATCH))
    {
        if (matchRange < context.matchCount)
        {
            int n = context.matchOffsets[matchRange][1] - context.matchOffsets[matchRange][0];
            if (context.caseSensitive)
            {
                if (!strncmp(str, context.beginning + context.matchOffsets[matchRange][0], n))
                    return n;
            }
            else
            {
                bool matches = true;
                const char *p = context.beginning + context.matchOffsets[matchRange][0];
                const char *q = str;
                
                for (int i=0; i < n && matches; i++)
                    if (toupper(*p++) != toupper(*q++))
                        matches = false; 
                if (matches)
                    return n;
            }
        }
    }
    if (IsSet(RE_M_WORD))
    {
        if (IsSet(wordChars, *str) && !IsSet(wordChars, str[-1]))
        {
            return 0;
        }
        if (!IsSet(wordChars, *str) && IsSet(wordChars, str[-1]))
        {
            return 0;
        }
    }
    if (IsSet(RE_M_IWORD))
    {
        if (IsSet(wordChars, *str))
            if (IsSet(wordChars, str[-1]))
                if (IsSet(wordChars, str[1]))
                    return 0;
    }
    if (IsSet(RE_M_BWORD))
    {
        if (IsSet(wordChars, *str))
            if (!IsSet(wordChars, str[-1]))
            {
                return 0;
            }
    }
    if (IsSet(RE_M_EWORD))
    {
        if (!IsSet(wordChars, *str))
            if (IsSet(wordChars, str[-1]))
            {
                return 0;
            }
    }
    if (IsSet(RE_M_WORDCHAR))
    {
        if (IsSet(wordChars, *str))
        {
            return 1;
        }
    }
    if (IsSet(RE_M_NONWORDCHAR))
    {
        if (!IsSet(wordChars, *str))
        {
            return 1;
        }
    }
    if (IsSet(RE_M_BBUFFER))
        if (str == context.beginning)
            return 0;
    if (IsSet(RE_M_EBUFFER))
        if (!*str)
            return 0;
    if (IsSet(RE_M_SOL))
    {
        if (str == context.beginning)
            return 0;
        if (str[-1] == '\n')
            return 0;
    }
    if (IsSet(RE_M_EOL))
    {
        if (*str == '\n' || !*str)
            return 0;	
    }
    return -1;
}
int RegExpMatch::Matches(RegExpContext &context, const char *str)
{
    if (rl >= 0 && rh >= 0)
    {
        int n,m = 0;
        int count = 0;
        n = MatchOne(context, str);
        while (n > 0)
        {
            m += n;
            n = MatchOne(context, str + m);
            count ++;
        }
        if (count >= rl && count <= rh)
        {
            if (count != 0 || MatchOne(context,str-1) == -1)
                return m;
        }
        if (m)
            return -m;
        else
            return -1;
    }
    else
        return MatchOne(context, str);
;
}
void RegExpContext::Clear()
{
    while (matches.size())
    {
        RegExpMatch *m = matches.front();
        matches.pop_front();
        delete m;
    }
    matchStackTop = 0; 
    m_so = m_eo = 0;
    matchCount = 0;
}
int RegExpContext::GetSpecial(char ch)
{
    switch(ch)
    {
        case 'b':
            return RegExpMatch::RE_M_WORD;
        case 'B':
            return RegExpMatch::RE_M_IWORD;
        case 'w':
            return RegExpMatch::RE_M_BWORD;
        case 'W':
            return RegExpMatch::RE_M_EWORD;
        case '<':
            return RegExpMatch::RE_M_WORDCHAR;
        case '>':
            return RegExpMatch::RE_M_NONWORDCHAR;
        case '`':
            return RegExpMatch::RE_M_BBUFFER;
        case '\'':
            return RegExpMatch::RE_M_EBUFFER;
        default:
            return ch;
    }
}
void RegExpContext::Parse(const char *exp, bool regular, bool CaseSensitive, bool matchesWord)
{
    invalid = false;
    caseSensitive = CaseSensitive;
    Clear();
    if (matchesWord)
    {
        RegExpMatch *m = new RegExpMatch(RegExpMatch::RE_M_BWORD, caseSensitive);
        if (m)
            matches.push_back(m);
        else
            invalid = true;
    }
    if (regular)
    {
        RegExpMatch *lastMatch = NULL;
        while (!invalid && *exp)
        {
            RegExpMatch *currentMatch = NULL;
            switch(*exp)
            {
                case '.':
                    currentMatch = new RegExpMatch(true);
                    if (!currentMatch)
                        invalid = true;
                    exp++;
                    break;
                case '*':
                    if (lastMatch)
                    {
                        lastMatch->SetInterval(0, INT_MAX);
                        lastMatch = NULL;
                    }
                    else 
                        invalid = true;
                    exp++;
                    break;
                case '+':
                    if (lastMatch)
                    {
                        lastMatch->SetInterval(1, INT_MAX);
                        lastMatch = NULL;
                    }
                    else 
                        invalid = true;
                    exp++;
                    break;
                case '?':
                    if (lastMatch)
                    {
                        lastMatch->SetInterval(0,1);
                        lastMatch = NULL;
                    }
                    else 
                        invalid = true;
                    exp++;
                    break;
                case '[':
                    currentMatch = new RegExpMatch(&exp, caseSensitive);
                    invalid = !currentMatch->IsValid();
                    break;
                case '^':
                    currentMatch = new RegExpMatch(RegExpMatch::RE_M_SOL, caseSensitive);
                    if (!currentMatch)
                        invalid = true;
                    exp++;
                    break;
                case '$':
                    currentMatch = new RegExpMatch(RegExpMatch::RE_M_EOL, caseSensitive);
                    if (!currentMatch)
                        invalid = true;
                    exp++;
                    break;
                case '\\':
                    switch (*++exp)
                    {
                        case '{':
                            exp++;
                            if (!lastMatch || !isdigit(*exp))
                            {
                                invalid = true;
                            }
                            else
                            {
                                int n1, n2;
                                n1 = n2 = atoi(exp);
                                while (isdigit(*exp))
                                    exp++;
                                if (*exp == ',')
                                {
                                    exp++;
                                    if (!isdigit(*exp))
                                        invalid = true;
                                    else
                                    {
                                        n2 = atoi(exp);
                                        while (isdigit(*exp))
                                            exp++;
                                    }
                                }
                                if (*exp != '\\' || exp[1] != '}' || n2 < n1)
                                    invalid = true;
                                else
                                {
                                    exp += 2;
                                    lastMatch->SetInterval(n1, n2);
                                }
                            }
                            break;
                        case '(':
                            exp++;
                            if (matchCount >= 10)
                                invalid = true;
                            else
                            {
                                matchStack[matchStackTop++] = matchCount;
                                currentMatch = new RegExpMatch(RegExpMatch::M_START, matchCount++);
                                if (!currentMatch)
                                    invalid = true;
                            }
                            break;
                        case ')':
                            exp++;
                            if (!matchStackTop)
                            {
                                invalid = true;
                            }
                            else
                            {
                                currentMatch = new RegExpMatch(RegExpMatch::M_END, matchStack[-matchStackTop]);
                                if (!currentMatch)
                                    invalid = true;
                            }
                            break;
                        default:
                            if (isdigit (*exp))
                            {
                                currentMatch = new RegExpMatch(RegExpMatch::M_MATCH, *exp++-'0', caseSensitive);
                                if (!currentMatch)
                                    invalid = true;
                            }
                            else
                            {
                                currentMatch = new RegExpMatch(GetSpecial(*exp++), caseSensitive);
                                if (!currentMatch)
                                    invalid = true;
                            }
                            break;
                    }
                    break;
                case '|':
                    if (!lastMatch)
                    {
                        invalid = true;
                    }
                    else
                    {
                        switch (*++exp)
                        {
                            case '\\':
                                exp++;
                                lastMatch->SetChar(GetSpecial(*exp++), caseSensitive);
                                lastMatch = NULL;
                                break;
                            case '[':
                                exp++;
                                lastMatch->SetSet(&exp, caseSensitive);
                                if (!lastMatch->IsValid())
                                    invalid = true;
                                break;
                            default:
                                lastMatch->SetChar(*exp++, caseSensitive);
                                break;
                        }
                    }
                    lastMatch = NULL;
                    break;				
                default:
                    currentMatch = new RegExpMatch(*exp++, caseSensitive);
                    if (!currentMatch)
                        invalid = true;
                    break;
            }
            if (currentMatch)
                matches.push_back(currentMatch);
            lastMatch = currentMatch;
        }
    }
    else
    {
        while (*exp)
        {
            RegExpMatch *m = new RegExpMatch(*exp++, caseSensitive);
            matches.push_back(m);
        }
    }
    if (!invalid && matchesWord)
    {
        RegExpMatch *m = new RegExpMatch(RegExpMatch::RE_M_EWORD, caseSensitive);
        if (m)
            matches.push_back(m);
        else
            invalid = true;
    }
}
int RegExpContext::MatchOne(const char *str)
{
    int m = 0;
    RegExpMatch::Reset(caseSensitive);
    matchCount = 0;
    matchStackTop = 0;
    for (std::deque<RegExpMatch *>::iterator it = matches.begin(); it != matches.end(); ++it)
    {
        int n = (*it)->Matches(*this, str + m);
        if (n < 0)
            return n;
        m += n;
    }
    return m;
}
bool RegExpContext::Match(int start, int len, const char *Beginning)
{
    beginning = Beginning;
    const char *str = Beginning + start;
    const char *end = str + len;
    matchStackTop = 0;
    while (*str && str < end)
    {
        int n = MatchOne(str);
        if (n >= 0)
        {
            m_so = str - beginning;
            m_eo = str - beginning + n;
            return true;
        }
        else
        {
            str+=-n;
        }
    }
    return false;
}
