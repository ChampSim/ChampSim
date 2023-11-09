#pragma once
#include  "svm4ap/global.hpp"
#include "backend/SVMClassifier.hpp"


class StandardSVM : public SVM{

    public:
        uint8_t inputSize;
        uint8_t numOutputClasses;
        double c;
        double learningRate;

        MultiSVMClassifierOneToAll model;

        StandardSVM(){
            this->inputSize = 0;
            this->numOutputClasses = 0;
            this->c = 1.0;
            this->learningRate = 0.5;
            this->model = MultiSVMClassifierOneToAll();
        }

        StandardSVM(uint8_t inputSize, uint8_t numOutputClasses,
            double c = 1.0, double learningRate = 0.5){
            this->inputSize = inputSize;
            this->numOutputClasses = numOutputClasses;
            this->c = c;
            this->learningRate = learningRate;
            
            this->model = MultiSVMClassifierOneToAll(inputSize, numOutputClasses, c, 1, learningRate);
        }

        StandardSVM(const StandardSVM& svm){
            this->inputSize = svm.inputSize;
            this->numOutputClasses = svm.numOutputClasses;
            this->c = svm.c;
            this->learningRate = svm.learningRate;
            this->model = svm.model;
        }

        ~StandardSVM(){
            this->clean();
        }

        void copyTo(std::shared_ptr<SVM>& svm){
            svm = std::shared_ptr<SVM>(
                (SVM*) new StandardSVM(*this)
            );
        }

        void clean(){
            this->model.SVMsTable = vector<SVMSGDClassifier>();
        }

        void fit(vector<double>& input, uint8_t output);
        uint8_t predict(vector<double>& input);
        
};