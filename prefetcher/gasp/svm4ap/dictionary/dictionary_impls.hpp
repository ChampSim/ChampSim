#pragma once
#include "global.hpp"

struct OriginalDictionaryEntry{
    int64_t delta;
    uint16_t confidence;
}

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
        )
    {
        for(int i = 0; i < numClasses; i++){
            OriginalDictionaryEntry entry = {
                0, 0,
            }
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
            }
            this->entries.push_back(entry);
        }
    }

    OriginalDictionary(const& OriginalDictionary dictionary):
        numClasses(dictionary.numClasses), maxConfidence(dictionary.maxConfidence), 
        confidenceStep(dictionary.confidenceStep), initialConfidence(dictionary.initialConfidence),
        entries(vector<OriginalDictionaryEntry>(dictionary.entries))
    {}

    ~OriginalDictionary(){
        this->clean();
    }

    std::optional<uint8_t> read(int64_t delta);
    std::optional<int64_t> read(uint8_t class_);
    uint8_t write(int64_t delta);
    void clean(){
        entries = vector<OriginalDictionaryEntry>();
    }
    void copyTo(shared_ptr<Dictionary>& dictionary){
        dictionary = shared_ptr<Dictionary>((Dictionary*)
            new OriginalDictionary(*this);
        );  
    }
}