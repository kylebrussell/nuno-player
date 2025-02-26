/* config.h for libFLAC */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `lround' function. */
#define HAVE_LROUND 1

/* Define to 1 if compiler has visibility support */
#define HAVE_VISIBILITY 1

/* Define to the size of a long in bits. */
#define SIZEOF_LONG 8

/* Define to the size of a long long in bits. */
#define SIZEOF_LONG_LONG 8

/* Define to the size of a void* in bits. */
#define SIZEOF_VOIDP 8

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
#undef WORDS_BIGENDIAN

/* Define to 1 if type `char' is unsigned and your compiler does not
   understand `-fsigned-char'. */
#undef __CHAR_UNSIGNED__

/* Define to empty if `const' does not conform to ANSI C. */
#undef const

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
#undef inline
#endif

/* Define to `long int' if <sys/types.h> does not define. */
#undef off_t

/* Define to `unsigned int' if <sys/types.h> does not define. */
#undef size_t

/* Define to disable use of assembly code */
#define FLAC__NO_ASM

/* Define to disable use of C++ code */
#define FLAC__NO_CPLUSPLUS

/* Define to 1 if `lex' declares `yytext' as a `char *' by default, not a
   `char[]'. */
#undef YYTEXT_POINTER 