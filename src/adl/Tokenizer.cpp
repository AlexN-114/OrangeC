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
#include "Parser.h"
#include "TokenNode.h"
#include <ctype.h>

int TokenNode::tk_next;
int TokenNode::tn_next = 1;
std::map<std::string, int> TokenNode::tokenTable;
std::deque<TokenNode *> TokenNode::tokenList;

bool Parser::CreateParseTree()
{
    if (LoadRegisterClasses())
    {
        if (LoadAddressClasses())
        {
            if (LoadAddresses())
            {
                if (LoadOperands())
                {
                    return true;
                }
            }
        }
    }
    return false;
}

void Parser::EnterRegisterClasses(Register *reg)
{
    std::string temp = reg->cclass;
    while (temp.size())
    {
        int npos = temp.find_first_not_of(", \t");
        if (npos == std::string::npos)
            break;
        int npos1 = temp.find_first_of(", \t", npos+1);
        if (npos1 == std::string::npos)
        {
            npos1 = temp.size();
        }
        std::string cclass = temp.substr(npos, npos1-npos);
        temp.replace(0,npos1,"");
        RegClass *p = registerClasses[cclass];
        if (!p)
        {
            p = new RegClass(cclass, registers.size());
            p->id = registerClasses.size();
            registerClasses[cclass] = p;
        }
        p->regs[reg->id/8] |= (1 << (reg->id & 7));
    }
}
bool Parser::LoadRegisterClasses()
{
    for (std::deque<Register *>::iterator it = registers.begin(); it != registers.end();
         ++it)
    {
        EnterRegisterClasses(*it);
    }
    return true;
}
void Parser::EnterAddressClasses(Address *address)
{
    std::string temp = address->cclass;
    while (temp.size())
    {
        int npos = temp.find_first_not_of(", \t");
        if (npos == std::string::npos)
            break;
        int npos1 = temp.find_first_of(", \t", npos+1);
        if (npos1 == std::string::npos)
        {
            npos1 = temp.size();
        }
        std::string cclass = temp.substr(npos, npos1-npos);
        temp.replace(0,npos1,"");
        AddressClass *p = addressClasses[cclass];
        if (!p)
        {
            p = new AddressClass(cclass);
            p->id = addressClasses.size();
            addressClasses[cclass] = p;
        }
    }
}
bool Parser::LoadAddressClasses()
{
    for (std::deque<Address *>::iterator it = addresses.begin(); it != addresses.end();
         ++it)
    {
        EnterAddressClasses(*it);
    }
    return true;
}

