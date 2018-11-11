#ifndef PROTO
#    define PROTO(PROT, NAME, FUNC) PROT
#endif
PROTO("unsigned __fastcall __builtin_clz(unsigned val);", __builtin_clz, handleCLZ)
PROTO("unsigned __fastcall __builtin_clzl(unsigned long val);", __builtin_clzl, handleCLZ)
PROTO("unsigned __fastcall __builtin_ctz(unsigned val);", __builtin_ctz, handleCTZ)
PROTO("unsigned __fastcall __builtin_ctzl(unsigned long val);", __builtin_ctzl, handleCTZ)
PROTO("unsigned int __fastcall _rotl(unsigned int val, int shift);", _rotl, handleROTL)
PROTO("unsigned int __fastcall _rotr(unsigned int val, int shift);", _rotr, handleROTR)
PROTO("unsigned char __fastcall _rotl8(unsigned char value, unsigned char shift);", _rotl8, handleROTL8)
PROTO("unsigned short __fastcall _rotl16(unsigned short value, unsigned char shift);", _rotl16, handleROTL16)
PROTO("unsigned char __fastcall _rotr8(unsigned char value, unsigned char shift);", _rotr8, handleROTR8)
PROTO("unsigned short __fastcall _rotr16(unsigned short value, unsigned char shift);", _rotr16, handleROTR16)
PROTO("unsigned char __fastcall _BitScanForward(unsigned long* index, unsigned long Mask);", _BitScanForward, handleBSF)
PROTO("unsigned char __fastcall _BitScanReverse(unsigned long* index, unsigned long Mask);", _BitScanReverse, handleBSR)
PROTO("void __fastcall __outbyte(unsigned short Port, unsigned char Data);", __outbyte, handleOUTB)
PROTO("void __fastcall __outword(unsigned short Port, unsigned short Data);", __outword, handleOUTW)
PROTO("void __fastcall __outdword(unsigned short Port, unsigned long  Data);", __outdword, handleOUTD)
PROTO("unsigned char __fastcall __inbyte(unsigned short Port);", __inbyte, handleINB)
PROTO("unsigned short __fastcall __inword(unsigned short Port);", __inword, handleINW)
PROTO("unsigned long __fastcall __indword(unsigned short Port);", __indword, handleIND)
#undef PROTO