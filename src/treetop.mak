# Software License Agreement
# 
#     Copyright(C) 1994-2018 David Lindauer, (LADSoft)
# 
#     This file is part of the Orange C Compiler package.
# 
#     The Orange C Compiler package is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version, with the addition of the 
#     Orange C "Target Code" exception.
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

ifneq "$(COMPILER)" "gcc-linux"
SHELL=cmd.exe
export SHELL
endif

all:

ifeq "$(COMPILER)" "gcc-linux"
PATHSWAP = $(subst \,/,$(1))
else
PATHSWAP = $(subst /,\,$(1))
endif
export PATHSWAP

TREETOP := $(call PATHSWAP,$(dir $(_TREEROOT)))

DISTROOT := $(TREETOP)..

export DISTROOT

_TARGETDIR:= $(call PATHSWAP,$(CURDIR))

ifeq "$(COMPILER)" "gcc-linux"
TEST := $(shell ls "$(_TARGETDIR)/dirs.mak")
COMPARE := $(_TARGETDIR)/dirs.mak
else
TEST := $(shell dir /b "$(_TARGETDIR)\dirs.mak")
COMPARE := dirs.mak
endif

ifeq "$(TEST)" "$(COMPARE)"
include $(call PATHSWAP, $(_TARGETDIR)\dirs.mak)
endif
	


LIBS:= $(addsuffix .library,$(DIRS))
EXES:= $(addsuffix .exefile,$(DIRS))
CLEANS:= $(addsuffix .clean,$(DIRS))
DISTS:= $(addsuffix .dist,$(DIRS))
DISTS1:= $(addsuffix .dist1,$(DIRS))
CDIRS:= $(addsuffix .dirs,$(DIRS))
FORMATS:= $(addsuffix .format,$(DIRS))

NULLDEV := NUL


del:
	-del /Q  $(call PATHSWAP, $(_OUTPUTDIR)\*.*) 2> $(NULLDEV)
	-del /Q *.exe 2> $(NULLDEV)
mkdir:
	-mkdir  $(_OUTPUTDIR) 2> $(NULLDEV)
rmdir:
	-rmdir  $(_OUTPUTDIR) 2> $(NULLDEV)


ifeq "$(COMPILER)" "gcc-linux"
TEST := $(shell ls "$(_TARGETDIR)/makefile")
COMPARE := $(_TARGETDIR)/makefile
else
TEST := $(shell dir /b "$(_TARGETDIR)\makefile")
COMPARE := makefile
endif
ifeq "$(TEST)" "$(COMPARE)"
include $(call PATHSWAP, $(_TARGETDIR)\makefile)
include $(call PATHSWAP, $(DISTROOT)\src\dist.mak)
else
DISTRIBUTE:

endif


ifeq "$(MAIN_DEPENDENCIES)" "$(MAIN_FILE)"
link:
else
link: $(NAME).exe
endif

ifeq "$(COMPILER)" "gcc-linux"
_OUTPUTDIR = $(_TARGETDIR)/obj/$(OBJ_IND_PATH)
_LIBDIR = $(DISTROOT)/src/lib/$(OBJ_IND_PATH)
else
_OUTPUTDIR = $(_TARGETDIR)\obj\$(OBJ_IND_PATH)
_LIBDIR = $(DISTROOT)\src\lib\$(OBJ_IND_PATH)
endif
export _OUTPUTDIR
export _LIBDIR

include $(TREETOP)config.mak

export LIB_EXT
export LIB_PREFIX

ifeq "$(NAME)" ""
compile:
else
compile: $(call PATHSWAP, $(_LIBDIR)\$(LIB_PREFIX)$(NAME)$(LIB_EXT))
endif


ifndef _STARTED
_STARTED = 1
export _STARTED
export _TREEROOT
DISTBIN=$(DISTROOT)\bin
export DISTBIN
#DISTBIN_7=$(DISTROOT)\bin_7
#export DISTBIN_7
#DISTBIN_8=$(DISTROOT)\bin_8
#export DISTBIN_8
DISTHELP=$(DISTROOT)\help
export DISTHELP
DISTINC=$(DISTROOT)\include
export DISTINC
DISTINCWIN=$(DISTINC)\win32
export DISTINCWIN
DISTINCSTL=$(DISTINC)\stlport
export DISTINCSTL
DISTLIB=$(DISTROOT)\lib
export DISTLIB
DISTSTARTUP=$(DISTROOT)\lib\startup
export DISTSTARTUP
DISTSTARTUPDOS=$(DISTROOT)\lib\startup\msdos
export DISTSTARTUPDOS
DISTSTARTUPWIN=$(DISTROOT)\lib\startup\win32
export DISTSTARTUPWIN
DISTADDON=$(DISTROOT)\addon
export DISTADDON
DISTEXAM=$(DISTROOT)\examples
export DISTEXAM
DISTDIST=$(DISTROOT)\dist
export DISTDIST

