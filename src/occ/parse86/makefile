# Software License Agreement
# 
#     Copyright(C) 1994-2019 David Lindauer, (LADSoft)
# 
#     This file is part of the Orange C Compiler package.
# 
#     The Orange C Compiler package is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
# 
#     The Orange C Compiler package is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
# 
#     You should have received a copy of the GNU General Public License
#     along with Orange C.  If not, see <http://www.gnu.org/licenses/>.
# 
#     contact information:
#         email: TouchStone222@runbox.com <David Lindauer>
# 

include ../../pathext2.mak

NAME=occpr
MAIN_FILE=ccmain.cpp
CPP_DEPENDENCIES= \
    $(BACKEND_FILES) \
    ccmain.cpp \
    ccif.cpp \
    db.cpp \
    beinterf.cpp \
    browse.cpp \
    ccerr.cpp \
    constopt.cpp \
    cppbltin.cpp \
    cpplookup.cpp \
    debug.cpp \
    declare.cpp \
    declcons.cpp \
    declcpp.cpp \
    expr.cpp \
    exprcpp.cpp \
    help.cpp \
    init.cpp \
    inline.cpp \
    lambda.cpp \
    lex.cpp \
    libcxx.cpp \
    list.cpp \
    mangle.cpp \
    memory.cpp \
    osutil.cpp \
    output.cpp \
    property.cpp \
    rtti.cpp \
    stmt.cpp \
    symtab.cpp \
    template.cpp \
    types.cpp \
    unmangle.cpp \
    wseh.cpp
LIB_DEPENDENCIES=sqlite3 ocpplib util
#determine if we can ignore the banner for a batch compile
#use long long type
LL = USE_LONGLONG
DEFINES=$(i386) $(LL) $(MSDOS) PARSER_ONLY 

BACKEND_INCLUDE_PATH=.

ifeq "$(COMPILER)" "gcc-linux"
SOURCE = ..$(PATHEXT2)parse:..$(PATHEXT2)preproc:..$(PATHEXT2)middle
INCLUDES=..$(PATHEXT2)parse ..$(PATHEXT2)preproc ..$(PATHEXT2)middle ..$(PATHEXT2)..$(PATHEXT2)util ..$(PATHEXT2)..$(PATHEXT2)ocpp
SRCPATH:=$(BACKEND_INCLUDE_PATH):$(SOURCE):..
else
SOURCE = ..$(PATHEXT2)parse;..$(PATHEXT2)preproc;..$(PATHEXT2)middle
INCLUDES=..$(PATHEXT2)parse ..$(PATHEXT2)preproc ..$(PATHEXT2)middle ..$(PATHEXT2)..$(PATHEXT2)util ..$(PATHEXT2)..$(PATHEXT2)ocpp
SRCPATH:=$(BACKEND_INCLUDE_PATH);$(SOURCE);..
endif
BACKEND_FILES = config.cpp


vpath %.c $(SRCPATH)
vpath %.cpp $(SRCPATH)

ifndef _TREEROOT
_TREEROOT := $(CURDIR)$(PATHEXT2)..$(PATHEXT2)..$(PATHEXT2)treetop.mak
_TREETARGET := $(CURDIR)
export _TREEROOT
export _TREETARGET

redirect:
	@$(MAKE) /f$(_TREEROOT) localfiles
format:
	@$(MAKE) /f$(_TREEROOT) format
endif

DISTRIBUTE: copyexe
