#include <random>
#include <vector>
#include <iostream>
#include <string>
#include <time.h>
#include "SVMClassifier.hpp"

SVMClassifier::SVMClassifier() {

}

SVMClassifier::SVMClassifier(double c, unsigned int epochs, unsigned int seed) {
  this->c = c;
  this->epochs = epochs;
  this->seed = seed;
}

void SVMClassifier::initWeights(int numFeatures) {
    w.resize(numFeatures);
    b = 0;
}

void SVMClassifier::setWeights(vector<double> w, double b) {
    this->w = w;
    this->b = b;
}


void SVMClassifier::fit(vector<vector<double>> & data, vector<int> & label) {
    srand(seed);
    
    if(w.size() == 0)
        w.resize(data[0].size());
    
    for(unsigned int t = 1; t <= epochs; t++) {

        unsigned int idx = rand() % data.size();

        if (label[idx] != 0) {
            double nt = epochs == 1? 1 : 1 / (c * t);

            vector<double> xi = data[idx];

            vector<double> next_w(xi.size(), 0);

            double dot_product = 0;


            for (unsigned int i = 0; i < xi.size(); i++) {
                dot_product += w[i] * xi[i];
            }

            if (dot_product * label[idx] < 1) {
                for (unsigned int k = 0; k < xi.size(); k++) {
                    next_w[k] = w[k] - nt * c * w[k] + nt * label[idx] * xi[k];
                }
            }

            else //  if (dot_product * label[idx] > 0) {
                for (unsigned int k = 0; k < xi.size(); k++) {
                    next_w[k] = w[k]; // - nt * c * w[k];
                }
            //}
            
            w = next_w;
        }

        
    }

    // for(unsigned int i = 0; i < w.size(); i++) {
    //     cout << "w" << i << " = " << w[i] << endl;
    // }

    // cout << endl;
}


SVMSGDClassifier::SVMSGDClassifier() {

}

SVMSGDClassifier::SVMSGDClassifier(double c, unsigned int epochs, unsigned int seed, double learningRate) :
    SVMClassifier(c, epochs, seed)
{
    this->learningRate = learningRate;
}

void SVMSGDClassifier::initWeights(int numFeatures) {
    // w.resize(numFeatures + 1);
    w = vector<double>(numFeatures, 0.5);
    b = 0;
}


// https://towardsdatascience.com/svm-implementation-from-scratch-python-2db2fc52e5c2
double computeCost(vector<double>& w, double b, vector<vector<double>>& x, vector<int>& y, double c) {
    double hingeLoss = 0.0;
    for (int i = 0; i < x.size(); i++) {
        double dot_product = 0;
        vector<double> xi = x[i];
        for (unsigned int j = 0; j < xi.size(); j++) {
            dot_product += w[j] * xi[j];
        }

        double distance = 1 - y[i] * (dot_product - b);
        distance = distance < 0 ? 0 : distance;
        hingeLoss += distance;
    }
    hingeLoss = hingeLoss * c / x.size();

    double dot_product = 0;
    for (unsigned int j = 0; j < w.size(); j++) {
        dot_product += w[j] * w[j];
    }

    return (1 - c)*(dot_product / 2) + hingeLoss;
}

vector<double> SVMSGDClassifier::computeDistanceToPlane(vector<vector<double>>& x) {
    vector<double> res = vector<double>();
    for (int i = 0; i < x.size(); i++) {
        double dot_product = 0;
        vector<double> xi = x[i];
        for (unsigned int j = 0; j < xi.size(); j++) {
            dot_product += w[j] * xi[j];
        }

        double distance = abs(dot_product - b);
        res.push_back(distance);
    }
    return res;
}

#define RESOLUCION_RAND 1000
#define FACTOR_RAND 0.1