// todo, support a TOKEN field in the ADL file for compound tokens...
bool Parser::LoadAddressTokens(std::string name, std::deque<TokenNode *> &nodes)
{
    int beginLevel = 0;
    bool rv= true;
    std::string expr = name;
    while (name.size())
    {
        int npos = name.find_first_not_of(" \n\r\v\t");
        if (npos == std::string::npos)
        {
            name = "";
        }
        else 
        {
            TokenNode *token = NULL;
            if (npos != 0)
                name = name.substr(npos);
            if (isalpha(name[0]))	// may need to be adjusted for architectures that have non-alpha name start chars
            {
                int i;
                for (i=0; i < name.size(); i++)
                {
                    if (!isalnum(name[i]))
                        break;
                }
                std::string label;
                label = name.substr(0, i);
                name = name.substr(i);
                for (std::deque<Register *>::iterator it = registers.begin(); it != registers.end(); ++it)
                {
                    if ((*it)->name == label)
                    {
                        token = new TokenNode((*it), beginLevel);
                        break;
                    }
                }
                if (!token)
                {
                    token = new TokenNode(label, beginLevel);
                }
            }
            else if (name[0] == '\'')
            {
                int npos = name.find_first_of("'", 1);
                std::string var, label;
                if (npos != std::string::npos)
                {
                    label = name.substr(1, npos-1);
                    name = name.substr(npos + 1);
                    npos = label.find_first_of(":");
                    if (npos == std::string::npos)
                    {
                        std::cout << "Address Error { " << expr << " } Register class ' " << label << " ' does not have a variable." << std::endl;
                    }
                    else
                    {
                        var = label.substr(0, npos);
                        label = label.substr(npos+1);
                    }
                }
                std::map<std::string, RegClass *>::iterator it = registerClasses.find(label);
                if (it != registerClasses.end())
                {
                    token = new TokenNode(it->second, beginLevel);
                    token->name = var;
                }
                else
                {
                    bool found = false;
                    for (std::deque<Number *>::iterator it = numbers.begin(); it != numbers.end(); ++ it)
                    {
                        if ((*it)->name == label)
                        {
                            found = true;
                            token = new TokenNode(*it, beginLevel);
                            token->name = var;
                        }
                    }
                    if (!found)
                    {
                        std::cout << "Address Error { " << expr << " } Register class ' " << label << " ' Not found." << std::endl;
                        rv = false;
                    }
                }
            }
            else if (name[0] == '{')
            {
                beginLevel++;
                name = name.substr(1);
            }
            else if (name[0] == '}')
            {
                if (beginLevel)
                {
                    beginLevel--;
                    name = name.substr(1);
                }
                else
                {
                    std::cout << "Address Error { " << expr << " } End without begin. " << std::endl;
                    rv = false;
                }
            }
            else if (ispunct(name[0]) || isdigit(name[0]))
            {
                token = new TokenNode(name.substr(0, 1), beginLevel);
                name = name.substr(1);
            }
            else
            {
                std::cout << "Error { " << expr << " } Unknown token." << std::endl;
                rv = false;
            }
            if (token)
            {
                nodes.push_back(token);
            }
        }
    }
    if (beginLevel)
    {
        std::cout << "Address Error { " << expr << " } Begin without end." << std::endl;
        
        rv = false;
    }
    return rv;
}
void Parser::TagClasses(std::string &cclass, std::string coding, 
                        std::map<std::string, std::string> *values, 
                        std::deque<TokenNode *> &nodes)
{
    int n = addressClasses.size();
    int bytes = (n + 7)/8;
    BYTE *b = new BYTE[bytes];
    memset(b, 0, bytes);
    std::string temp = cclass;
    while (temp.size())
    {
        int npos = temp.find_first_not_of(", \t");
        if (npos == std::string::npos)
            break;
        int npos1 = temp.find_first_of(", \t", npos+1);
        if (npos1 == std::string::npos)
        {
            npos1 = temp.size();
        }
        std::string cclass = temp.substr(npos, npos1-npos);
        temp.replace(0,npos1,"");
        AddressClass *p = addressClasses[cclass];
        if (!p)
        {
            return ;
        }
        b[(p->id-1)/8] |= (1 << ((p->id-1) & 7));
    }
    for (std::deque<TokenNode *>::iterator it = nodes.begin(); it != nodes.end(); ++it)
    {
        (*it)->SetBytes(b, bytes);
    }
    if (nodes.size())
    {
        if (coding != "")
        {
            std::map<std::string, int>::iterator it = codings.find(coding);
            n = codings.size() + 1;
            if (it == codings.end())
                codings[coding] = n;
            else
                n = it->second;
            nodes.back()->coding = n;			
        }
        nodes.back()->eos = 1;
        if (values->size())
            nodes.back()->values = values;
    }
    delete b;
}
void Parser::EnterInTokenTree(TokenNode *root, std::deque<TokenNode *> &nodes)
{
    int n = addressClasses.size();
    int bytes = (n + 7)/8;
    std::deque<TokenNode *> *container = &root->branches;
    root->used = true;
    for (std::deque<TokenNode *>::iterator it = nodes.begin(); it != nodes.end(); ++it)
    {
        bool found = false;
        for (std::deque<TokenNode *>::iterator itb = container->begin(); itb != container->end(); ++ itb)
        {
            if ((*itb)->type == (*it)->type && (*itb)->opaque == (*it)->opaque && (*itb)->optionLevel == (*it)->optionLevel)
            {
                found = true;
                if (root == addressRoot)
                {
                    for (int i=0; i < bytes; i++)
                        (*itb)->bytes[i] |= (*it)->bytes[i];
                }
                if ((*it)->eos)
                {
                    (*itb)->eos = 1;
                    if ((*it)->coding != -1)
                    {
                        if ((*itb)->coding != -1)
                            std::cout << "Error { " << (*itb)->name << " } already has a coding" << std::endl;
                        (*itb)->coding = (*it)->coding;
                    }
                    if ((*it)->values != NULL)
                    {
                        if ((*itb)->values != NULL)
                            std::cout << "Error { " << (*itb)->name << " } already has values" << std::endl;
                        (*itb)->values = (*it)->values;
                    }
                }
                container = &(*itb)->branches;
                break;
            }
        }
        if (!found)
        {
            (*it)->used = true;
            container->push_back((*it));
            container = &(*it)->branches;
        }
    }
}
bool Parser::LoadAddresses()
{
    bool rv = true;
    addressRoot = new TokenNode("",0);
    for (std::deque<Address *>::iterator it = addresses.begin(); it != addresses.end();
         ++it)
    {
        std::deque<TokenNode *> nodes;
        rv &= LoadAddressTokens((*it)->name, nodes);
        TagClasses((*it)->cclass, (*it)->coding, &(*it)->values, nodes);
        EnterInTokenTree(addressRoot, nodes);
        nodes.clear();
    }
    return rv;
}
bool Parser::LoadOperandTokens(std::string name, std::string coding, 
                               std::map<std::string, std::string> *values, 
                               std::deque<TokenNode *> &nodes)
{
    int beginLevel = 0;
    bool rv= true;
    std::string expr = name;
    while (name.size() && rv)
    {
        int npos = name.find_first_not_of(" \n\r\v\t");
        if (npos == std::string::npos)
        {
            name = "";
        }
        else 
        {
            TokenNode *token = NULL;
            if (npos != 0)
                name = name.substr(npos);
            if (isalpha(name[0]))	// may need to be adjusted for architectures that have non-alpha name start chars
            {
                int i;
                for (i=0; i < name.size(); i++)
                {
                    if (!isalnum(name[i]))
                        break;
                }
                std::string label;
                label = name.substr(0, i);
                name = name.substr(i);
                for (std::deque<Register *>::iterator it = registers.begin(); it != registers.end(); ++it)
                {
                    if ((*it)->name == label)
                    {
                        token = new TokenNode((*it), beginLevel);
                        break;
                    }
                }
                if (!token)
                {
                    token = new TokenNode(label, beginLevel);
                }
            }
            else if (name[0] == '\'')
            {
                int npos = name.find_first_of("'", 1);
                std::string var, label;
                if (npos != std::string::npos)
                {
                    label = name.substr(1, npos-1);
                    name = name.substr(npos + 1);
                    npos = label.find_first_of(":");
                    if (npos == std::string::npos)
                    {
                        std::cout << "Address Error { " << expr << " } Register class ' " << label << " ' does not have a variable." << std::endl;
                    }
                    else
                    {
                        var = label.substr(0, npos);
                        label = label.substr(npos+1);
                    }
                }
                std::map<std::string, RegClass *>::iterator it = registerClasses.find(label);
                if (it != registerClasses.end())
                {
                    token = new TokenNode(it->second, beginLevel);
                    token->name = var;
                }
                else
                {
                    std::map<std::string, AddressClass *>::iterator it = addressClasses.find(label);
                    if (it != addressClasses.end())
                    {
                        token = new TokenNode(it->second, beginLevel);
                        token->name = var;
                    }
                    else
                    {
                        bool found = false;
                        for (std::deque<Number *>::iterator it = numbers.begin(); it != numbers.end(); ++ it)
                        {
                            if ((*it)->name == label)
                            {
                                found = true;
                                token = new TokenNode(*it, beginLevel);
                                token->name = var;
                            }
                        }
                        if (!found)
                        {
                            std::cout << "Operand Error { " << expr << " } class ' " << label << " ' Not found." << std::endl;
                            rv = false;
                        }
                    }
                }
            }
            else if (name[0] == '{')
            {
                beginLevel++;
                name = name.substr(1);
            }
            else if (name[0] == '}')
            {
                if (beginLevel)
                {
                    beginLevel--;
                    name = name.substr(1);
                }
                else
                {
                    std::cout << "Operand Error { " << expr << " } End without begin." << std::endl;
                    rv = false;
                }
            }
            else if (ispunct(name[0])|| isdigit(name[0]))
            {
                token = new TokenNode(name.substr(0, 1), beginLevel);
                name = name.substr(1);
            }
            else
            {
                std::cout << "Operand Error { " << expr << " } Unknown token." << std::endl;
                rv = false;
            }
            if (token)
            {
                nodes.push_back(token);
            }
        }
    }
    if (beginLevel)
    {
        rv = false;
        std::cout << "Operand Error { " << expr << " } Begin without end." << std::endl;
    }
    if (nodes.size())
    {
        if (coding != "")
        {
            std::map<std::string, int>::iterator it = codings.find(coding);
            int n = codings.size() + 1;
            if (it == codings.end())
                codings[coding] = n;
            else
                n = it->second;		
            nodes.back()->coding = n;
        }
        nodes.back()->eos = 1;
        if (values->size())
            nodes.back()->values = values;
    }
    return rv;
}
bool Parser::LoadOperands()
{
    bool rv = true;
    for (std::deque<Opcode *>::iterator it = opcodes.begin(); it != opcodes.end(); ++it)
    {
        (*it)->tokenRoot = new TokenNode("", 0);
        for (std::deque<Operand *>::iterator it1 = (*it)->operands.begin();
             it1 != (*it)->operands.end(); ++it1)
        {
            std::deque<TokenNode *>nodes;
            rv &= LoadOperandTokens((*it1)->name, (*it1)->coding, &(*it1)->values, nodes);
            EnterInTokenTree((*it)->tokenRoot, nodes);
        }
    }
    return rv;
}
