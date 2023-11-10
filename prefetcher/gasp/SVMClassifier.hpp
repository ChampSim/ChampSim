#ifndef SVM_CLASS_H
#define SVM_CLASS_H

#include <vector>
#include <time.h>
#include <math.h>

using namespace std;

template<typename T>
string vectorToString(vector<T> v);

class SVMClassifier {
protected:
	vector<double> w;
	double b;
public:
  double c;
  unsigned int epochs;
  unsigned int seed;
  

  SVMClassifier(){}

  SVMClassifier(double c, unsigned int epochs, unsigned int seed){
	this->c = c;
	this->epochs = epochs;
	this->seed = seed;
	}

  void setWeights(vector<double> w, double b);

  void initWeights(int numFeatures);

  void fit(vector<vector<double>> & data, vector<int> & label);

  vector<int> predict(vector<vector<double>> & data){
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

  double  accuracy(vector<int> & label, vector<int> & pred_label);

  double accuracy(vector<vector<double>>& data, vector<int>& label);

};


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
        if (distance < 0) {
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


class SVMSGDClassifier : public SVMClassifier {
public:
	double learningRate;
	SVMSGDClassifier(){}
	SVMSGDClassifier(double c, unsigned int epochs, unsigned int seed, double learningRate):
		SVMClassifier(c, epochs, seed)
	{
		this->learningRate = learningRate;
	}
	void initWeights(int numFeatures){
		// w.resize(numFeatures + 1);
		w = vector<double>(numFeatures, 0.5);
		b = 0.5; // 0.0
	}
	void fit(vector<vector<double>>& data, vector<int>& label){
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
				cout << vectorToString(label_) << "\n";
				for (int j = 0; j < w.size(); j++) {
					w[j] = w[j] - (this->learningRate * gradient_w[j]);
					cout << to_string((this->learningRate * gradient_w[j])) << " ";
				}
				b = b - (this->learningRate * gradient_b);
				cout << "; " << to_string((this->learningRate * gradient_b)) << " ";
				cout << "\n";
			}


		}

		// for(unsigned int i = 0; i < w.size(); i++) {
		//     cout << "w" << i << " = " << w[i] << endl;
		// }

		// cout << endl;
	}
	vector<double> computeDistanceToPlane(vector<vector<double>>& x) {
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
};

enum class MultiSVMClassifierType { OneToOne, OneToAll };

class MultiSVMClassifier : public SVMSGDClassifier{
private:
	vector<double> w;
	unsigned int seed;

public:
	int numClasses;

	int numFeatures;


	vector<SVMSGDClassifier> SVMsTable;

	MultiSVMClassifier(){
		this->numClasses = 0;
		this->c = 0;
		this->epochs = 0;
		this->numClasses = 0;
		this->seed = 0;
		this->learningRate = 0;

	}
	
	
	MultiSVMClassifier(int numFeatures, int numClasses, double c, unsigned int epochs, double learningRate){
    // MultiSVMClassifierType type) {

		this->numFeatures = numFeatures;
		this->numClasses = numClasses;
		this->c = c;
		this->epochs = epochs;
		this->seed = rand();
		this->learningRate = learningRate;

	}
		//MultiSVMClassifierType type = MultiSVMClassifierType::OneToOne);

	void initWeights(int numFeatures);

	vector<int> predict(vector<vector<double>>& data);


	void fit(vector<vector<double>>& data, vector<int>& label);


};

class MultiSVMClassifierOneToOne : public MultiSVMClassifier {

public:

	MultiSVMClassifierOneToOne();
	
	MultiSVMClassifierOneToOne(int numFeatures, int numClasses, double c, unsigned int epochs, double learningRate);

	vector<int> predict(vector<vector<double>>& data);

	void fit(vector<vector<double>>& data, vector<int>& label);

};

class MultiSVMClassifierOneToAll : public MultiSVMClassifier {

public:

	MultiSVMClassifierOneToAll(){}

	MultiSVMClassifierOneToAll(int numFeatures, int numClasses, double c, unsigned int epochs, double learningRate): 
		MultiSVMClassifier(numFeatures, numClasses, c, epochs, learningRate) {

    for (int i = 0; i < numClasses; i++) {
        auto model = SVMSGDClassifier(c, epochs, rand(), learningRate);
        model.initWeights(this->numFeatures);
        this->SVMsTable.push_back(model);
    }
        
}

	vector<int> predict(vector<vector<double>>& data){
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

	void fit(vector<vector<double>>& data, vector<int>& label){
		
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

};

#endif