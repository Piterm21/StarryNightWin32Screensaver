#pragma once

#define WIN32_LEAN_AND_MEAN 1
#include "Windows.h"
#undef WIN32_LEAN_AND_MEAN

#include <stdint.h>

void* operator new(size_t sz);
void* operator new[](size_t sz);
void operator delete(void* ptr);
void operator delete[](void* ptr, size_t Size);
void operator delete[](void* ptr);

//Most likely way slower than intrinsic version
void* memset(void* Address, int32_t Value, size_t Size);
#pragma intrinsic(memset)

// From http://xoroshiro.di.unimi.it/xoroshiro128plus.c
uint64_t xoroshiro128plus(void);

void SeedRandom(uint64_t Seed);
float RandomFloat();