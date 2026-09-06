#include "simulationprocess.h"
#include "TiffDataRead.h"
#include "TiffDataWrite.h"
#include <iomanip>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include "platform_compat.h"
#include <assert.h>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <ctime>

#ifdef _RANK
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/legacy/legacy.hpp>
#include <opencv2/nonfree/nonfree.hpp>
using namespace cv;
#endif

#include <stack>
#include<numeric>

using namespace std;

namespace {

unsigned int resolveRandomSeed()
{
	const char* envSeed = getenv("FLUS_RANDOM_SEED");
	if (envSeed != NULL && envSeed[0] != '\0')
	{
		char* end = NULL;
		errno = 0;
		unsigned long parsed = strtoul(envSeed, &end, 10);
		if (errno == 0 && end != envSeed && *end == '\0' && parsed <= UINT_MAX)
		{
			cout<<"FLUS random seed: "<<parsed<<" (FLUS_RANDOM_SEED)"<<endl;
			return static_cast<unsigned int>(parsed);
		}
		cout<<"Invalid FLUS_RANDOM_SEED '"<<envSeed<<"'; falling back to time seed"<<endl;
	}
	unsigned int seed = static_cast<unsigned int>(time(NULL));
	cout<<"FLUS random seed: "<<seed<<" (time)"<<endl;
	return seed;
}

void seedRandomOnce()
{
	static bool seeded = false;
	if (seeded)
	{
		return;
	}
	srand(resolveRandomSeed());
	seeded = true;
}

}


SimulationProcess::SimulationProcess(string _configfile)
{
	init();
	configfile=_configfile;
	isMultGoal=false;

	if (!readImageData())
		cout << "read image error"<<endl;

	_rows=imgList[0]->rows();
	_cols=imgList[0]->cols();
	isbreak=0;
	temp = new unsigned char[_cols*_rows];
}

SimulationProcess::SimulationProcess(string _configfile,string _demandfile)
{
	init();
	configfile=_configfile;
	demandfile=_demandfile;
	isMultGoal=true;

	if (!readImageData())
		cout << "read image error"<<endl;

	_rows=imgList[0]->rows();
	_cols=imgList[0]->cols();
	isbreak=0;
	temp = new unsigned char[_cols*_rows];
}

template<typename T>
string num2string(T _num)
{
	stringstream ss;
	string s;
	ss << _num;
	ss >> s;
	return s;
}

#ifdef _RANK

void icvprCcaByTwoPass(const cv::Mat& _binImg, cv::Mat& _lableImg)  
{  
	// connected component analysis (4-component)  
	// use two-pass algorithm  
	// 1. first pass: label each foreground pixel with a label  
	// 2. second pass: visit each labeled pixel and merge neighbor labels  
	//   
	// foreground pixel: _binImg(x,y) = 1  
	// background pixel: _binImg(x,y) = 0  


	if (_binImg.empty() ||  
		_binImg.type() != CV_8UC1)  
	{  
		return ;  
	}  

	// 1. first pass  

	_lableImg.release() ;  
	_binImg.convertTo(_lableImg, CV_32SC1) ;  

	int label = 1 ;  // start by 2  
	std::vector<int> labelSet ;  
	labelSet.push_back(0) ;   // background: 0  
	labelSet.push_back(1) ;   // foreground: 1  

	int rows = _binImg.rows - 1 ;  
	int cols = _binImg.cols - 1 ;  
	for (int i = 1; i < rows; i++)  
	{  
		int* data_preRow = _lableImg.ptr<int>(i-1) ;  
		int* data_curRow = _lableImg.ptr<int>(i) ;  
		for (int j = 1; j < cols; j++)  
		{  
			if (data_curRow[j] == 1)  
			{  
				std::vector<int> neighborLabels ;  
				neighborLabels.reserve(2) ;  
				int leftPixel = data_curRow[j-1] ;  
				int upPixel = data_preRow[j] ;  
				if ( leftPixel > 1)  
				{  
					neighborLabels.push_back(leftPixel) ;  
				}  
				if (upPixel > 1)  
				{  
					neighborLabels.push_back(upPixel) ;  
				}  

				if (neighborLabels.empty())  
				{  
					labelSet.push_back(++label) ;  // assign to a new label  
					data_curRow[j] = label ;  
					labelSet[label] = label ;  
				}  
				else  
				{  
					std::sort(neighborLabels.begin(), neighborLabels.end()) ;  
					int smallestLabel = neighborLabels[0] ;    
					data_curRow[j] = smallestLabel ;  

					// save equivalence  
					for (size_t k = 1; k < neighborLabels.size(); k++)  
					{  
						int tempLabel = neighborLabels[k] ;  
						int& oldSmallestLabel = labelSet[tempLabel] ;  
						if (oldSmallestLabel > smallestLabel)  
						{                             
							labelSet[oldSmallestLabel] = smallestLabel ;  
							oldSmallestLabel = smallestLabel ;  
						}                         
						else if (oldSmallestLabel < smallestLabel)  
						{  
							labelSet[smallestLabel] = oldSmallestLabel ;  
						}  
					}  
				}                 
			}  
		}  
	}  

	// update equivalent labels  
	// assigned with the smallest label in each equivalent label set  
	for (size_t i = 2; i < labelSet.size(); i++)  
	{  
		int curLabel = labelSet[i] ;  
		int preLabel = labelSet[curLabel] ;  
		while (preLabel != curLabel)  
		{  
			curLabel = preLabel ;  
			preLabel = labelSet[preLabel] ;  
		}  
		labelSet[i] = curLabel ;  
	}  


	// 2. second pass  
	for (int i = 0; i < rows; i++)  
	{  
		int* data = _lableImg.ptr<int>(i) ;  
		for (int j = 0; j < cols; j++)  
		{  
			int& pixelLabel = data[j] ;  
			pixelLabel = labelSet[pixelLabel] ;   
		}  
	}  
}


