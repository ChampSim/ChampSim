#pragma once
#include "prefetcher/gasp/svm4ap/global.hpp"


class SVM{
    public:
        virtual void fit(vector<double>& input, uint8_t output)= 0;
        virtual uint8_t predict(vector<double>& input)= 0;
        virtual void copyTo(shared_ptr<SVM>& svm)= 0;
        virtual void clean() = 0;
        // virtual double getCost() = 0;
        // virtual double getTotalCost() = 0;

};