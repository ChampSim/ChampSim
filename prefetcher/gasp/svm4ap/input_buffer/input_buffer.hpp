#pragma once
#include  "svm4ap/global.hpp"
template<typename InputBufferEntry>
class InputBuffer{
    public:
        virtual std::optional<InputBufferEntry> read(uint64_t ip) = 0; // IP = Instruction Pointer = Instruction Program Counter
        virtual void write (InputBufferEntry entry) = 0;
        virtual void clean() = 0;
};

