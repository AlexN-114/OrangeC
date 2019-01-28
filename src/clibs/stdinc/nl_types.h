#ifndef _NL_TYPES_H
#    define _NL_TYPES_H

/* libcxx support */
#    include <stdarg.h>
#    include <string.h>
#    include <wchar.h>
#    include <stdlib.h>
#    include <time.h>
#    include <locale.h>
#    include <wctype.h>

#    define MB_CUR_MAX_L(a) MB_CUR_MAX

#    ifdef __cplusplus
extern "C"
{
#    endif
    inline void* newlocale(int category_mask, const char* locale, void* base) { return 0; }
    inline void freelocale(void* arg) {}

    inline int snprintf_l(char* __buffer, size_t n, locale_t larg, const char* __format, ...)
    {
        int rv;
        va_list arg;
        va_start(arg, __format);
        rv = vsnprintf(__buffer, n, __format, arg);
        va_end(arg);
        return rv;
    }
    inline int asprintf_l(char** str, locale_t larg, const char* __format, ...)
    {
        char __buffer[10000];
        int rv;
        va_list arg;
        va_start(arg, __format);
        rv = vsnprintf(__buffer, sizeof(__buffer), __format, arg);
        va_end(arg);
        *str = (char*)malloc(strlen(__buffer) + 1);
        strcpy(*str, __buffer);
        return rv;
    }
    inline int isdigit_l(char c, locale_t larg) { return c >= '0' && c <= '9'; }
    inline int isxdigit_l(char c, locale_t larg) { return c >= '0' && c <= '9' || c >= 'a' && c <= 'f' || c >= 'A' && c <= 'F'; }
    inline long long strtoll_l(const char* restrict __s, char** restrict __endptr, int __radix, locale_t larg)
    {
        return strtoll(__s, __endptr, __radix);
    }
    inline long long strtoull_l(const char* restrict __s, char** restrict __endptr, int __radix, locale_t larg)
    {
        return strtoull(__s, __endptr, __radix);
    }
    inline long double strtold_l(const char* restrict __s, char** restrict __endptr, locale_t larg)
    {
        return strtold(__s, __endptr);
    }
    inline int sscanf_l(const char* restrict __buffer, locale_t larg, const char* restrict __format, ...)
    {
        va_list arg;
        va_start(arg, __format);
        int rv = vsscanf(__buffer, __format, arg);
        va_end(arg);
        return rv;
    }
    inline int strcoll_l(const char* __s1, const char* __s2, locale_t larg) { return strcoll(__s1, __s2); }
    inline size_t strxfrm_l(char* restrict __s1, const char* restrict __s2, size_t __n, locale_t larg)
    {
        return strxfrm(__s1, __s2, __n);
    }
    inline int wcscoll_l(const wchar_t* __s1, const wchar_t* __s2, locale_t larg) { return wcscoll(__s1, __s2); }
    inline size_t wcsxfrm_l(wchar_t* restrict __s1, const wchar_t* restrict __s2, size_t __n, locale_t larg)
    {
        return wcsxfrm(__s1, __s2, __n);
    }
    inline int islower_l(int __c, locale_t larg) { return __c >= 'a' && __c <= 'z'; }
    inline int isupper_l(int __c, locale_t larg) { return __c >= 'A' && __c <= 'Z'; }
    inline int tolower_l(int __ch, locale_t larg)
    {
        if (isupper_l(__ch, 0))
            __ch += 'a' - 'A';
        return __ch;
    }
    inline int toupper_l(int __ch, locale_t larg)
    {
        if (islower_l(__ch, 0))
            __ch -= 'a' - 'A';
        return __ch;
    }
    inline int iswcntrl_l(wint_t __wc, locale_t larg) { return iswcntrl(__wc); }
    inline int iswdigit_l(wint_t __wc, locale_t larg) { return __wc >= '0' && __wc <= '9'; }
    inline int iswgraph_l(wint_t __wc, locale_t larg) { return iswgraph(__wc); }
    inline int iswlower_l(wint_t __wc, locale_t larg) { return __wc >= 'a' && __wc <= 'z'; }
    inline int iswprint_l(wint_t __wc, locale_t larg) { return __wc >= ' ' && __wc <= '~'; }
    inline int iswpunct_l(wint_t __wc, locale_t larg) { return iswpunct(__wc); }
    inline int iswspace_l(wint_t __wc, locale_t larg) { return iswspace(__wc); }
    inline int iswblank_l(wint_t __wc, locale_t larg) { return iswblank(__wc); }
    inline int iswupper_l(wint_t __wc, locale_t larg) { return __wc >= 'A' && __wc <= 'Z'; }
    inline int iswxdigit_l(wint_t __wc, locale_t larg)
    {
        return iswdigit_l(__wc, (locale_t)0) || __wc >= 'a' && __wc <= 'f' || __wc >= 'A' && __wc <= 'F';
    }
    inline int iswalpha_l(wint_t __wc, locale_t larg) { return iswlower_l(__wc, (locale_t)0) || iswupper_l(__wc, (locale_t)0); }
    inline int iswalnum_l(wint_t __wc, locale_t larg) { return iswalpha_l(__wc, (locale_t)0) || iswdigit_l(__wc, (locale_t)0); }

    inline wint_t towlower_l(wint_t __wc, locale_t larg) { return tolower_l(__wc, (locale_t)0); }
    inline wint_t towupper_l(wint_t __wc, locale_t larg) { return toupper_l(__wc, (locale_t)0); }

    inline wint_t btowc_l(int __c, locale_t larg) { return btowc(__c); }
    inline int wctob_l(wint_t __c, locale_t larg) { return wctob(__c); }

    inline size_t mbrtowc_l(wchar_t* restrict __pwc, const char* restrict __s, size_t __n, mbstate_t* restrict __p, locale_t larg)
    {
        return mbrtowc(__pwc, __s, __n, __p);
    }
    inline size_t mbtowc_l(wchar_t* restrict __pwc, const char* restrict __s, size_t __n, locale_t larg)
    {
        return mbtowc(__pwc, __s, __n);
    }
    inline size_t wcrtomb_l(char* restrict __s, wchar_t __wc, mbstate_t* restrict __ps, locale_t larg)
    {
        return wcrtomb(__s, __wc, __ps);
    }
    inline size_t mbrlen_l(const char* restrict __s, size_t __n, mbstate_t* restrict __ps, locale_t larg)
    {
        return mbrlen(__s, __n, __ps);
    }
    inline size_t mbsrtowcs_l(wchar_t* restrict __dst, const char** restrict __src, size_t __len, mbstate_t* restrict __ps,
                              locale_t larg)
    {
        return mbsrtowcs(__dst, __src, __len, __ps);
    }
    inline size_t wcsrtombs_l(char* restrict __dst, const wchar_t** restrict __src, size_t __len, mbstate_t* restrict __ps,
                              locale_t larg)
    {
        return wcsrtombs(__dst, __src, __len, __ps);
    }
    inline size_t wcsnrtombs_l(char* restrict __dst, const wchar_t** restrict __src, size_t __nms, size_t __len,
                               mbstate_t* restrict __ps, locale_t larg)
    {
        return wcsnrtombs(__dst, __src, __nms, __len, __ps);
    }
    inline size_t mbsnrtowcs_l(wchar_t* restrict __dst, const char** restrict __src, size_t nms, size_t __len,
                               mbstate_t* restrict __ps, locale_t larg)
    {
        return mbsnrtowcs(__dst, __src, nms, __len, __ps);
    }

    inline size_t strftime_l(char* restrict __s, size_t __maxsize, char* restrict __fmt, const struct tm* restrict __t,
                             locale_t larg)
    {
        return strftime(__s, __maxsize, __fmt, __t);
    }
    inline struct lconv* localeconv_l(locale_t larg) { return localeconv(); }
#    ifdef __cplusplus
}
#    endif

