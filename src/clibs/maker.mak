#	Software License Agreement (BSD License)
#	
#	Copyright (c) 1997-2009, David Lindauer, (LADSoft).
#	All rights reserved.
#	
#	Redistribution and use of this software in source and binary forms, 
#	with or without modification, are permitted provided that the following 
#	conditions are met:

#	* Redistributions of source code must retain the above
#	  copyright notice, this list of conditions and the
#	  following disclaimer.

#	* Redistributions in binary form must reproduce the above
#	  copyright notice, this list of conditions and the
#	  following disclaimer in the documentation and/or other
#	  materials provided with the distribution.

#	* Neither the name of LADSoft nor the names of its
#	  contributors may be used to endorse or promote products
#	  derived from this software without specific prior
#	  written permission of LADSoft.

#	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
#	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
#	THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
#	PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
#	OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
#	EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
#	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
#	OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
#	WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
#	OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
#	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#	contact information:
#		email: TouchStone222@runbox.com <David Lindauer>
CC=occ
CFLAGS = $(C_FLAGS) $(DEFINES)
CILCFLAGS = $(CIL_C_FLAGS) $(DEFINES)

OCCIL_CLASS=lsmsilcrtl.rtl

LINK=olink
LINKFLAGS= -c+

vpath %.c .\cil\ #
vpath %.ilo $(CILOBJECT)
vpath %.l $(SYSOBJECT)
vpath %.nas .\386\ #
vpath %.o $(OBJECT) $(SYSOBJECT)

ASM=oasm
ASMFLAGS=

LIB=olib
LIBFLAGS=

IMPLIB=oimplib
IMPLIBFLAGS=

ifdef OLDSGL
else
ifdef STLPORT
else
CFLAGS := $(CFLAGS) /DSTD_NEWHANDLER
endif
endif

%.o: %.cpp
	$(CC) /c $(CFLAGS) $(BUILDING_DLL) -I$(STDINCLUDE) -o$(OBJECT)\$@F $^
#	$(CC) /S $(CFLAGS) $(BUILDING_DLL) -I$(STDINCLUDE) $^
#	$(ASM) $(ASMFLAGS) $(BUILDING_DLL) -o$(OBJECT)\$@F $*

%.o: %.c
	$(CC) /1 /c $(CFLAGS) $(BUILDING_DLL) -I$(STDINCLUDE) -o$(OBJECT)\$@F $^
#	$(CC) /S $(CFLAGS) $(BUILDING_DLL) -I$(STDINCLUDE) $^
#	$(ASM) $(ASMFLAGS) $(BUILDING_DLL) -o$(OBJECT)\$@F $*

%.o: %.nas
	$(ASM) $(ASMFLAGS) $(BUILDING_DLL) -o$(OBJECT)\$@F $^

%.ilo: %.c
	occil -N$(OCCIL_CLASS) /1 /c /WcMn $(CILCFLAGS) -I$(STDINCLUDE) -o$(CILOBJECT)\$@F $^

C_deps = $(notdir $(C_DEPENDENCIES:.c=.o))
ASM_deps = $(notdir $(ASM_DEPENDENCIES:.nas=.o))
CPP_deps = $(notdir $(CPP_DEPENDENCIES:.cpp=.o))
ifdef LSMSILCRTL
CIL_DEPS = $(notdir $(CIL_DEPENDENCIES:.c=.ilo))
endif
DEPENDENCIES = $(filter-out $(EXCLUDE), $(C_deps) $(ASM_deps) $(CPP_deps) $(CIL_DEPS))

define CALLDIR
	$(MAKE) -C$(dir)
endef

define ALLDIRS
alldirs:
	$(foreach dir, $(DIRS), $(CALLDIR))
endef
