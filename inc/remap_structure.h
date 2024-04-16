// remap_structure.h
#ifndef REMAP_STRUCTURE_H
#define REMAP_STRUCTURE_H
#include <algorithm>

class RemapStructure
{
public:
    uint32_t temp;
    int32_t remap_set;
    int32_t parent;
    uint32_t *line;
    uint64_t access;

    RemapStructure()
    {
        temp = 0;
        remap_set = -1;
        parent = -1;
        access = 0;
    }

    // static bool compareAccess(const RemapStructure& a, const RemapStructure& b) {
    //     return a.access > b.access;
    // }

    // static void map_sets(RemapStructure *remap)
    // {
    //     sort(remap, remap + 2048, compareAccess);
    //     for(int i=0;i<1024;i++){
    //         remap[i].remap_set = 2047-i;
    //     }
    // }

};

#endif // REMAP_STRUCTURE_H