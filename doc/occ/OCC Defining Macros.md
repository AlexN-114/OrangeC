## OCC Defining Macros

### /Dxxx    define a macro
 
This switch defines a macro as if a \#define statement had been used somewhere in the source.  It is useful for building different versions of a program without modifying the source files between compiles.  Note that you may not give macros defined this way a value.  For example:
 
     OCC /DORANGE myfunc.c
 
is equivalent to placing the following statement in the file and compiling it.
 
     #define ORANGE 

The following macros are predefined by the compiler:


 

|Macro |Usage |
|--- |--- |
|\_\_ORANGEC\_\_|always defined|
|\_\_RTTI\_\_|defined when C++ rtti/exception handling info is present|
|\_\_386\_\_|always defined|
|\_\_i386\_\_|always defined|
|\_i386\_|always defined|
|\_\_i386|always defined|
|\_\_WIN32\_\_|defined for WIN32|
|\_WIN32|defined for WIN32|
|\_\_DOS\_\_|defined for MSDOS|
|\_\_RAW\_IMAGE\_\_|defined if building a raw image|
|\_\_LSCRTL\_DLL|defined when LSCRTL.dll is in use|
|\_\_MSVCRT\_DLL|defined when MSVCRT.dll is in use|
|\_\_CRTDLL\_DLL|defined when CRTDLL.dll is in use|



  