void icvprCcaBySeedFill(const cv::Mat& _binImg, cv::Mat& _lableImg)  
{  
	// connected component analysis (4-component)  
	// use seed filling algorithm  
	// 1. begin with a foreground pixel and push its foreground neighbors into a stack;  
	// 2. pop the top pixel on the stack and label it with the same label until the stack is empty  
	//   
	// foreground pixel: _binImg(x,y) = 1  
	// background pixel: _binImg(x,y) = 0  


	if (_binImg.empty() ||  
		_binImg.type() != CV_8UC1)  
	{  
		return ;  
	}  

	_lableImg.release() ;  
	_binImg.convertTo(_lableImg, CV_32SC1) ;  

	int label = 1 ;  // start by 2  

	int rows = _binImg.rows - 1 ;  
	int cols = _binImg.cols - 1 ;  
	for (int i = 1; i < rows-1; i++)  
	{  
		int* data= _lableImg.ptr<int>(i) ;  
		for (int j = 1; j < cols-1; j++)  
		{  
			if (data[j] == 1)  
			{  
				std::stack<std::pair<int,int>> neighborPixels ;     
				neighborPixels.push(std::pair<int,int>(i,j)) ;     // pixel position: <i,j>  
				++label ;  // begin with a new label  
				while (!neighborPixels.empty())  
				{  
					// get the top pixel on the stack and label it with the same label  
					std::pair<int,int> curPixel = neighborPixels.top() ;  
					int curX = curPixel.first ;  
					int curY = curPixel.second ;  
					_lableImg.at<int>(curX, curY) = label ;  

					// pop the top pixel  
					neighborPixels.pop() ;  

					// push the 4-neighbors (foreground pixels)  
					if (_lableImg.at<int>(curX, curY-1) == 1)  
					{// left pixel  
						neighborPixels.push(std::pair<int,int>(curX, curY-1)) ;  
					}  
					if (_lableImg.at<int>(curX, curY+1) == 1)  
					{// right pixel  
						neighborPixels.push(std::pair<int,int>(curX, curY+1)) ;  
					}  
					if (_lableImg.at<int>(curX-1, curY) == 1)  
					{// up pixel  
						neighborPixels.push(std::pair<int,int>(curX-1, curY)) ;  
					}  
					if (_lableImg.at<int>(curX+1, curY) == 1)  
					{// down pixel  
						neighborPixels.push(std::pair<int,int>(curX+1, curY)) ;  
					}  
				}         
			}  
		}  
	}  
}


cv::Scalar icvprGetRandomColor()  
{  
	uchar r = 255 * (rand()/(1.0 + RAND_MAX));  
	uchar g = 255 * (rand()/(1.0 + RAND_MAX));  
	uchar b = 255 * (rand()/(1.0 + RAND_MAX));  
	return cv::Scalar(b,g,r) ;  
}  


void icvprLabelColor(const cv::Mat& _labelImg, cv::Mat& _colorLabelImg)   
{  
	if (_labelImg.empty() ||  
		_labelImg.type() != CV_32SC1)  
	{  
		return ;  
	}  

	std::map<int, cv::Scalar> colors ;  

	int rows = _labelImg.rows ;  
	int cols = _labelImg.cols ;  

	_colorLabelImg.release() ;  
	_colorLabelImg.create(rows, cols, CV_8UC3) ;  
	_colorLabelImg = cv::Scalar::all(0) ;  

	for (int i = 0; i < rows; i++)  
	{  
		const int* data_src = (int*)_labelImg.ptr<int>(i) ;  
		uchar* data_dst = _colorLabelImg.ptr<uchar>(i) ;  
		for (int j = 0; j < cols; j++)  
		{  
			int pixelValue = data_src[j] ;  
			if (pixelValue > 1)  
			{  
				if (colors.count(pixelValue) <= 0)  
				{  
					colors[pixelValue] = icvprGetRandomColor() ;  
				}  
				cv::Scalar color = colors[pixelValue] ;  
				*data_dst++   = color[0] ;  
				*data_dst++ = color[1] ;  
				*data_dst++ = color[2] ;  
			}  
			else  
			{  
				data_dst++ ;  
				data_dst++ ;  
				data_dst++ ;  
			}  
		}  
	}  
}
 
#endif

void SimulationProcess::runFLUS()
{
	startloop();
	runloop2();
}

SimulationProcess::~SimulationProcess()
{

}


