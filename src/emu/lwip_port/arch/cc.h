/* Copyright (c) 2026 Ricardo Kerschbaumer
 * SPDX-License-Identifier: MIT
 *
 * lwIP's compiler and platform glue for SimNodus's Wi-Fi gateway (SN-268):
 * MSVC on Windows, GCC and Clang on Linux; little-endian hosts.
 */
#ifndef SIMNODUS_LWIP_ARCH_CC_H
#define SIMNODUS_LWIP_ARCH_CC_H

#include <stdio.h>
#include <stdlib.h>

#define LWIP_PLATFORM_DIAG(x) \
    do {                      \
    } while (0)
#define LWIP_PLATFORM_ASSERT(x)                                                   \
    do {                                                                          \
        fprintf(stderr, "lwIP assertion \"%s\" at %s:%d\n", x, __FILE__, __LINE__); \
        abort();                                                                  \
    } while (0)

#define LWIP_RAND() ((u32_t)rand())
#define LWIP_NO_UNISTD_H 1

/* Packed protocol headers. */
#ifdef _MSC_VER
#define PACK_STRUCT_USE_INCLUDES 1 /* arch/bpstruct.h, arch/epstruct.h */
#pragma warning(disable : 4127) /* conditional expression is constant */
#pragma warning(disable : 4996) /* unsafe C library functions */
#else
#define PACK_STRUCT_STRUCT __attribute__((packed))
#endif

#endif
