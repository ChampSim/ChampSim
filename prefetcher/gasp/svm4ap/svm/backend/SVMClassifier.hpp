#ifndef SVM_CLASS_H
#define SVM_CLASS_H

#include <vector>
#include <time.h>

using namespace std;

class SVMClassifier {
protected:
	vector<double> w;
	double b;
public:
  double c;
  unsigned int epochs;
  unsigned int seed;
  

  SVMClassifier();

  SVMClassifier(double c, unsigned int epochs, unsigned int seed);

  void setWeights(vector<double> w, double b);

  void initWeights(int numFeatures);

  void fit(vector<vector<double>> & data, vector<int> & label);

  vector<int> predict(vector<vector<double>> & data);

  double  accuracy(vector<int> & label, vector<int> & pred_label);

  double accuracy(vector<vector<double>>& data, vector<int>& label);

};

class SVMSGDClassifier : public SVMClassifier {
public:
	double learningRate;
	SVMSGDClassifier();
	SVMSGDClassifier(double c, unsigned int epochs, unsigned int seed, double learningRate);
	void initWeights(int numFeatures);
	void fit(vector<vector<double>>& data, vector<int>& label);
	vector<double> computeDistanceToPlane(vector<vector<double>>& data);
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

	MultiSVMClassifier();
	
	MultiSVMClassifier(int numFeatures, int numClasses, double c, unsigned int epochs, double learningRate);
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

	MultiSVMClassifierOneToAll();

	MultiSVMClassifierOneToAll(int numFeatures, int numClasses, double c, unsigned int epochs, double learningRate);

	vector<int> predict(vector<vector<double>>& data);

	void fit(vector<vector<double>>& data, vector<int>& label);

};

#endif