void SimulationProcess::init()
{
	goalNum=NULL;
	saveCount=NULL;
	mIminDis2goal=NULL;
	temp=NULL;
	val=NULL;
	mdNeiborhoodProbability=NULL;
	mdRoulette=NULL;
	probability=NULL;
	sProbability=NULL;
	normalProbability=NULL;
	mdNeighIntensity=NULL;
	isSave=false;
}

///
vector<string> split_n(const string& src, string separate_character)  
{  
	vector<string> strs;  
	int separate_characterLen = separate_character.size();
	int lastPosition = 0, index = -1;  
	while (-1 != (index = src.find(separate_character, lastPosition)))  
	{  
		strs.push_back(src.substr(lastPosition, index - lastPosition));  
		lastPosition = index + separate_characterLen;  
	}  
	string lastString = src.substr(lastPosition);  
	if (!lastString.empty())  
		strs.push_back(lastString);  
	return strs;  
} 


int** ScanWindow(int sizeWindows)
{
	//defining a two-dimensions window
	int _numWindows=sizeWindows*sizeWindows-1;
	int i,k,f;
	int** direction=new int*[_numWindows];
	for(i=0;i<_numWindows;i++)
	{
		direction[i]=new int[2];
	}
	//loop
	i=0;
	for (k=-(sizeWindows-1)/2;k<=(sizeWindows-1)/2;k++)
	{
		for (f=-(sizeWindows-1)/2;f<=(sizeWindows-1)/2;f++)
		{
			if (!(k==0&&f==0))
			{
				direction[i][0]=k;
				direction[i][1]=f;
				i++;
			}
		}
	}
	return direction;
}



//
struct forArray
{
	int mb1;
	int mb2;
};

bool sortbymb1(const forArray &v1,const forArray &v2)
{
	return v1.mb1>v2.mb1;
};

void myPushback(vector<forArray> &vecTest,const int &m1,const int &m2)
{
	forArray test;
	test.mb1=m1;
	test.mb2=m2;
	vecTest.push_back(test);
}

#ifdef _RANK
void countSums(Mat src,vector<int>& _value)  
{  
	for (int i = 0; i<src.rows; ++i)
	{
		for (int j = 0; j<src.cols; ++j)
		{
			int val = src.at<int>(i, j);
			_value[val]++;
		}
	}
} 
#endif

bool SimulationProcess::getparameters()
{

	ifstream infile; 
	infile.open(configfile.c_str());
	assert(infile.is_open());

	string str;
	while(getline(infile,str))
	{
		if (str=="[Number of types]")
		{
			getline(infile,str);
			nType =atoi(str.c_str());
			
			mdRoulette=new double[nType+1];
			mdNeiborhoodProbability=new double[nType];
			probability=new double[nType];
			mIminDis2goal=new int[nType];
			saveCount=new int[nType];
			val=new int[nType];
			goalNum=new int[nType];
			mdNeighIntensity=new double[nType];

			mdRoulette[0]=0;
			for (int i=0;i<nType;i++)
			{
				mdNeiborhoodProbability[i]=0;
				val[i]=0;
				mdRoulette[i+1]=0;
				probability[i]=0;
			}

			t_filecost=new double*[nType];
			for(int i=0;i<nType;i++)
			{
				t_filecost[i]=new double[nType];
			}
		}

		if (str=="[Future Pixels]")
		{
			for (int ii=0;ii<nType;ii++)
			{
				getline(infile,str);
				vector<string> strList;
				strList = split_n(str,",");

				for (int jj=0;jj<strList.size();jj++)
				{
					goalNum[ii]=atoi(strList[jj].c_str());
				}
			}
		}
		if (str=="[Cost Matrix]")
		{
			for (int ii=0;ii<nType;ii++)
			{
				getline(infile,str);
				vector<string> strList;
				strList = split_n(str,",");

				for (int jj=0;jj<strList.size();jj++)
				{
					t_filecost[ii][jj]=atof(strList[jj].c_str());
				}
			}
		}

		if (str=="[Intensity of neighborhood]")
		{
			for (int ii=0;ii<nType;ii++)
			{
				getline(infile,str);
				vector<string> strList;
				strList = split_n(str,",");

				for (int jj=0;jj<strList.size();jj++)
				{
					mdNeighIntensity[ii]=atof(strList[jj].c_str());
				}
			}
		}

		if (str=="[Maximum Number Of Iterations]")
		{
			getline(infile,str);
			looptime=atoi(str.c_str());
		}
		if (str=="[Size of neighborhood]")
		{
			getline(infile,str);
			sizeWindows=atoi(str.c_str());

			if (sizeWindows!=1)
			{
				numWindows=sizeWindows*sizeWindows-1;
			}
			else
			{
				numWindows=1;
			}
			//---------------------------define a scanning window--------------------------- 
			//
			if (sizeWindows!=1)
			{
				direction=ScanWindow(sizeWindows);
			}
			//
		}
		if (str=="[Accelerated factor]")
		{
			getline(infile,str);
			degree=atof(str.c_str());
		}
	}
	infile.close();


	if (isMultGoal==true)
	{   
		ifstream infilem;
		infilem.open(demandfile.c_str());
		string str;
		getline(infilem,str);
		while(getline(infilem,str))
		{
			vector<string> strList;
			strList = split_n(str,",");
			multipleYear.push_back(atoi(strList[0].c_str()));
			for (int ii=0;ii<nType;ii++)
			{
				multiDemand.push_back(atoi(strList[ii+1].c_str()));
			}
		}
		infilem.close();
	}



	cout<<"Cols: "<<_cols<<endl;
	cout<<"Rows: "<<_rows<<endl;

	cout<<"Maximum Number Of Iterations: "<<looptime<<endl;

	cout<<"Neighborhood influence: "<<sizeWindows<<endl;

	cout<<"Acceleration for iterate: "<<degree<<endl;

	cout<<"Cost Matrix"<<endl;

	for (int i=0;i<nType;i++)
	{
		string line;
		for (int j=0;j<nType;j++)
		{
			line=line+num2string(t_filecost[i][j])+" ";
		}
		cout<<line<<endl;
	}

	cout<<"Future year"<<endl;

	if (isMultGoal==true)
	{
		string line="Initial year--";
		for (int j=0;j<multipleYear.size();j++)
		{
			line=line+num2string(multipleYear[j])+"--";
		}
		cout<<line.substr(0,line.length()-2)<<endl;
	}


	return true;
}

