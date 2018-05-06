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

ifeq "$(COMPILER)" "OCC"

COMPILER_PATH := $(DISTROOT)
OBJ_IND_PATH := occ

CPP_deps = $(notdir $(CPP_DEPENDENCIES:.cpp=.o))
C_deps = $(notdir $(C_DEPENDENCIES:.c=.o))
ASM_deps = $(notdir $(ASM_DEPENDENCIES:.nas=.o))
TASM_deps = $(notdir $(TASM_DEPENDENCIES:.asm=.o))
RES_deps = $(notdir $(RC_DEPENDENCIES:.rc=.res))

MAIN_DEPENDENCIES = $(MAIN_FILE:.cpp=.o)
ifeq "$(MAIN_DEPENDENCIES)" "$(MAIN_FILE)"
MAIN_DEPENDENCIES = $(MAIN_FILE:.c=.o)
endif

LLIB_DEPENDENCIES = $(notdir $(filter-out $(addsuffix .o,$(EXCLUDE)) $(MAIN_DEPENDENCIES), $(CPP_deps) $(C_deps) $(ASM_deps) $(TASM_deps)))


CC=$(COMPILER_PATH)\bin\occ
CCFLAGS = /c /E- /!

LINK=$(COMPILER_PATH)\bin\olink
LFLAGS=-c -mx /L$(_LIBDIR) /!

LIB=$(COMPILER_PATH)\bin\olib
LIB_EXT:=.l
LIB_PREFIX:=
LIBFLAGS= /!

ASM=$(COMPILER_PATH)\bin\\oasm

ASM=oasm
ASMFLAGS= /!

RC=$(COMPILER_PATH)\bin\orc
RCINCLUDE=$(DISTROOT)\include
	RCFLAGS = -r /!

ifneq "$(INCLUDES)" ""
CINCLUDES:=$(addprefix /I,$(INCLUDES))
endif
DEFINES := $(addprefix /D,$(DEFINES))
DEFINES := $(subst @, ,$(DEFINES))
LIB_DEPENDENCIES := $(foreach file, $(addsuffix .l,$(LIB_DEPENDENCIES)), $(file))

$(info $(LIB_DEPENDENCIES))

CCFLAGS := $(CCFLAGS) $(CINCLUDES) $(DEFINES) /DMICROSOFT /DBORLAND /DWIN32
ifeq "$(TARGET)" "GUI"
STARTUP=C0pe.o
TYPE=/T:GUI32
COMPLIB=clwin$(LIB_EXT) climp$(LIB_EXT)
else
STARTUP=C0Xpe.o
TYPE=/T:CON32
COMPLIB=clwin$(LIB_EXT) climp$(LIB_EXT)
endif

vpath %.o $(_OUTPUTDIR)
vpath %$(LIB_EXT) $(DISTROOT)\lib $(_LIBDIR)
vpath %.res $(_OUTPUTDIR)

%.o: %.cpp
	$(CC) $(CCFLAGS) -o$(_OUTPUTDIR)/$@ $^

%.o: %.c
	$(CC) /9 $(CCFLAGS) -o$(_OUTPUTDIR)/$@ $^

%.o: %.nas
	$(ASM) $(ASMFLAGS) -o$(_OUTPUTDIR)/$@ $^

%.res: %.rc
	$(RC) -i$(RCINCLUDE) $(RCFLAGS) -o$(_OUTPUTDIR)/$@ $^

$(_LIBDIR)\$(NAME)$(LIB_EXT): $(LLIB_DEPENDENCIES)
#	-del $(_LIBDIR)\$(NAME)$(LIB_EXT) >> $(NULLDEV)
	$(LIB) $(LIBFLAGS) $(_LIBDIR)\$(NAME)$(LIB_EXT) $(addprefix +-$(_OUTPUTDIR)\,$(LLIB_DEPENDENCIES))

$(NAME).exe: $(MAIN_DEPENDENCIES) $(addprefix $(_LIBDIR)\,$(LIB_DEPENDENCIES)) $(_LIBDIR)\$(NAME)$(LIB_EXT) $(RES_deps)
	$(LINK) /o$(NAME).exe $(TYPE) $(LFLAGS) $(STARTUP) $(addprefix $(_OUTPUTDIR)\,$(MAIN_DEPENDENCIES)) $(_LIBDIR)\$(NAME)$(LIB_EXT) $(LIB_DEPENDENCIES) $(COMPLIB) $(DEF_DEPENDENCIES) $(addprefix $(_OUTPUTDIR)\,$(RES_deps))

%.exe: %.c
	$(CC) -! -o$@ $^

%.exe: %.cpp
	$(CC) -! -o$@ $^

endif