void computeGradients(vector<double>& w, double b, vector<vector<double>>& x, vector<int>& y, double c,
    vector<double>& dw, double* pointer_db) {
    vector<double> resultingGradient_w = vector<double>(w.size(), 0);
    double resultingGradient_b = 0.0;

    // double cost = computeCost(w, x, y, c);
    
    for (int i = 0; i < x.size(); i++) {
        vector<double> partialGradient_w = vector<double>(w.size(), 0);
        double partialGradient_b = 0.0;
        double dot_product = 0;
        vector<double> xi = x[i];
        for (unsigned int j = 0; j < xi.size(); j++) {
            dot_product += w[j] * xi[j];
        }

        double distance = 1 - y[i] * (dot_product - b);
        if (distance <= 0) {
            for (unsigned int j = 0; j < xi.size(); j++) {
                partialGradient_w[j] = w[j] * (1 - c);
            }
            partialGradient_b = 0.0;
            // partialGradient_w = w;// vector<double>(w.size(), 0); // = w;

        }
        else {

            for (unsigned int j = 0; j < xi.size(); j++) {
                partialGradient_w[j] = (w[j] * (1 - c)) - (c * y[i] * xi[j]);
            }
            partialGradient_b = c * y[i];
        }

        // We sum a small random vector:
        double norm = 0.0;
        for (unsigned int j = 0; j < xi.size(); j++) {
            norm += partialGradient_w[j] * partialGradient_w[j];
        }
        norm = sqrt(norm);

        for (unsigned int j = 0; j < xi.size(); j++) {
            double r = ((double)(rand() % RESOLUCION_RAND)) / RESOLUCION_RAND;
            r = (r - 0.5) * 2;
            r = r * FACTOR_RAND;
            // partialGradient_w[j] += norm * r;
        }

        for (unsigned int j = 0; j < xi.size(); j++) {
            resultingGradient_w[j] += partialGradient_w[j] / x.size();
        }
        resultingGradient_b += partialGradient_b / x.size();
    }

    dw = resultingGradient_w;
    *pointer_db = resultingGradient_b;
}

void SVMSGDClassifier::fit(vector<vector<double>>& data, vector<int>& label) {
    srand(seed);

    if (w.size() == 0)
        w.resize(data[0].size());

    for (unsigned int t = 1; t <= epochs; t++) {

        unsigned int idx = rand() % data.size();

        if (label[idx] != 0) {

            vector<double> xi = data[idx];
            auto xi_ = vector<vector<double>>{ xi };
            auto label_ = vector<int>{ label[idx] };

            vector<double> gradient_w = vector<double>();
            double gradient_b = 0.0;
            computeGradients(w, b, xi_, label_, c, gradient_w, &gradient_b);

            for (int j = 0; j < w.size(); j++) {
                w[j] = w[j] - (this->learningRate * gradient_w[j]);
            }
            b = b - (this->learningRate * gradient_b);
        }


    }

    // for(unsigned int i = 0; i < w.size(); i++) {
    //     cout << "w" << i << " = " << w[i] << endl;
    // }

    // cout << endl;
}

vector<int> SVMClassifier::predict(vector<vector<double>> & data) {
    vector<int> predicted_labels;

    for(unsigned int i = 0; i < data.size(); i++) {
        
        vector<double> xi = data[i];
        
        double dot_product = 0;
       
        for(unsigned int j = 0; j < xi.size(); j++) {   
            dot_product += w[j]*xi[j];
        }

        if ((dot_product - b) >= 0) {
            predicted_labels.push_back(1);
        }

        else predicted_labels.push_back(-1);
    }

    return predicted_labels;
}

double SVMClassifier::accuracy(vector<int> & label, vector<int> & pred_label) {
    int correct_pred = 0;

    
    for(unsigned int i = 0; i < label.size(); i++) {
        if (label[i] == pred_label[i]) {
            correct_pred += 1;
            //cout<< "acertou" <<endl;
        }
        //else cout<< "erou" <<endl;
    }

    return (double) correct_pred/label.size();
}


double SVMClassifier::accuracy(vector<vector<double>>& data, vector<int>& labels) {
    int correct_preds = 0;
    vector<int> pred_labels = this->predict(data);

    for (unsigned int i = 0; i < data.size(); i++) {
        
        if (labels[i] == pred_labels[i]) {
            correct_preds += 1;
            //cout<< "acertou" <<endl;
        }
        //else cout<< "erou" <<endl;
    }

    return (double)correct_preds / labels.size();
}