bool SimulationProcess::getcellstatistic()
{
	for (int kk=0;kk<nType;kk++)
	{
		typeIndex.push_back(kk+1);
		saveCount[kk]=0;
	} 

	for (size_t ii=0;ii<_rows*_cols;ii++)
	{
		unsigned char temp=*(unsigned char*)(imgList[0]->imgData()+ii*sizeof(unsigned char));

		for (int kk=0;kk<nType;kk++)
		{
			if (temp==typeIndex[kk])
			{
				saveCount[kk]++;
			}
		} 
	}
	return true;

}

void SimulationProcess::saveResult(string filename)
{
	if (isMultGoal==false)
	{
		imgList[1]->close();
		if (imgList.size()==3)
		{
			imgList[2]->close();
		}
	}

	size_t i,j,k;
	TiffDataWrite pwrite;

	bool brlt ;

	brlt = pwrite.init(filename.c_str(), imgList[0]->rows(), imgList[0]->cols(), 1, \
		imgList[0]->geotransform(), imgList[0]->projectionRef(), GDT_Byte, 0);


	if (!brlt)
	{
		cout<<"write init error!"<<endl;
		return ;
	}


	unsigned char _val = 0;

	if (imgList[0]->imgData()!=NULL)
	{
		for (i=0; i<pwrite.rows(); i++)
		{
			for (j=0; j<pwrite.cols(); j++)
			{
				for (k=0; k<pwrite.bandnum(); k++)
				{
					_val = imgList[0]->imgData()[k*pwrite.rows()*pwrite.cols()+i*pwrite.cols()+j];
					pwrite.write(i, j, k, &_val);
				}
			}
		}
		//cout<<"write success!"<<endl;
		pwrite.close();

	}

	if (isMultGoal==false)
	{
		imgList[0]->close();
	}

	isSave=true;
}

#ifdef _RANK
void SimulationProcess::savePatchGeoImage(string filename,Mat labelImg)
{
	TiffDataWrite pwrite;
	bool brlt = pwrite.init(filename.c_str(), _rows, _cols, 1, \
		imgList.at(0)->geotransform(), imgList.at(0)->projectionRef(), GDT_UInt32, 0);
	if (!brlt)
	{
		cout<<"write init error!"<<endl;
		return ;
	}

	unsigned int* geoPatcgImg=new unsigned int[pwrite.rows()*pwrite.cols()];

	for (size_t i=0; i<pwrite.rows(); i++)
	{
		for (size_t j=0; j<pwrite.cols(); j++)
		{
			for (size_t k=0; k<pwrite.bandnum(); k++) 
			{  
				int _val=labelImg.at<int>(i,j);  
				pwrite.write(i, j, k, &_val);
			}	 
		}
	}


	cout<<"write success!"<<endl;
	pwrite.close();
	delete[] geoPatcgImg;
	geoPatcgImg=NULL;
}
#endif

bool SimulationProcess::readImageData()
{
	bool bRlt=false;
	ifstream file(configfile.c_str());

	string str;
	while(getline(file,str))
	{
		if (str=="[Path of land use data]")
		{
			getline(file,str);
			bRlt=imageOpenConver2uchar(str.c_str());
		}
		if (str=="[Path of probability data]")
		{
			getline(file,str);
			bRlt=imageOpen(str.c_str());
		}
		if (str == "[Path of simulation result]")
		{
			getline(file,str);
			savepath=str;
		}
		if (str == "[Path of restricted area]")
		{
			getline(file,str);
			if (str == "No restrict data")
			{
				isRestrictExit=false;
			}
			else
			{
				isRestrictExit=true;
				bRlt=imageOpenConver2uchar(str.c_str());
			}
		}

	}
	file.close();

	if (!bRlt)
	{
		cout<<"Read image error"<<endl;
	}

	return bRlt;
}

