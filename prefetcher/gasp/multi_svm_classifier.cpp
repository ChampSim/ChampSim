#include <random>
#include <vector>
#include <iostream>
#include <string>
#include <time.h>
#include "SVMClassifier.hpp"
/*
MultiSVMClassifier::MultiSVMClassifier() {
    this->numClasses = 0;
    this->c = 0;
    this->epochs = 0;
    this->numClasses = 0;
    this->seed = 0;
    this->learningRate = 0;

}
*/

MultiSVMClassifier::MultiSVMClassifier(int numFeatures, int numClasses, double c, unsigned int epochs, double learningRate){
    // MultiSVMClassifierType type) {

    this->numFeatures = numFeatures;
    this->numClasses = numClasses;
    this->c = c;
    this->epochs = epochs;
    this->seed = rand();
    this->learningRate = learningRate;

}

// MultiSVMClassifierOneToAll::MultiSVMClassifierOneToAll() : MultiSVMClassifier() {}

MultiSVMClassifierOneToOne::MultiSVMClassifierOneToOne() : MultiSVMClassifier() {}

MultiSVMClassifierOneToAll::MultiSVMClassifierOneToAll(int numFeatures, int numClasses, double c, unsigned int epochs, double learningRate)
    : MultiSVMClassifier(numFeatures, numClasses, c, epochs, learningRate) {

    for (int i = 0; i < numClasses; i++) {
        auto model = SVMSGDClassifier::SVMSGDClassifier(c, epochs, rand(), learningRate);
        model.initWeights(this->numFeatures);
        this->SVMsTable.push_back(model);
    }
        
}

MultiSVMClassifierOneToOne::MultiSVMClassifierOneToOne(int numFeatures, int numClasses, double c, unsigned int epochs, double learningRate)
    : MultiSVMClassifier(numFeatures, numClasses, c, epochs, learningRate) {

    for (int i = 0; i < numClasses; i++)
        for (int j = 0; j < numClasses; j++) {
            if (j >= i) {
                auto model = SVMSGDClassifier::SVMSGDClassifier(c, epochs, rand(), learningRate);
                model.initWeights(this->numFeatures);
                this->SVMsTable.push_back(model);

            }
            else {
                auto model = SVMSGDClassifier::SVMSGDClassifier();
                model.initWeights(this->numFeatures);
                this->SVMsTable.push_back(model);
            }
                
        }
            
}

void MultiSVMClassifier::initWeights(int numFeatures) {
    for (auto& svm : this->SVMsTable)
        svm.initWeights(numFeatures);
}

/*
void MultiSVMClassifierOneToAll::fit(vector<vector<double>> & data, vector<int> & label) {
    
    // For each classifier, we build the real-label vector fpr the entirety of given data:
    for (int k = 0; k < SVMsTable.size(); k++) {
        vector<int> realLabels = vector<int>();
        if (label.size() > 1) {
            std::transform(label.begin(), label.end(), realLabels.begin(),
                [k](int c) {return k == c ? -1 : +1; });
        }
        else realLabels = vector<int>{ k == label[0]? -1 : +1 };

        // Then, we fit each classifier with the resulting real-label vector:
        SVMsTable[k].fit(data, realLabels);
    }

}
*/

void MultiSVMClassifierOneToOne::fit(vector<vector<double>>& data, vector<int>& label) {

    // For each classifier, we build the real-label vector for the entirety of given data:
    
    for (int j = 0; j < this->numClasses; j++)
        for (int k = 0; k < this->numClasses; k++) {
            if (k > j) {
                int class0 = j, class1 = k;
                int predictorIndex = ((j * numClasses) + k);
                vector<int> realLabels = vector<int>();
                if (label.size() > 1) {
                    std::transform(label.begin(), label.end(), realLabels.begin(),
                        [class0, class1](int c) {
                            int res = 0;
                            if (c == class0) res = -1;
                            else if(c == class1) res = +1;
                            return res;
                        });
                }
                else {
                    int res = 0;
                    if (label[0] == class0) res = -1;
                    else if(label[0] == class1) res = +1;
                    realLabels = vector<int>{ res };
                }

                

                // Then, we fit each classifier with the resulting real-label vector:
                SVMsTable[predictorIndex].fit(data, realLabels);
            }
        }

}

/*
vector<int> MultiSVMClassifierOneToAll::predict(vector<vector<double>> & data) {
    vector<int> predicted_labels;

    vector<vector<int>> predictedLabelsPerPredictor;
    vector<vector<double>> distancesPerPredictor;
    // Each classifier gives its prediction for the given data:
    for (int k = 0; k < SVMsTable.size(); k++) {
        distancesPerPredictor.push_back(SVMsTable[k].computeDistanceToPlane(data));
        predictedLabelsPerPredictor.push_back(SVMsTable[k].predict(data));
    }

    // For each data sample, we append as label the first class which has been predicted against 
    // the rest:
    for (int i = 0; i < data.size(); i++) {

        int bestPrediction = 0;
        double bestDistance = -1;

        for (int k = 0; k < SVMsTable.size(); k++) {
            if (predictedLabelsPerPredictor[k][i] == -1) {
                if ((bestDistance == -1) || (distancesPerPredictor[k][i] < bestDistance)) {
                    bestPrediction = k;
                    bestDistance = distancesPerPredictor[k][i];
                }
            }
        }

        predicted_labels.push_back(bestPrediction);

    }


    return predicted_labels;
}
*/
vector<int> MultiSVMClassifierOneToOne::predict(vector<vector<double>>& data) {
    vector<int> predicted_labels;

    vector<vector<int>> predictedLabelsPerPredictor;
    // Each classifier gives its prediction for the given data:
    for (int k = 0; k < SVMsTable.size(); k++) {
        predictedLabelsPerPredictor.push_back(SVMsTable[k].predict(data));
    }

    for (int i = 0; i < data.size(); i++) {
        vector<int> predictionsHistogram = vector<int>(numClasses, 0);
        int maxPreds = -1;
        int bestPred = -1;

        // We iterate through the whole set of classifiers:
        for(int j = 0; j < this->numClasses; j++)
            for (int k = 0; k < this->numClasses; k++) {
                if (k > j) {
                    int class0 = j, class1 = k;
                    int winnerClass;
                    int predictorIndex = ((j * numClasses) + k) ;
                    if (predictedLabelsPerPredictor[predictorIndex][i] == -1) {
                        winnerClass = class0;
                    }
                    else {
                        winnerClass = class1;
                    }
                    
                    // We keep track of the predictions with a histogram:
                    predictionsHistogram[winnerClass]++;

                    // The current class with maximum number of predictions in total
                    // is selected as best predicted class:
                    if (predictionsHistogram[winnerClass] > maxPreds) {
                        maxPreds = predictionsHistogram[winnerClass];
                        bestPred = winnerClass;
                    }


                }
            }

        // We append the best predicted class as predicted label:
        predicted_labels.push_back(bestPred);


    }

    return predicted_labels;
}
