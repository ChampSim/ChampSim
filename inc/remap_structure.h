// remap_structure.h

#ifndef REMAP_STRUCTURE_H
#define REMAP_STRUCTURE_H

#include <vector>

class RemapStructure {
public:
    uint32_t set;
    int remap_set;
    int size;

    RemapStructure() {
        remap_set = -1;
        set = 0;
        size = 0;
    }
};

#endif // REMAP_STRUCTURE_H