ifdef MS
    RELEASEPATH = $(DISTROOT)\src\release
else
    RELEASEPATH= .
endif

export RELEASEPATH

COPYDIR := $(realpath $(DISTROOT)\src\copydir.exe)
export COPYDIR

PEPATCH:=$(realpath $(DISTROOT)\src\pepatch.exe)
export PEPATCH

RESTUB:=$(realpath $(DISTROOT)\src\restub.exe)
export RESTUB

RENSEG:=$(realpath $(DISTROOT)\src\renseg.exe)
export RENSEG

STUB:=$(realpath $(DISTROOT)\src\clibs\platform\dos32\extender\hx\dpmist32.bin)
export STUB

DISTMAKE := $(realpath $(DISTROOT)\src\dist.mak)
export DISTMAKE

cleanDISTRIBUTE: copydir.exe restub.exe renseg.exe pepatch.exe
ifndef NOMAKEDIR
	-mkdir $(DISTROOT) 2> $(NULLDEV)
#	-del /Q $(DISTROOT) 2> $(NULLDEV)
	-mkdir $(DISTROOT)\rule 2> $(NULLDEV)
	-del /Q $(DISTROOT)\rule 2> $(NULLDEV)
	-mkdir $(DISTBIN) 2> $(NULLDEV)
	-del /Q $(DISTBIN) 2> $(NULLDEV)
#	-mkdir $(DISTBIN_8) 2> $(NULLDEV)
#	-del /Q $(DISTBIN_8) 2> $(NULLDEV)
#	-mkdir $(DISTBIN_8)\branding 2> $(NULLDEV)
#	-del /Q $(DISTBIN_8)\branding 2> $(NULLDEV)
#	-mkdir $(DISTBIN_7) 2> $(NULLDEV)
#	-del /Q $(DISTBIN_7) 2> $(NULLDEV)
#	-mkdir $(DISTBIN_7)\branding 2> $(NULLDEV)
#	-del /Q $(DISTBIN_7)\branding 2> $(NULLDEV)
	-mkdir $(DISTHELP) 2> $(NULLDEV)
	-del /Q $(DISTHELP) 2> $(NULLDEV)
	-mkdir $(DISTINC) 2> $(NULLDEV)
	-del /Q $(DISTINC) 2> $(NULLDEV)
	-mkdir $(DISTINC)\sys 2> $(NULLDEV)
	-del /Q $(DISTINC)\sys 2> $(NULLDEV)
	-mkdir $(DISTINCWIN) 2> $(NULLDEV)
	-del /Q $(DISTINCWIN) 2> $(NULLDEV)
	-mkdir $(DISTINCSTL) 2> $(NULLDEV)
	-del /Q $(DISTINCSTL) 2> $(NULLDEV)
	-mkdir $(DISTINCSTL)\stlport 2> $(NULLDEV)
	-del /Q $(DISTINCSTL)\stlport 2> $(NULLDEV)
	-mkdir $(DISTINCSTL)\stlport\config 2> $(NULLDEV)
	-del /Q $(DISTINCSTL)\stlport\config 2> $(NULLDEV)
	-mkdir $(DISTINCSTL)\stlport\debug 2> $(NULLDEV)
	-del /Q $(DISTINCSTL)\stlport\debug 2> $(NULLDEV)
	-mkdir $(DISTINCSTL)\stlport\pointers 2> $(NULLDEV)
	-del /Q $(DISTINCSTL)\stlport\pointers 2> $(NULLDEV)
	-mkdir $(DISTINCSTL)\using 2> $(NULLDEV)
	-del /Q $(DISTINCSTL)\using 2> $(NULLDEV)
	-mkdir $(DISTINC)\stl 2> $(NULLDEV)
	-del /Q $(DISTINC)\stl 2> $(NULLDEV)
	-del /Q $(DISTLIB) 2> $(NULLDEV)
	-mkdir $(DISTSTARTUP) 2> $(NULLDEV)
	-del /Q $(DISTSTARTUP) 2> $(NULLDEV)
	-mkdir $(DISTSTARTUPDOS) 2> $(NULLDEV)
	-del /Q $(DISTSTARTUPDOS) 2> $(NULLDEV)
	-mkdir $(DISTSTARTUPWIN) 2> $(NULLDEV)
	-del /Q $(DISTSTARTUPWIN) 2> $(NULLDEV)
	-mkdir $(DISTADDON) 2> $(NULLDEV)
	-del /Q $(DISTADDON) 2> $(NULLDEV)
	-mkdir $(DISTEXAM) 2> $(NULLDEV)
	-del /Q $(DISTEXAM) 2> $(NULLDEV)
	-mkdir $(DISTEXAM)\msdos 2> $(NULLDEV)
	-del /Q $(DISTEXAM)\msdos 2> $(NULLDEV)
	-mkdir $(DISTEXAM)\system 2> $(NULLDEV)
	-del /Q $(DISTEXAM)\system 2> $(NULLDEV)
	-mkdir $(DISTEXAM)\win32 2> $(NULLDEV)
	-del /Q $(DISTEXAM)\win32 2> $(NULLDEV)
	-mkdir $(DISTEXAM)\win32\atc 2> $(NULLDEV)
	-del /Q $(DISTEXAM)\win32\atc 2> $(NULLDEV)
	-mkdir $(DISTEXAM)\win32\listview 2> $(NULLDEV)
	-del /Q $(DISTEXAM)\win32\listview 2> $(NULLDEV)
	-mkdir $(DISTEXAM)\win32\xmlview 2> $(NULLDEV)
	-del /Q $(DISTEXAM)\win32\xmlview 2> $(NULLDEV)
	-mkdir $(DISTEXAM)\win32\RCDemo 2> $(NULLDEV)
	-del /Q $(DISTEXAM)\win32\RCDemo 2> $(NULLDEV)
	-mkdir $(DISTEXAM)\win32\huff 2> $(NULLDEV)
	-del /Q $(DISTEXAM)\win32\huff 2> $(NULLDEV)
	-mkdir $(DISTDIST) 2> $(NULLDEV)
