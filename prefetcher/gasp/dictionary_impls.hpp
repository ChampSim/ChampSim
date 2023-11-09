#pragma once
#include "global.hpp"

struct OriginalDictionaryEntry{
    int64_t delta;
    uint16_t confidence;
};

class OriginalDictionary : public Dictionary{
public:
    uint8_t numClasses;
    uint16_t maxConfidence;
    uint16_t confidenceStep;
    uint16_t initialConfidence;

    vector<OriginalDictionaryEntry> entries;

    OriginalDictionary() : 
        numClasses(0), maxConfidence(0), confidenceStep(0), initialConfidence(0), entries(vector<OriginalDictionaryEntry>())
    {
    }

    OriginalDictionary(uint8_t numClasses) : 
        numClasses(numClasses), maxConfidence(255), confidenceStep(32), initialConfidence(64)
        
    {
        for(int i = 0; i < numClasses; i++){
            OriginalDictionaryEntry entry = {
                0, 0,
            };
            this->entries.push_back(entry);
        }
    }

    OriginalDictionary(uint8_t numClasses, uint16_t maxConfidence, uint16_t confidenceStep, uint16_t initialConfidence) : 
        numClasses(numClasses), maxConfidence(maxConfidence), 
        confidenceStep(confidenceStep), initialConfidence(initialConfidence)
        
    {
        for(int i = 0; i < numClasses; i++){
            OriginalDictionaryEntry entry = {
                0, 0,
            };
            this->entries.push_back(entry);
        }
    }

    OriginalDictionary(const OriginalDictionary& dictionary):
        numClasses(dictionary.numClasses), maxConfidence(dictionary.maxConfidence), 
        confidenceStep(dictionary.confidenceStep), initialConfidence(dictionary.initialConfidence),
        entries(vector<OriginalDictionaryEntry>(dictionary.entries))
    {}

    ~OriginalDictionary(){
        this->clean();
    }

    std::optional<uint8_t> read(int64_t delta){
        std::optional<uint8_t> res = std::nullopt;
        for(int i = 0; i < this->numClasses; i++){
            auto& entry = this->entries[i];
            if(delta == entry.delta){
                res = i;
                break;
            }
        }
        return res;
    }
    std::optional<int64_t> read(uint8_t class_){
        std::optional<int64_t> res = std::nullopt;
        if(class_ < this->numClasses)
            res = this->entries[class_].delta;
        return res;
    }

    uint8_t write(int64_t delta){
        bool hasHit = false;
        uint8_t res = 0;

        // We look for a hit. In such case, only confidence is updated:
        for(int i = 0; i < this->numClasses; i++){
            auto& entry = this->entries[i];
            if(!hasHit && delta == entry.delta){
                res = i;
                hasHit = true;

                entry.confidence += this->confidenceStep;
                if(entry.confidence > this->maxConfidence)
                    entry.confidence = maxConfidence;
            }
            else{
                if(entry.confidence > 0)
                    entry.confidence--;
            }
        }

        // In case of write miss, the least reliable class is to be evicted in favour of the input delta:
        if(!hasHit){
            uint16_t minConfidence = maxConfidence;
            uint8_t leastReliableClass = 0;
            for(int i = 0; i < this->numClasses; i++){
                auto& entry = this->entries[i];
                if(entry.confidence < minConfidence){
                    minConfidence = entry.confidence;
                    leastReliableClass = i;
                }
            }

            res = leastReliableClass;
            this->entries[res].delta = delta;
            this->entries[res].confidence = this->initialConfidence;
        }

        return res;
    }
    void clean(){
        entries = vector<OriginalDictionaryEntry>();
    }
    void copyTo(shared_ptr<Dictionary>& dictionary){
        dictionary = shared_ptr<Dictionary>((Dictionary*)
            new OriginalDictionary(*this)
        );  
    }
};