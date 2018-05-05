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

#ifndef DEPENDS_H
#define DEPENDS_H

#include <string>
#include <map>
#include <list>
#include <set>
#include "Rule.h"

class Spawner;
class Depends
{
public:
	Depends(const std::string &Goal, Time &xx, bool ToDelete)
	: goal(Goal), time(xx), toDelete(ToDelete), ordered(false), isSecondary(false), 
    rule(nullptr), ruleList(nullptr), spawner(nullptr) { }
	~Depends();
	
	void operator +=(Depends *depend) { subgoals.push_back(depend); }
	Time &GetTime() { return time; }
	static Depends *Lookup(const std::string &val) 
	{ 
		auto it = all.find(val);
		if (it != all.end())
			return it->second;
		return nullptr;
	}
	size_t size() const { return subgoals.size(); }
	void SetRuleList(RuleList *r) { ruleList = r; }
	RuleList *GetRuleList() const { return ruleList; }
	void SetRule(Rule *r) { rule = r; }
	Rule *GetRule() const { return rule; }
	void SetSilent(bool flag) { silent = flag; }
	void SetDisplayOnly(bool flag) { displayOnly = flag; }
	void SetIgnoreResults(bool flag) { ignoreResults = flag; }
	void SetOrdered(bool flag) { ordered = flag; }
	bool GetOrdered() { return ordered; }		
	bool ShouldDelete() { return toDelete; }
	void SetSecondary(bool flag) { isSecondary = flag; }
	bool IsSecondary() { return isSecondary; }
	std::string GetGoal() const { return goal; }
	typedef std::list<Depends *>::iterator iterator;
	const iterator begin() { return subgoals.begin(); }
	const iterator end() { return subgoals.end(); }
	void Precious() { toDelete = false; }
	Spawner *GetSpawner() { return spawner; }
	void SetSpawner(Spawner *spawner) { this->spawner = spawner; }
private:
	std::string goal;
	std::string foundName;
	RuleList *ruleList;
	Rule *rule;
	Time time;
	bool silent;
	bool displayOnly;
	bool ignoreResults;
	bool toDelete;
	bool ordered;
	bool isSecondary;
	Spawner *spawner;
	std::list<Depends *> subgoals;		
	static std::map<std::string, Depends *> all;
};
#endif