bool SimulationProcess::imageOpen(string filename)
{
	//register
	GDALAllRegister();
	//OGRRegisterAll();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");
	
	TiffDataRead* pread = new TiffDataRead;

	imgList.push_back(pread);


	
	if (!imgList.at(imgList.size()-1)->loadFrom(filename.c_str()))
	{
		cout<<"load error!"<<endl;
		return false;
	}
	else
	{
		cout<<"load success!"<<endl;
	}

	return true;
}

bool SimulationProcess::imageOpenConver2uchar(string filename)
{
	//register
	GDALAllRegister();
	//OGRRegisterAll();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");
	
	TiffDataRead* pread = new TiffDataRead;

	imgList.push_back(pread);

	
	if (!imgList.at(imgList.size()-1)->loadFrom(filename.c_str()))
	{
		cout<<"load error!"<<endl;
		return false;
	}
	else
	{
		imgList.at(imgList.size()-1)->convert2uchar();
		cout<<"convert success!"<<endl;
	}

	return true;
}

void SimulationProcess::startloop()
{
	seedRandomOnce();
	
	bool _isStatus=getparameters();

	if (_isStatus!=true)
	{
		cout<<"Get Parameter Error";
		return;
	}

	getcellstatistic();

	string _label("\n-----------------Start to iterate-----------------\n");
	cout<<_label<<endl;

	string sendInfo("Number of pixels of each land use before iteration, ");
	for (int ii=0;ii<nType;ii++)
	{
		sendInfo = sendInfo + num2string(saveCount[ii])+  ","; 
	}
	cout<<sendInfo.substr(0,sendInfo.length()-1)<<endl;
}

