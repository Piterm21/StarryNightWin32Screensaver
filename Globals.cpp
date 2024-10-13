#include "Globals.h"

extern "C" {
    int _fltused = 0;
}

void* operator new(size_t sz)
{
    return VirtualAlloc(0, sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void* operator new[](size_t sz)
{
    return VirtualAlloc(0, sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void operator delete(void* ptr)
{
    VirtualFree(ptr, 0, MEM_RELEASE);
}

void operator delete[](void* ptr, size_t Size)
{
    VirtualFree(ptr, 0, MEM_RELEASE);
}

void operator delete[](void* ptr)
{
    VirtualFree(ptr, 0, MEM_RELEASE);
}

#pragma function(memset)
void* memset(void* Address, int32_t Value, size_t Size)
{
    int32_t* Temp = (int32_t*)Address;
    while ((uint64_t)(Temp - (int32_t*)Address) < Size / sizeof(int32_t))
    {
        *Temp = Value;
        Temp++;
    }

    uint64_t SizeReminder = Size % sizeof(int32_t);
    switch (SizeReminder)
    {
        case 3:
        {
            *(char*)Temp = (char)Value;
            (char*)Temp++;
        }
        case 2:
        {
            *(char*)Temp = (char)Value;
            (char*)Temp++;
        }
        case 1:
        {
            *(char*)Temp = (char)Value;
            (char*)Temp++;
        }
    }

    return Address;
}

static uint64_t s[2] = { 1, 1 };

uint64_t xoroshiro128plus(void)
{
    const auto rotl = [](const uint64_t x, int k) { return (x << k) | (x >> (64 - k)); };

    const uint64_t s0 = s[0];
    uint64_t s1 = s[1];
    const uint64_t result = s0 + s1;

    s1 ^= s0;
    s[0] = rotl(s0, 24) ^ s1 ^ (s1 << 16); // a, b
    s[1] = rotl(s1, 37); // c

    return result;
}

void SeedRandom(uint64_t Seed)
{
    s[0] = { 1 };
    s[1] = Seed;
}

float RandomFloat()
{
    union U { uint32_t I; float F; };
    return U{ uint32_t{0x3F800000u} | static_cast<uint32_t>(xoroshiro128plus()) & ((uint32_t{1} << 23) - uint32_t{1}) }.F - 1.0f;
}