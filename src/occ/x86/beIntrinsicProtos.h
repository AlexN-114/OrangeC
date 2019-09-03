#ifndef PROTO
#    define PROTO(PROT, NAME, FUNC) PROT
#endif
PROTO("unsigned __fastcall__ __builtin_clz(unsigned val);", __builtin_clz, handleCLZ)
PROTO("unsigned __fastcall__ __builtin_clzl(unsigned long val);", __builtin_clzl, handleCLZ)
PROTO("unsigned __fastcall__ __builtin_ctz(unsigned val);", __builtin_ctz, handleCTZ)
PROTO("unsigned __fastcall__ __builtin_ctzl(unsigned long val);", __builtin_ctzl, handleCTZ)
PROTO("unsigned int __fastcall__ _rotl(unsigned int val, int shift);", _rotl, handleROTL)
PROTO("unsigned int __fastcall__ _rotr(unsigned int val, int shift);", _rotr, handleROTR)
PROTO("unsigned char __fastcall__ _rotl8(unsigned char value, unsigned char shift);", _rotl8, handleROTL8)
PROTO("unsigned short __fastcall__ _rotl16(unsigned short value, unsigned char shift);", _rotl16, handleROTL16)
PROTO("unsigned char __fastcall__ _rotr8(unsigned char value, unsigned char shift);", _rotr8, handleROTR8)
PROTO("unsigned short __fastcall__ _rotr16(unsigned short value, unsigned char shift);", _rotr16, handleROTR16)
PROTO("unsigned char __fastcall__ _BitScanForward(unsigned long* index, unsigned long Mask);", _BitScanForward, handleBSF)
PROTO("unsigned char __fastcall__ _BitScanReverse(unsigned long* index, unsigned long Mask);", _BitScanReverse, handleBSR)
PROTO("void __fastcall__ __outbyte(unsigned short Port, unsigned char Data);", __outbyte, handleOUTB)
PROTO("void __fastcall__ __outword(unsigned short Port, unsigned short Data);", __outword, handleOUTW)
PROTO("void __fastcall__ __outdword(unsigned short Port, unsigned long  Data);", __outdword, handleOUTD)
PROTO("unsigned char __fastcall__ __inbyte(unsigned short Port);", __inbyte, handleINB)
PROTO("unsigned short __fastcall__ __inword(unsigned short Port);", __inword, handleINW)
PROTO("unsigned long __fastcall__ __indword(unsigned short Port);", __indword, handleIND)
PROTO("unsigned short __fastcall__ __builtin_bswap16(unsigned short val);", __builtin_bswap16, handleBSWAP16)
PROTO("unsigned __fastcall__ __builtin_bswap32(unsigned val);", __builtin_bswap32, handleBSWAP32)
PROTO("unsigned __int64__  __fastcall__ __builtin_bswap64(unsigned __int64__ val);", __builtin_bswap64, handleBSWAP64)
#undef PROTO