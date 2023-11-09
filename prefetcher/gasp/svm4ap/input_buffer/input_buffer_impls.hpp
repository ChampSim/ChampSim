#pragma once
#include  "svm4ap/global.hpp"

#include "cache.h"
#include "msl/lru_table.h"

struct OriginalInputBufferEntry {
    uint64_t ip = 0;           
    uint64_t lastAddress = 0; 
    vector<uint8_t> classSequence = vector<uint8_t>();  

    auto index() const { return ip; }
    auto tag() const { return ip; }
  };

struct ConfidenceInputBufferEntry{
    uint64_t ip = 0;           
    uint64_t lastAddress = 0; 
    vector<uint8_t> classSequence = vector<uint8_t>();  
    uint8_t predictedClass = 0;
    uint8_t confidence = 0;

    auto index() const { return ip; }
    auto tag() const { return ip; }
  };

template<typename InputBufferEntry>
class StandardInputBuffer : public InputBuffer<InputBufferEntry>{
    public:
        int numSets;
        int numWays;
        int sequenceSize;
        int numClasses;
        champsim::msl::lru_table<InputBufferEntry> table;

        StandardInputBuffer() : numSets(0), numWays(0), sequenceSize(0), numClasses(0), 
            table(champsim::msl::lru_table<InputBufferEntry>(0,0)){}

        StandardInputBuffer(int numSets, int numWays, int sequenceSize, int numClasses) :
            numSets(numSets), numWays(numWays), sequenceSize(sequenceSize), numClasses(numClasses), 
            table(champsim::msl::lru_table<InputBufferEntry>(numSets,numWays)){}

        ~StandardInputBuffer(){
            this->clean();
        }

        void clean(){
            this->table = champsim::msl::lru_table<InputBufferEntry>(0,0);
        }

        std::optional<InputBufferEntry> read(uint64_t ip); // IP = Instruction Pointer = Instruction Program Counter
        void write (InputBufferEntry entry);
        
};