#endif
#ifdef __cplusplus
namespace __STD_NS__
{
using ::asprintf_l;
using ::btowc_l;
using ::freelocale;
using ::isdigit_l;
using ::islower_l;
using ::isupper_l;
using ::iswalnum_l;
using ::iswalpha_l;
using ::iswblank_l;
using ::iswcntrl_l;
using ::iswdigit_l;
using ::iswgraph_l;
using ::iswlower_l;
using ::iswprint_l;
using ::iswpunct_l;
using ::iswspace_l;
using ::iswupper_l;
using ::iswxdigit_l;
using ::isxdigit_l;
using ::localeconv_l;
using ::mbrlen_l;
using ::mbrtowc_l;
using ::mbsnrtowcs_l;
using ::mbsrtowcs_l;
using ::mbtowc_l;
using ::newlocale;
using ::snprintf_l;
using ::sscanf_l;
using ::strcoll_l;
using ::strftime_l;
using ::strtold_l;
using ::strtoll_l;
using ::strtoull_l;
using ::strxfrm_l;
using ::tolower_l;
using ::toupper_l;
using ::towlower_l;
using ::towupper_l;
using ::wcrtomb_l;
using ::wcscoll_l;
using ::wcsnrtombs_l;
using ::wcsrtombs_l;
using ::wcsxfrm_l;
using ::wctob_l;
}  // namespace __STD_NS__
#endif