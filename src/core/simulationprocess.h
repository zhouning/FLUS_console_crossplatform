#ifndef SIMULATIONPROCESS_H
#define SIMULATIONPROCESS_H

#define DAREA


#include <vector>
#include <iostream>

#ifdef _RANK
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>  
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/legacy/legacy.hpp>
#include <opencv2/nonfree/nonfree.hpp>
using namespace cv;
#endif 

class TiffDataRead;

using namespace std;


class SimulationProcess 
{

public:
	SimulationProcess(string _configfile);
	SimulationProcess(string _configfile,string _demandfile);
	bool runFLUS();
	bool ready() const;
	~SimulationProcess();


private:
	void init();
	bool readImageData();
	bool imageOpen(string filename);
	bool imageOpenConver2uchar(string filename);
	bool startloop();
	void runloop2();
	bool getparameters();
	bool getcellstatistic();
	void saveResult(string filename);
#ifdef _RANK
	void savePatchGeoImage(string filename,cv::Mat labelImg);
#endif 


private:
	size_t _rows;
	size_t _cols;
	int nType;
	int numWindows;
	int sizeWindows;
	int looptime;
	double degree;
	bool isRestrictExit;
	bool isSave;
	int isbreak;
	int* goalNum;
	int* mIminDis2goal;
	string savepath;
	bool isMultGoal;

	string configfile;
	string demandfile;

protected:
	vector<TiffDataRead*> imgList;
	vector<int> typeIndex;
	vector<int> multipleYear;
	vector<int> multiDemand;

	int* saveCount;
	int* val;
	double* mdNeiborhoodProbability;
	double* mdRoulette;
	double* probability;
	double* sProbability;
	double* normalProbability;
	double* mdNeighIntensity;
	unsigned char* temp;
	double** t_filecost;
	int** direction;
	short** Colour;
	bool initializationSucceeded;

};

#endif // SIMULATIONPROCESS_H
