#OCC

 
 **OCC** is an optimiziing compiler capable of compilng C language files written to the C99 standard.  However in its default mode, it compiles to the older standard for which most legacy programs are written.
 
 **OCC** currently only generates code for the x86 series processor.  Together with the rest of the toolchain and supplied libraries, it can be used to create WIN32 program files.  This toolchain also includes extenders necessary for running WIN32 applications on MSDOS, so **OCC** may be run on MSDOS and used to generate MSDOS programs as well.
 
 By default **OCC** will spawn the necessary subprograms to generate a completed executable from a source file.
 
 A companion program, **OCL, **may be used to generate MSDOS executables which depend on one of a variety of MSDOS extenders.


##Command Line Options

 
 The general form of an **OCC** [Command Line](OCC%20Command%20Line.html) is:
 
> OAsm \[options\] filename-list
 
 Where _filename-list_ gives a list of files to assemble.


##Extended Keywords
 

 In addition to support for the C99 standard, **OCC** supports a variety of the usual [compiler extensions](OCC%20Extended%20Keywords.html) found in MSDOS and WIN32 compilers.


##\#Pragma Directives

 
 **OCC** supports a range of [#pragma preprocessor directives](OCC%20Pragma%20Directives.html) to allow some level of control over the generated code.  Such directives include support for structure alignment, having the CRTL run routines
 as part of its normal startup and shutdown process, and so forth.
 
 
 