endif


cleanstart:
	-del /Q $(_LIBDIR)
	-rmdir $(_LIBDIR)

$(CLEANS): %.clean :
	$(MAKE) clean -f $(_TREEROOT) -C$*

clean: cleanstart $(CLEANS)
distribute: $(DISTS1)
	$(MAKE) DISTRIBUTE
else

cleanDISTRIBUTE:

$(CLEANS): %.clean :
	$(MAKE) clean -f $(_TREEROOT) -C$*
clean: del rmdir $(CLEANS)
	del *.xcf *.xcppf *.xhf

distribute: $(DISTS1)

endif

$(DISTS): %.dist : cleanDISTRIBUTE
	$(MAKE) DISTRIBUTE -f $(_TREEROOT) -C$*

$(DISTS1): %.dist1 : $(DISTS)
	$(MAKE) distribute -f $(_TREEROOT) -C$*

zip:
ifdef WITHMSDOS
# this requires CC386 be installed since it relies on far pointer support
# so I don't make it a part of the default install
	@$(MAKE) -C$(DISTROOT)\src\..\ -f $(realpath .\doszip.mak)
	@$(MAKE) -C$(DISTROOT)\src\dos\install -fmakefile.le
endif
	@$(MAKE) -C\ -f $(realpath .\zip.mak)

ifeq "$(COMPILER)" "gcc-linux"
$(CDIRS): %.dirs :
	-mkdir $*/obj 2> $(NULLDEV)
	-mkdir $*/obj/$(OBJ_IND_PATH) 2> $(NULLDEV)
else
$(CDIRS): %.dirs :
	-mkdir $*\obj\$(OBJ_IND_PATH) 2> $(NULLDEV)
endif
$(LIBS): %.library : $(CDIRS)
	$(MAKE) library compile -f $(_TREEROOT) -C$*
$(EXES): %.exefile :
	$(MAKE) exefile link -f $(_TREEROOT) -C$*

distribute_self:  cleanDISTRIBUTE
	$(MAKE) DISTRIBUTE
distribute_exe: $(DISTS1)

distribute_clibs_no_binary:
	@$(MAKE) -Cclibs DISTRIBUTE --eval="BUILDENV:=1"
distribute_clibs:
	@$(MAKE) -Cclibs DISTRIBUTE

makelibdir:
	-mkdir  $(_LIBDIR) 2> $(NULLDEV)

library: makelibdir $(LIBS)

exefile: makelibdir $(EXES)

localfiles: makelibdir mkdir compile library exefile


%.xcppf: %.cpp
	clang-format -style=file $< > $@
	copy $@ $<

%.xcf: %.c
	clang-format -style=file $< > $@
	copy $@ $<

%.xhf: %.h
	clang-format -style=file $< > $@
	copy $@ $<


$(FORMATS): %.format :
	$(MAKE) format -f $(_TREEROOT) -C$*

format: $(H_FILES:.h=.xhf) $(CPP_DEPENDENCIES:.cpp=.xcppf) $(C_DEPENDENCIES:.c=.xcf) $(FORMATS)


