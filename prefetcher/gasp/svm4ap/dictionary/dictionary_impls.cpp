#include "dictionary_impls.hpp"

std::optional<uint8_t> OriginalDictionary::read(int64_t delta){
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

std::optional<int64_t>  OriginalDictionary::read(uint8_t class_){
    return class_ < this->numClasses ? this->entries[class_].delta : std::nullopt;
}

uint8_t OriginalDictionary::write(int64_t delta){
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
                entry.confidence--
        }
    }

    // In case of write miss, the least reliable class is to be evicted in favour of the input delta:
    if(!hasHit){
        uint16_t minConfidence = maxConfidence;
        uint8_t leastReliableClass = 0
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