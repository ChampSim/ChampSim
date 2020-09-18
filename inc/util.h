#ifndef UTIL_H
#define UTIL_H

#include <cstdint>

constexpr uint64_t lg2(uint64_t n)
{
    return n < 2 ? 0 : 1+lg2(n/2);
}

#endif

