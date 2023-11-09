#include "svm_impls.hpp"

void StandardSVM::fit(vector<double>& input, uint8_t output){
    auto in = vector<vector<double>>{ vector<double>(input.begin(), input.end()) };
    auto out = vector<int>{ output };
    this->model.fit(in, out);
}

uint8_t StandardSVM::predict(vector<double>& input){
    auto in = vector<vector<double>>{ vector<double>(input.begin(), input.end()) };
    return this->model.predict(in)[0];
}

