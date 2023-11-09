#include "input_buffer_impls.hpp"

template<typename InputBufferEntry>
std::optional<InputBufferEntry> StandardInputBuffer<InputBufferEntry>::read(uint64_t ip){
    auto entry = this->table.check_hit(InputBufferEntry(ip));
    return entry;
}

template<typename InputBufferEntry>
void StandardInputBuffer<InputBufferEntry>::write (InputBufferEntry entry){
    this->table.fill(entry);
}