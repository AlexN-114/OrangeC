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

#include "Spawner.h"
#include "Eval.h"
#include "Maker.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <deque>
#include "utils.h"
#include <algorithm>

const char Spawner::escapeStart = '\x1';
const char Spawner::escapeEnd = '\x2';
int Spawner::lineLength = 1024 * 1024; // os limitation on XP and later is 8191
std::list<std::string> Spawner::cmdList;

int Spawner::Run(Command &commands, RuleList *ruleList, Rule *rule)
{
    int rv = 0;
    std::string longstr;
    std::deque<std::string> tempFiles;
    for (Command::iterator it = commands.begin(); it != commands.end() && (!rv || !posix) ; ++it)
    {
        bool curSilent = silent;
        bool curIgnore = ignoreErrors;
        bool curDontRun = dontRun;
        const std::string &a = (*it);
        int i;
        for (i=0; i < a.size(); i++)
            if (a[i] == '+')
                curDontRun = false;
            else if (a[i] == '@')
                curSilent = true;
            else if (a[i] == '-')
                curIgnore = true;
            else
                break;
        if (a.find("$(MAKE)") != std::string::npos || a.find("${MAKE}") != std::string::npos)
            curDontRun = false;
        std::string cmd = a.substr(i);
        Eval c(cmd, false, ruleList, rule);
        cmd = c.Evaluate(); // deferred evaluation
        size_t n = cmd.find("&&");
        std::string makeName;
        if (n != std::string::npos && n == cmd.size() - 3)
        {
            char match = cmd[n+2];
            cmd.erase(n);
            makeName = "maketemp.";
            if (tempNum < 10)
                makeName = makeName + "00" + Utils::NumberToString(tempNum);
            else if (tempNum < 100)
                makeName = makeName + "0" + Utils::NumberToString(tempNum);
            else
                makeName = makeName + Utils::NumberToString(tempNum);
            tempNum++;
            if (!keepResponseFiles && makeName.size())
                tempFiles.push_back(makeName);
            std::fstream fil(makeName.c_str(), std::ios::out);
            bool done = false;
            std::string tail;
            do
            {
                ++it;
                std::string current = *it;
                size_t n = current.find(match);
                if (n != std::string::npos)
                {
                    done = true;
                    if (n+1 < current.size())
                        tail = current.substr(n+1);
                    current.erase(n);
                }
                Eval ce(current, false, ruleList, rule);
                fil << ce.Evaluate().c_str() << std::endl;
            } while (!done);						
            fil.close();
            cmd += makeName + tail;
        }
        cmd = QualifyFiles(cmd);
        if (oneShell)
             longstr += cmd;
        else
             rv = Run(cmd, curIgnore, curSilent, curDontRun);
        if (curIgnore)
            rv = 0;
        if (rv && posix)
            break;
    }
    if (oneShell)
        rv = Run(longstr, ignoreErrors, silent, dontRun);
    for (auto f : tempFiles)
        OS::RemoveFile(f);
    return rv;
}
int Spawner::Run(const std::string &cmdin, bool ignoreErrors, bool silent, bool dontrun)
{
    std::string cmd = OS::NormalizeFileName(cmdin);
    if (oneShell)
    {
        return OS::Spawn(cmd, environment);
    }
    else if (!split(cmd))
    {
        Eval::error(std::string ("Command line too long:\n") + cmd);
        return 0xff;
    }
    else
    {
        int rv = 0;
        for (auto command : cmdList)
        {
            if (!silent)
                std::cout << "\t" << command.c_str() << std::endl;
            int rv1;
            if (!dontrun)
            {
                rv1 = OS::Spawn(command, environment);
                if (!rv)
                     rv = rv1;
                if (rv && posix)
                     return rv;
            }
        }
        return rv;
    }
}
bool Spawner::split(const std::string &cmd)
{
    bool rv = true;
    cmdList.clear();
    int n = cmd.find_first_of(escapeStart);
    if (n != std::string::npos)
    {
        int m = cmd.find_first_of(escapeEnd);
        std::string first = cmd.substr(0, n);
        std::string last = cmd.substr(m);
        std::string middle = cmd.substr(n, m);
        int z = middle.find_first_of(escapeStart);
        if (z != std::string::npos)
            rv = cmd.size() < lineLength;
        z = last.find_first_of(escapeStart);
        if (z != std::string::npos)
            rv = cmd.size() < lineLength;
            
        int sz = first.size() + last.size();
        int szmiddle = lineLength - sz;
        while (middle.size() >= szmiddle)
        {
            std::string p = middle.substr(0, szmiddle);
            int lsp = middle.find_last_of(' ');
            if (lsp != std::string::npos)
            {
                p.replace(lsp, p.size()-lsp, "");
                middle.replace(0, lsp+1, "");
                cmdList.push_back(first + p + last);
            }
            else 
            {
                rv = false;
                break;
            }
        }
        if (middle.size())
        {
            cmdList.push_back(first + middle + last);
        }
    }
    else
    {
        rv = cmd.size() < lineLength;
        cmdList.push_back(cmd);
    }
    return rv;
}
std::string Spawner::shell(const std::string &cmd)
{
    std::string rv = OS::SpawnWithRedirect(cmd);
    int n = rv.size();
    while (n && rv[n - 1] == '\r' || rv[n - 1] == '\n')
        n--;
    rv = rv.substr(0, n);
    std::replace(rv.begin(), rv.end(), '\r', ' ');
    std::replace(rv.begin(), rv.end(), '\n', ' ');
    return rv;
}
std::string Spawner::QualifyFiles(const std::string &cmd)
{
    std::string rv;
    std::string working = cmd;
    while (working.size())
    {
        std::string cur = Eval::ExtractFirst(working, " ");
        cur = Maker::GetFullName(cur);
        if (rv.size())
            rv += " ";
        rv += cur;
    }
    return rv;
}