void SimulationProcess::runloop2()
{
	double start,end;
	double _max=0;
	int k=0;
	bool isRestrictPix;
	double* initialDist;
	double* dynaDist;
	double* adjustment;
	double* adjustment_effect;
	double* initialProb;
	int* opposite2reverse;
	double* bestdis;

	vector<double> inherant;
	long piexelsum=0;
	int numofYear=0;
	int historyDis=0;
	int sumDis=0;
	int stasticHistroyDis=0;
	bool isSwitchIniYear=false;

	seedRandomOnce();
	start=GetTickCount();  
	adjustment=new double[nType];
	initialDist=new double[nType];
	dynaDist=new double[nType];
	adjustment_effect=new double[nType];
	initialProb=new double[nType];
	opposite2reverse=new int[nType];
	bestdis=new double[nType];

	for (int ii=0;ii<nType;ii++)
	{
		adjustment_effect[ii]=1;
		piexelsum+=saveCount[ii];
		opposite2reverse[ii]=0;
	}

	// 	QFile file("Inheritance.csv");
	// 	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
	// 	{
	// 		cout<<"Create 'Inheritance.csv' Error";
	// 		return;
	// 	}

	//	QTextStream out(&file);

	if (isMultGoal==true)
	{
		for (int ii=0;ii<nType;ii++)
		{
			goalNum[ii]=multiDemand[numofYear*nType+ii];
		}
	}


	for(;;)
	{

		for (int ii=0;ii<nType;ii++)
		{
			mIminDis2goal[ii]=goalNum[ii]-saveCount[ii];

			if (k==0||isSwitchIniYear==true)
			{
				initialDist[ii]=mIminDis2goal[ii];
				dynaDist[ii]=initialDist[ii]*1.01;
				bestdis[ii]=initialDist[ii];
			}

			if (abs(bestdis[ii])>abs(mIminDis2goal[ii]))
			{
				bestdis[ii]=mIminDis2goal[ii];
			}
			else
			{
				if ((abs(mIminDis2goal[ii])-abs(bestdis[ii]))/abs(initialDist[ii])>0.05)
				{
					opposite2reverse[ii]=1;
				}
			}


			adjustment[ii]=mIminDis2goal[ii]/dynaDist[ii];


			if (adjustment[ii]<1&&adjustment[ii]>0)
			{
				dynaDist[ii]=mIminDis2goal[ii];

				if (initialDist[ii]>0&&adjustment[ii]>(1-degree))
				{
					adjustment_effect[ii]=adjustment_effect[ii]*(adjustment[ii]+degree);
				}

				if (initialDist[ii]<0&&adjustment[ii]>(1-degree))
				{
					adjustment_effect[ii]=adjustment_effect[ii]*(1/(adjustment[ii]+degree));
				}

			}

			if ((initialDist[ii]>0&&adjustment[ii]>1))
			{
				adjustment_effect[ii]=adjustment_effect[ii]*adjustment[ii]*adjustment[ii];
			}

			if ((initialDist[ii]<0&&adjustment[ii]>1))
			{
				adjustment_effect[ii]=adjustment_effect[ii]*(1.0/adjustment[ii])*(1.0/adjustment[ii]);
			}

		}



		string sendInher;
		for (int ii=0;ii<nType;ii++)
		{
			sendInher = sendInher + num2string(adjustment_effect[ii])+  ",";
		}
			
		//cout<<sendInher.substr(0,sendInher.length() - 1)<<endl;

#ifdef _RANK
		//==========================================gdal2mat============================================
		cv::Mat tmpMat = cv::Mat(_rows,_cols,CV_8UC1,imgList[0]->imgData());
		cv::Mat binImage = tmpMat-1;
		cv::Mat labelImg ;
		//double timestart=GetTickCount();
		//icvprCcaByTwoPass(binImage, labelImg) ;  
		icvprCcaBySeedFill(binImage, labelImg) ;
		//double timeend=GetTickCount();
		//cout<<timeend-timestart<<endl;
		
		double max, min;  
		cv::Point min_loc, max_loc;  
		cv::minMaxLoc(labelImg, &min, &max, &min_loc, &max_loc); 
		//cout<<"min: "<<min<<endl;
		//cout<<"max: "<<max<<endl;

		double fScale = 1; 
		// show result  
// 		cv::Mat grayImg ,grayImg2;  
// 		labelImg *= 10 ;
// 		labelImg.convertTo(grayImg, CV_8UC1) ;  
// 		resize(grayImg,grayImg2,Size(grayImg.cols*fScale,grayImg.rows*fScale),0,0,CV_INTER_LINEAR); 
// 		cv::imshow("labelImg", grayImg2) ;
		// show result  
		cv::Mat colorLabelImg, colorLabelImg2;  
		icvprLabelColor(labelImg, colorLabelImg) ;
		resize(colorLabelImg,colorLabelImg2,Size(colorLabelImg.cols*fScale,colorLabelImg.rows*fScale),0,0,CV_INTER_LINEAR); 
		//cv::imshow("colorImg", colorLabelImg2) ;  
		//cv::waitKey(1000) ;  
		imwrite("patches.jpg",colorLabelImg2);
		savePatchGeoImage("geopatch.tif",labelImg);

		/*int hahaType = labelImg.type();
		std::cout<<"Type = "<<hahaType<<std::endl;*/
		//vector<forArray> listValue;
		vector<int> vlistValue;
		for (int ii=0;ii<=max;ii++)
		{
			//myPushback(listValue,0,ii);
			vlistValue.push_back(0); 
		}
		countSums(labelImg,vlistValue) ;
		//cout<<accumulate(vlistValue.begin(),vlistValue.end(),0)<<endl;
		//sort(vlistValue.begin(),vlistValue.end(),sortbymb1);
		//vector<int>::iterator itr = vlistValue.begin();
		//vlistValue.erase(itr); 
		vlistValue[0]=0;

		 std::vector<int>::iterator max1 = std::max_element(std::begin(vlistValue), std::end(vlistValue));  

		 double maxvalue=*max1;

		
		 cout<<"size of biggest patches: "<<maxvalue<<endl;
#endif

		size_t i;
		size_t j;
		for (i=0;i<_rows;i++)
		{
			for (j=0;j<_cols;j++)
			{
				
				if ((imgList[0]->imgData()[i*_cols+j]<typeIndex[0]||imgList[0]->imgData()[i*_cols+j]>typeIndex[nType-1]))
				{
					temp[i*_cols+j]=imgList[0]->imgData()[i*_cols+j];
				}
				else
				{
					if (isRestrictExit==true)
					{
						if (imgList[2]->imgData()[i*_cols+j]==0)
						{
							isRestrictPix=true;
						}
						else
						{
							isRestrictPix=false;
						}
					}
					else
					{
						isRestrictPix=false; 
					}

					if (isRestrictPix==false)
					{
						double sumpbPatch=0;
						
						if (sizeWindows!=1)
						{

							for (int ii=0;ii<nType;ii++)
							{
								val[ii]=0;
							}


							vector<int> pixelPatch;

							for (int m =0; m<numWindows; m++)
							{
								int _x = i+direction[m][0];
								int _y = j+direction[m][1];
								if (_x<0 || _y<0 || _x>=_rows || _y>=_cols)
									continue;
								for (int _ii=1;_ii<=nType;_ii++)
								{
									if (imgList[0]->imgData()[_x*_cols+_y] == _ii)
									{
										val[_ii-1]+=1;
									}
								}
#ifdef _RANK
								
								int val = labelImg.at<int>(_x, _y);
								if (val>0)
								{
									
									pixelPatch.push_back(val);
								}
#endif
							}
#ifdef _RANK
							
							sort(pixelPatch.begin(),pixelPatch.end());
							pixelPatch.erase(unique(pixelPatch.begin(), pixelPatch.end()), pixelPatch.end());
							

							//if (pixelPatch.size()>5)
							//{
							//	cout<<endl;
							//}

							for (int ii=0;ii<pixelPatch.size();ii++)
							{
 								double pbPatch;
 
 								pbPatch=(double)vlistValue[pixelPatch[ii]];
 
 								pbPatch=pbPatch/maxvalue;
 
 								//sumpbPatch+=pbPatch;

								if (pbPatch>=sumpbPatch)
								{
									sumpbPatch=pbPatch;
								}

							}
#endif

						}
						else
						{
							for (int _ii=1;_ii<=nType;_ii++)
							{
								val[_ii-1]=1; 
							}
							numWindows=1; 
						}

						
						int oldType=imgList[0]->imgData()[i*_cols+j]-1;

						double Inheritance =0;



						
						switch(imgList[1]->datatype())
						{
						case GDT_Float32:

							for (int _ii=0;_ii<nType;_ii++)
							{

								mdNeiborhoodProbability[_ii]= val[_ii]/(double)numWindows;

								double dSuitability;

								dSuitability=*(float*)(imgList[1]->imgData()+(_cols*_rows*(_ii)+i*_cols+j)*sizeof(float));

								double _neigheffect=mdNeighIntensity[_ii]+0.000000001;

								mdNeiborhoodProbability[_ii]=mdNeiborhoodProbability[_ii]*_neigheffect; 

								probability[_ii]=dSuitability*mdNeiborhoodProbability[_ii];

								initialProb[_ii]=dSuitability;

								if (oldType==_ii)
								{
									Inheritance=10*nType;
									probability[_ii]=probability[_ii]*(adjustment_effect[_ii])*Inheritance;
								}


							}

							break;

						case GDT_Float64: 

							for (int _ii=0;_ii<nType;_ii++)
							{
								mdNeiborhoodProbability[_ii]= val[_ii]/(double)numWindows;

								double dSuitability;

								dSuitability=*(double*)(imgList[1]->imgData()+(_cols*_rows*(_ii)+i*_cols+j)*sizeof(double));

								double _neigheffect=mdNeighIntensity[_ii]+0.00000001; 

								mdNeiborhoodProbability[_ii]=mdNeiborhoodProbability[_ii]*_neigheffect; 

								probability[_ii]=dSuitability*mdNeiborhoodProbability[_ii];

								initialProb[_ii]=dSuitability;

								if (oldType==_ii)
								{
									Inheritance=10*nType;
									probability[_ii]=probability[_ii]*(adjustment_effect[_ii])*Inheritance;
								}

							}

							break;

						default:
							return ;
						}
#ifdef _RANK
						probability[1]*=sumpbPatch+0.000000001;
#endif
						
						for (int jj=0;jj<nType;jj++)
						{
							probability[jj]=probability[jj]*t_filecost[oldType][jj];
						}

						
						double SumProbability=0;
						for (int ii=0;ii<nType;ii++)
						{
							SumProbability+=probability[ii];		
						}

#ifndef DAREA

						for (int ii=0;ii<nType;ii++)
						{
							mdNeiborhoodProbability[ii]=probability[ii]/SumProbability;		
						}
#endif




#ifdef DAREA
						
						int codeurban=2; 
						double origleftvalue=0;
						for (int _ii=0;_ii<nType;_ii++)
						{
							if (SumProbability!=0)
							{
								double ___tmp= probability[_ii]/SumProbability;
								mdNeiborhoodProbability[_ii]=___tmp;
							}
							else
							{
								mdNeiborhoodProbability[_ii]=0;
							}

							if (_ii!=codeurban-1)
							{
								origleftvalue+=mdNeiborhoodProbability[_ii];
							}
						}

						if (imgList[2]->imgData()[i*_cols+j]==2)
						{
							double rdmData=(double)rand()/(double)RAND_MAX;

							double rdmData2=(double)rand()/(double)RAND_MAX;

							if (*(float*)(imgList[1]->imgData()+(_cols*_rows*(1)+i*_cols+j)*sizeof(float))>rdmData2)
							{
								mdNeiborhoodProbability[codeurban-1]=mdNeiborhoodProbability[codeurban-1]+rdmData;
								if (mdNeiborhoodProbability[codeurban-1]>=1)
								{
									for (int _ii=0;_ii<nType;_ii++)
									{
										mdNeiborhoodProbability[_ii]=0;
									}
									mdNeiborhoodProbability[codeurban-1]=1;
								}
								else
								{
									double leftvalue=1-mdNeiborhoodProbability[codeurban-1];
									for (int _ii=0;_ii<nType;_ii++)
									{
										if (_ii!=codeurban-1)
										{
											mdNeiborhoodProbability[_ii]=mdNeiborhoodProbability[_ii]/origleftvalue*leftvalue;
										}
									}
								}
							}
							else
							{
								// nothing to do, nothing to say
							}
						}

#endif



						
						mdRoulette[0]=0;
						for (int ii=0;ii<nType;ii++)
						{
							mdRoulette[ii+1]=mdRoulette[ii]+mdNeiborhoodProbability[ii];
						}


						
						
						bool isConvert;

						double rdmData=(double)rand()/(double)RAND_MAX;


						
						for (int _kk=0;_kk<nType;_kk++)
						{

							int newType=_kk;

							if (rdmData<=mdRoulette[newType+1]&&rdmData>mdRoulette[newType]) 
							{
								double rdmData=(double)rand()/(double)RAND_MAX;
								double rdmData2=(double)rand()/(double)RAND_MAX;
								double rdmData3=(double)rand()/(double)RAND_MAX;

								if((oldType!=newType)&&(t_filecost[oldType][newType]!=0))
								{
									isConvert=true;
								}
								else
								{
									isConvert=false;
								}


								
								double _disChangeFrom;
								_disChangeFrom=mIminDis2goal[oldType];

								double _disChangeTo;
								_disChangeTo=mIminDis2goal[newType];

								if (initialDist[newType]>=0&&_disChangeTo==0) 
								{
									adjustment_effect[newType]=1;
									isConvert=false;
								}

								if (initialDist[oldType]<=0&&_disChangeFrom==0) 
								{
									adjustment_effect[oldType]=1;
									isConvert=false;
								}


								if (initialDist[oldType]>=0&&opposite2reverse[oldType]==1) 
								{
									isConvert=false;
								}
								if (initialDist[newType]<=0&&opposite2reverse[newType]==1)
								{
									isConvert=false;
								}


								if (isConvert==true) 
								{
									if ((rdmData3+(1.0/nType))/((k+1))<initialProb[newType])
									{
										isConvert=true;
									}
									else
									{
										isConvert=false;
									}
								}

								if (isConvert==true)
								{
									temp[i*_cols+j]=(unsigned char)(newType+1);
									saveCount[newType]+=1;
									saveCount[oldType]-=1;
									mIminDis2goal[newType]=goalNum[newType]-saveCount[newType];
									mIminDis2goal[oldType]=goalNum[oldType]-saveCount[oldType];
									break; 
								}
								else
								{
									temp[i*_cols+j]=imgList[0]->imgData()[i*_cols+j];
									break; 
								}

								opposite2reverse[oldType]=0;
								opposite2reverse[newType]=0;

							}
							else
							{
								temp[i*_cols+j]=imgList[0]->imgData()[i*_cols+j];
							}

						}
					}
					else
					{
						temp[i*_cols+j]=imgList[0]->imgData()[i*_cols+j];
					}
				}
			}
		}

		for (int ii=0;ii<nType;ii++)
		{
			saveCount[ii]=0;
		}

		for (int jj=0;jj<_rows*_cols;jj++)
		{
			imgList[0]->imgData()[jj]=temp[jj];
			for (int kk=0;kk<nType;kk++)
			{
				if (temp[jj]==typeIndex[kk])
				{
					saveCount[kk]+=1;
					break;
				}
			}
		}

#ifdef _RANK
		tmpMat.release();
		binImage.release();
		colorLabelImg.release();
		labelImg.release();
		vlistValue.clear();
#endif

		string sendInfo("Number of pixels of each land use at ");
		sendInfo=sendInfo+num2string(k+1)+" iteration, ";
		for (int ii=0;ii<nType;ii++)
		{
			sendInfo = sendInfo + num2string(saveCount[ii])+  ",";
		}

		cout<<sendInfo.substr(0,sendInfo.length()-1)<<endl;


		k++;

		
		if (sumDis==historyDis)
		{
			stasticHistroyDis++;
		}

		if (isMultGoal==false)
		{
			sumDis=0;

			for (int ii=0;ii<nType;ii++)
			{
				sumDis=sumDis+abs(mIminDis2goal[ii]);
			}

			if (sumDis==0||(stasticHistroyDis>5)&&(sumDis<(piexelsum*0.0001)))
			{
				saveResult(savepath);

				isbreak=1;
			}
		}

		if (isMultGoal==true&&numofYear<multipleYear.size())
		{
			sumDis=0;

			for (int ii=0;ii<nType;ii++)
			{
				sumDis=sumDis+abs(mIminDis2goal[ii]);
			}

			if (sumDis==0||(stasticHistroyDis>5)&&(sumDis<(piexelsum*0.0001)))
			{

				stasticHistroyDis=0;

				isSwitchIniYear=true;

				string _year;

				string savepathyear;

				string appendage=".tif";

				_year= num2string(multipleYear[numofYear]);

				savepathyear=savepath.substr(0,savepath.length()-4)+"_"+_year+appendage;

				saveResult(savepathyear);

				cout<<"\nSave image at: "+savepathyear+"\n"<<endl;

				numofYear++;

				if (numofYear>=multipleYear.size())
				{
					isbreak=1;
				}
				else
				{
					for (int ii=0;ii<nType;ii++)
					{
						goalNum[ii]=multiDemand[numofYear*nType+ii];
					}
				}
			}
			else
			{
				isSwitchIniYear=false;
			}

		}

		historyDis=sumDis;

		if (isbreak==1||isbreak==2||k>=looptime-1)
		{
			break;
		}


	}

	if (isbreak==2||isSave==false)
	{
		saveResult(savepath);
	}

	end=GetTickCount();   

	double timecost=end-start;  

	string sendtime("Time used: ");
	cout<<sendtime+num2string(timecost/1000)+" s"<<endl;

	if (isMultGoal==true)
	{
		imgList[0]->close();
		imgList[1]->close();
		if (imgList.size()==3)
		{
			imgList[2]->close();
		}
	}

	
	delete[] initialProb;
	delete[] adjustment;
	delete[] dynaDist;
	delete[] adjustment_effect;
	delete[] initialDist;
	delete[] saveCount;
	delete[] probability;
	delete[] mIminDis2goal;
	delete[] mdNeiborhoodProbability;
	delete[] mdRoulette;
	delete[] goalNum;
	delete[] val;
	delete[] bestdis;
	delete[] opposite2reverse;

	
	if (sizeWindows!=1)
	{
		for(int i=0;i<numWindows;i++)
		{delete []direction[i];}
		delete []direction;
	}

	for(int i=0;i<nType;i++)
	{delete []t_filecost[i];}
	delete []t_filecost;

}
