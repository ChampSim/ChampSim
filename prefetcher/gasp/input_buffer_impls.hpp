#pragma once
#include  "global.hpp"

#include "cache.h"
#include "msl/lru_table.h"

class OriginalInputBufferEntry {
    public:
    uint64_t ip = 0;           
    uint64_t lastAddress = 0; 
    vector<uint8_t> classSequence = vector<uint8_t>(); 
    
    OriginalInputBufferEntry(){

    }

    OriginalInputBufferEntry(uint64_t ip){
        this->ip = ip;
    }

    OriginalInputBufferEntry(uint64_t ip, uint64_t lastAddress, vector<uint8_t> classSequence){
        this->ip = ip;
        this->lastAddress = lastAddress;
        this->classSequence = vector<uint8_t>(classSequence);
    }

    auto index() const { return ip; }
    auto tag() const { return ip; }
  };

class ConfidenceInputBufferEntry{
    public:
    uint64_t ip;
    uint64_t lastAddress;
    vector<uint8_t> classSequence;  
    uint8_t predictedClass;
    uint8_t confidence;

     
    ConfidenceInputBufferEntry(){
        ip = 0;           
        lastAddress = 0; 
        classSequence = vector<uint8_t>();  
        predictedClass = 0;
        confidence = 0;
    }

    ConfidenceInputBufferEntry(uint64_t ip){
        this->ip = ip;
        ip = 0;           
        lastAddress = 0; 
        classSequence = vector<uint8_t>();  
        predictedClass = 0;
        confidence = 0;
    }

    ConfidenceInputBufferEntry(uint64_t ip, uint64_t lastAddress, vector<uint8_t> classSequence,
        uint8_t predictedClass,
        uint8_t confidence){
        this->ip = ip;
        this->lastAddress = lastAddress;
        this->classSequence = vector<uint8_t>(classSequence);
        this->predictedClass = predictedClass;
        this->confidence = confidence;
    }


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

        std::optional<InputBufferEntry> read(uint64_t ip) // IP = Instruction Pointer = Instruction Program Counter
        {
            auto entry = this->table.check_hit(InputBufferEntry(ip));
            return entry;
        }
        void write (InputBufferEntry entry){
            this->table.fill(entry);
        }
};

