#pragma once
#include "global.hpp"

#include "cache.h"
#include "msl/lru_table.h"

class OriginalInputBufferEntry {
    uint64_t ip = 0;           
    uint64_t lastAddress = 0; 
    vector<uint8_t> classSequence = vector<int>();  

    OriginalInputBufferEntry(uint64_t ip) {
        this->ip = ip; 
    }

    OriginalInputBufferEntry(uint64_t ip, uint64_t lastAddress, int sequenceSize, int numClasses) {
        this->ip = ip; 
        this->lastAddress = lastAddress;
        this->classSequence = vector<uint8_t>(sequenceSize, numClasses);
    }

    auto index() const { return ip; }
    auto tag() const { return ip; }
  };

class ConfidenceInputBufferEntry : public OriginalInputBufferEntry{
    uint64_t ip = 0;           
    uint64_t lastAddress = 0; 
    vector<uint8_t> classSequence = vector<int>();  
    uint8_t predictedClass = 0;
    uint8_t confidence = 0;

    ConfidenceInputBufferEntry(uint64_t ip) {
        this->ip = ip; 
    }

    ConfidenceInputBufferEntry(uint64_t ip, uint64_t lastAddress, int sequenceSize, int numClasses, uint8_t confidence) {
        this->ip = ip; 
        this->lastAddress = lastAddress;
        this->classSequence = vector<uint8_t>(sequenceSize, numClasses);
        this->predictedClass = 0;
        this->confidence = 0;
    }

    auto index() const { return ip; }
    auto tag() const { return ip; }
  };

template<typename InputBufferEntry>
class StandardInputBuffer : public InputBuffer{
    public:
        int numSets;
        int numWays;
        int sequenceSize;
        int numClasses;
        champsim::msl::lru_table<InputBufferEntry> table;

        StandardInputBuffer() : numSets(0), numWays(0), sequenceSize(0), numClasses(0), 
            table(lru_table<InputBufferEntry>(0,0)){}

        StandardInputBuffer(int numSets, int numWays, int sequenceSize, int numClasses) :
            numSets(numSets), numWays(numWays), sequenceSize(sequenceSize), numClasses(numClasses), 
            table(lru_table<InputBufferEntry>(numSets,numWays)){}

        ~StandardInputBuffer(){
            this->clean();
        }

        void clean(){
            this->table = lru_table<InputBufferEntry>(0,0);
        }

        std::optional<InputBufferEntry> read(uint64_t ip); // IP = Instruction Pointer = Instruction Program Counter
        void write (InputBufferEntry entry);
        
};

