#ifndef NNTRAIN_H
#define NNTRAIN_H

#include <gdal.h>
#include <string>
#include <vector>
#include <iostream>

using namespace std;

class TiffDataRead;

struct redata1
{
	std::string str;
	int num;
};

struct bandInfo
{
	size_t dataNum;
	size_t bandCom;
};

class NNtrain 
{


public:
	NNtrain(string _configimage);
	NNtrain(string _configimage,string _updatefile);
	~NNtrain();
	bool ready() const;

public:
	void trainprocess();


private:
	void init();
	void nnTrain();
	bool normalizationdata();
	bool getcellstatistic();
	bool setRandomPoint(bool _stra);
	size_t randomFunction(size_t _lon);
	template<class TT> bool dataCopyConvert(unsigned char* buffer);
	void saveNormalizedData();
	bool imageOpen(std::string filename);
	bool imageOpenConver2uchar(std::string filename);
	bool getAllParameter();

	bool normalizeUpdateData();
	template<class TT> bool dataCopyConvertUpdate(unsigned char* buffer);
	void updateDataprepared();
	void updatePredictData();


private:
	int* m_PointCoodinateX;
	int* m_PointCoodinateY;
	size_t nWidth;
	size_t nHeight;
 	size_t bandCount;
	size_t updatebandCount;
	int nData;

 	size_t currentBand;
 	size_t currentPos;
	size_t numof;
	vector<double> minmaxsave;
	vector<double> minmaxsaveupdate;

	float* f_allBandData;
	double* d_allBandData;
	unsigned short* us_allBandData;

	float* f_allBandData_update;
	double* d_allBandData_update;
	unsigned short* us_allBandData_update;

private:
	vector<int> mvLanduseType;
	vector<int> mvCountType;

	std::string pathoflanduse;
	std::string pathofsimresult;
	vector<std::string> pathofdivingfactor;
	vector<redata1> qlredat;
	vector<bandInfo> qlbio;
	string datatype;
	bool isNomalized;
	bool isUnifomSam;
	bool isNoDataExit;
	bool isUpdateData;
	double noDataValue;
	double samplingRate;
	int numHiddenLayer;
	vector<TiffDataRead*> imgList;
	vector<TiffDataRead*> divingList;
	vector<TiffDataRead*> updatedivingList;
	vector<std::string> bandName;
	vector<std::string> updatebandName;

	float* saveMemF;
	double* saveMemD;
	unsigned short* saveMemUs;

	string configimage;
	string updatefile;
	bool initializationSucceeded;

};

#endif // NNTRAINTHREAD_H
