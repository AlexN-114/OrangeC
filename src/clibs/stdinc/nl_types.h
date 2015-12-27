/* libcxx support */
#include <string.h>
#include <wchar.h>
#include <stdlib.h>
#include <time.h>

#define MB_CUR_MAX_L(a) MB_CUR_MAX

#ifdef __cplusplus
extern "C" {
#endif
struct lconv *  _RTL_FUNC _IMPORT localeconv( void );
int _RTL_FUNC _IMPORT iswcntrl(int);
int _RTL_FUNC _IMPORT iswgraph(int);
int _RTL_FUNC _IMPORT iswpunct(int);
int _RTL_FUNC _IMPORT iswspace(int);
int _RTL_FUNC _IMPORT iswblank(int);
inline void * newlocale(int category_mask, const char *locale,
                          void *base) { return 0; }
inline void freelocale(void *) { }

inline int snprintf_l(char * __buffer, size_t n, locale_t, 
                                const char * __format, ...)
{
    int rv;
    va_list arg;
    va_start(arg, __format);
    rv = vsnprintf(__buffer, n, __format, arg);
    va_end(arg);
    return rv;
}
inline int asprintf_l(char **str, locale_t, const char *__format, ...)
{
    char __buffer[10000];
    int rv;
    va_list arg;
    va_start(arg, __format);
    rv = vsnprintf(__buffer, sizeof(__buffer), __format, arg);
    va_end(arg);
    *str = (char *)malloc(strlen(__buffer) + 1);
    strcpy(*str, __buffer);
    return rv;
}
inline int isdigit_l(char c, locale_t)
{ return c >= '0' && c <= '9'; }
inline int isxdigit_l(char c, locale_t)
{ return c >= '0' && c <= '9' || c>='a' && c <= 'f' || c >= 'A' && c <= 'F'; }
inline long long strtoll_l(const char *restrict __s, char **restrict __endptr,
                                    int __radix, locale_t)
{ return strtoll(__s, __endptr, __radix); }
inline long long strtoull_l(const char *restrict __s, char **restrict __endptr,
                                    int __radix, locale_t)
{ return strtoull(__s, __endptr, __radix); }
inline long double strtold_l(const char *restrict __s, char ** restrict __endptr, locale_t)
{ return strtold(__s, __endptr); }
inline int sscanf_l(const char *restrict __buffer, locale_t, 
                                  const char *restrict __format, ...)
{
    va_list arg;
    va_start(arg, __format);
    int rv = vsscanf(__buffer, __format, arg);
    va_end(arg);
    return rv;
}
inline int          strcoll_l(const char *__s1, const char *__s2, locale_t)
{ return strcoll(__s1, __s2); }
inline size_t       strxfrm_l(char *restrict __s1, const char *restrict __s2,
                                          size_t __n, locale_t )
{ return strxfrm(__s1, __s2, __n); }
inline int wcscoll_l (const wchar_t *__s1, const wchar_t *__s2, locale_t)
{ return wcscoll(__s1, __s2); }
inline size_t wcsxfrm_l (wchar_t *restrict __s1, const wchar_t *restrict __s2, 
                    size_t __n, locale_t)
{ return wcsxfrm(__s1, __s2, __n); }
inline int      islower_l (int __c, locale_t)
{ return __c >= 'a' && __c <= 'z'; }
inline int      isupper_l (int __c, locale_t)
{ return __c >= 'A' && __c <= 'Z'; }
inline int      tolower_l(int __ch, locale_t)
{
    if (isupper_l(__ch, 0))
        __ch += 'a' - 'A';
    return __ch;
}
inline int      toupper_l(int __ch, locale_t)
{
    if (islower_l(__ch, 0))
        __ch -= 'a' - 'A';
    return __ch;
}
inline int iswcntrl_l (wint_t __wc, locale_t)
{ return iswcntrl(__wc); }
inline int iswdigit_l (wint_t __wc, locale_t)
{ return __wc >= '0' && __wc <= '9'; }
inline int iswgraph_l (wint_t __wc, locale_t)
{ return iswgraph(__wc); }
inline int iswlower_l (wint_t __wc, locale_t)
{ return __wc >= 'a' && __wc <= 'z'; }
inline int iswprint_l (wint_t __wc, locale_t)
{ return __wc >= ' ' && __wc <= '~'; }
inline int iswpunct_l (wint_t __wc, locale_t)
{ return iswpunct(__wc); }
inline int iswspace_l (wint_t __wc, locale_t)
{ return  iswspace(__wc); }
inline int iswblank_l (wint_t __wc, locale_t)
{ return  iswblank(__wc); }
inline int iswupper_l (wint_t __wc, locale_t)
{ return __wc >= 'A' && __wc <= 'Z'; }
inline int iswxdigit_l (wint_t __wc, locale_t)
{ return iswdigit_l(__wc, (locale_t)0) || __wc >= 'a' && __wc <= 'f' || __wc >= 'A' && __wc <= 'F'; }
inline int iswalpha_l (wint_t __wc, locale_t)
{ return iswlower_l(__wc, (locale_t)0) || iswupper_l(__wc, (locale_t)0); }
inline int iswalnum_l (wint_t __wc, locale_t)
{ return iswalpha_l(__wc, (locale_t)0) || iswdigit_l(__wc, (locale_t)0); }

inline wint_t towlower_l (wint_t __wc, locale_t)
{ return tolower_l(__wc, (locale_t)0); }
inline wint_t towupper_l (wint_t __wc, locale_t)
{ return toupper_l(__wc, (locale_t)0); }

inline wint_t btowc_l (int __c, locale_t)
{ return btowc(__c); }
inline int wctob_l (wint_t __c, locale_t)
{ return wctob(__c); }

inline size_t mbrtowc_l (wchar_t *restrict __pwc, const char *restrict __s, size_t __n,
			    mbstate_t *restrict __p, locale_t)
{ return mbrtowc(__pwc, __s, __n, __p); }
inline size_t mbtowc_l (wchar_t *restrict __pwc, const char *restrict __s, size_t __n,
			    locale_t)
{ return mbtowc(__pwc, __s, __n); }
inline size_t wcrtomb_l (char *restrict __s, wchar_t __wc, mbstate_t *restrict __ps, locale_t)
{return wcrtomb(__s, __wc, __ps); }
inline size_t mbrlen_l (const char *restrict __s, size_t __n, mbstate_t *restrict __ps, locale_t)
{ return mbrlen(__s, __n, __ps); }
inline size_t mbsrtowcs_l (wchar_t *restrict __dst, const char **restrict __src,
			      size_t __len, mbstate_t *restrict __ps, locale_t)
{ return mbsrtowcs(__dst, __src, __len, __ps); }
inline size_t wcsrtombs_l (char *restrict __dst, const wchar_t **restrict __src,
			      size_t __len, mbstate_t *restrict __ps, locale_t)
{ return wcsrtombs(__dst, __src, __len, __ps); }
inline size_t wcsnrtombs_l (char *restrict __dst, const wchar_t **restrict __src,
			      size_t __nms, size_t __len, mbstate_t *restrict __ps, locale_t)
{ return wcsnrtombs(__dst, __src, __nms, __len, __ps); }
inline size_t mbsnrtowcs_l (wchar_t *restrict __dst, const char **restrict __src,
			      size_t nms, size_t __len, mbstate_t *restrict __ps, locale_t)
{return mbsnrtowcs(__dst, __src, nms, __len, __ps); }

inline size_t strftime_l(char *restrict __s, size_t __maxsize,
                        char *restrict __fmt, const struct tm *restrict __t, locale_t)
{ return strftime( __s, __maxsize, __fmt, __t); }
inline struct lconv *  localeconv_l(locale_t) 
{ return localeconv(); }
#ifdef __cplusplus
}
#endif
