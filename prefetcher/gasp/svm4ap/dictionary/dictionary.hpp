#pragma once
#include  "svm4ap/global.hpp"



class Dictionary{
    public:
        virtual std::optional<uint8_t> read(int64_t delta) = 0;
        virtual std::optional<int64_t> read(uint8_t class_) = 0;
        virtual uint8_t write(int64_t delta) = 0;
        virtual void clean() = 0;
        virtual void copyTo(shared_ptr<Dictionary>&) = 0;
};