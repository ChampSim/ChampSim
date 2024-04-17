// remap_structure.h
#ifndef REMAP_STRUCTURE_H
#define REMAP_STRUCTURE_H
#include <algorithm>
#include <set>
#include <cstdint>

class RemapStructure
{
public:
    uint32_t temp;
    std::set<int32_t> remap_set;
    uint32_t *line;
    uint64_t access;
    uint32_t evicts;

    RemapStructure()
    {
        temp = 0;
        access = 0;
    }
};

#endif // REMAP_STRUCTURE_H