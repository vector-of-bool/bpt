#pragma once

// clang-format off

/**
 * Header checks
 */
#if __has_include(<sys/mman.h>)
    #define HAVE_SYS_MMAN_H 1
#endif

#if __has_include(<sys/random.h>)
    #define HAVE_SYS_RANDOM_H 1
#endif

#if __has_include(<intrin.h>)
    #define HAVE_INTRIN_H 1
#endif

#if __has_include(<sys/auxv.h>)
    #define HAVE_SYS_AUXV_H 1
#endif

/**
 * Architectural checks for intrinsics
 */
#if __has_include(<mmintrin.h>) && __MMX__
    #define HAVE_MMINTRIN_H 1
#endif

#if __has_include(<emmintrin.h>) && __SSE2__
    #define HAVE_EMMINTRIN_H 1
#endif

#if __SSE3__
    #if __has_include(<pmmintrin.h>)
        #define HAVE_PMMINTRIN_H 1
    #endif
    #if __has_include(<tmmintrin.h>)
        #define HAVE_TMMINTRIN_H 1
    #endif
#endif

#if __has_include(<smmintrin.h>) && __SSE4_1__
    #define HAVE_SMMINTRIN_H
#endif

#if __has_include(<immintrin.h>)
    #if __AVX__
        #define HAVE_AVXINTRIN_H
    #endif
    #if __AVX2__
        #define HAVE_AVX2INTRIN_H
    #endif
    #if __AVX512F__
        #if defined(__clang__) && __clang_major__ < 4
            // AVX512 may be broken
        #elif defined(__GNUC__) && __GNUC__ < 6
            // ''
        #else
            #define HAVE_AVX512FINTRIN_H
        #endif
    #endif
#endif

#if __has_include(<wmmintrin.h>) && __AES__
    #define HAVE_WMMINTRIN_H 1
#endif

#if __RDRND__
    #define HAVE_RDRAND
#endif

/**
 * Detect mman APIs
 */
#if __has_include(<sys/mman.h>)
    #define HAVE_MMAP 1
    #define HAVE_MPROTECT 1
    #define HAVE_MLOCK 1

    #if defined(_DEFAULT_SOURCE) || defined(_BSD_SOURCE)
        #define HAVE_MADVISE 1
    #endif
#endif

#if __has_include(<sys/random.h>)
    #define HAVE_GETRANDOM 1
#endif

/**
 * POSIX-Only stuff
 */
#if __has_include(<unistd.h>)
    #if defined(_DEFAULT_SOURCE)
        #define HAVE_GETENTROPY 1
    #endif

    /**
     * Default POSIX APIs
     */
    #define HAVE_POSIX_MEMALIGN 1
    #define HAVE_GETPID 1
    #define HAVE_NANOSLEEP 1

    /**
     * Language/library features from C11
     */
    #if __STDC_VERSION__ >= 201112L
        #define HAVE_MEMSET_S 1
    #endif

    #if __linux__
        #define HAVE_EXPLICIT_BZERO 1
    #endif
#endif

/**
 * Miscellaneous
 */
#if __has_include(<pthread.h>)
    #define HAVE_PTHREAD 1
#endif

#if __has_include(<sys/param.h>)
    #include <sys/param.h>
    #if __BYTE_ORDER == __BIG_ENDIAN
        #define NATIVE_BIG_ENDIAN 1
    #elif __BYTE_ORDER == __LITTLE_ENDIAN
        #define NATIVE_LITTLE_ENDIAN 1
    #else
        #error "Unknown endianness for this platform."
    #endif
#elif defined(_MSVC)
    // At time of writing, MSVC only targets little-endian.
    #define NATIVE_LITTLE_ENDIAN 1
#else
    #error "Unknown endianness for this platform."
#endif

#define CONFIGURED 1
