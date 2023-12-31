#pragma once
//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.
//
// Accessed from https://github.com/aappleby/smhasher/ on 2023 Nov 15 with git
// hash 92cf3702fcfaadc84eb7bef59825a23e0cd84f56

#ifndef _MURMURHASH3_H_
#define _MURMURHASH3_H_

//-----------------------------------------------------------------------------
// Platform-specific functions and macros

// Microsoft Visual Studio

#if defined(_MSC_VER) && (_MSC_VER < 1600)

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;

// Other compilers

#else // defined(_MSC_VER)

#include <stdint.h>

#endif // !defined(_MSC_VER)

//-----------------------------------------------------------------------------

void
MurmurHash3_x86_32(const void *key, int len, uint32_t seed, void *out);

void
MurmurHash3_x86_128(const void *key, int len, uint32_t seed, void *out);

void
MurmurHash3_x64_128(const void *key, int len, uint32_t seed, void *out);

//-----------------------------------------------------------------------------

#endif // _MURMURHASH3_H_
