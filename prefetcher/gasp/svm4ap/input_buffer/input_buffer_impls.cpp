#include "input_buffer_impls.hpp"

std::optional<InputBufferEntry> StandardInputBuffer<InputBufferEntry>::read(uint64_t ip){
    auto entry = this->table.check_hit(InputBufferEntry(ip));
    return entry;
}

void StandardInputBuffer<InputBufferEntry>::write (InputBufferEntry entry){
    this->table.fill(entry);
}