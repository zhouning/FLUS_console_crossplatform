#include "nntrain.h"
#include "TiffDataRead.h"
#include "TiffDataWrite.h"
#include <iostream>
#include <statistics.h>
#include <dataanalysis.h>
#include <alglibmisc.h>
#include <linalg.h>
#include <time.h>
#include "platform_compat.h"
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <ctime>

using namespace std;
using namespace alglib;

namespace {

unsigned int resolveNNRandomSeed()
{
	const char* envSeed = getenv("FLUS_RANDOM_SEED");
	if (envSeed != NULL && envSeed[0] != '\0')
	{
		char* end = NULL;
		errno = 0;
		unsigned long parsed = strtoul(envSeed, &end, 10);
		if (errno == 0 && end != envSeed && *end == '\0' && parsed <= UINT_MAX)
		{
			cout<<"FLUS NN random seed: "<<parsed<<" (FLUS_RANDOM_SEED)"<<endl;
			return static_cast<unsigned int>(parsed);
		}
		cout<<"Invalid FLUS_RANDOM_SEED '"<<envSeed<<"'; falling back to time seed"<<endl;
	}
	unsigned int seed = static_cast<unsigned int>(time(NULL));
	cout<<"FLUS NN random seed: "<<seed<<" (time)"<<endl;
	return seed;
}

}

vector<string> split(const string& src, string separate_character)  
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

NNtrain::NNtrain(string _configimage)
{
	configimage=_configimage;
	isUpdateData=false;
	initializationSucceeded=false;
	init();
}

NNtrain::NNtrain(string _configimage,string _updatefile)
{
	configimage=_configimage;
	updatefile=_updatefile;
	isUpdateData=false;
	initializationSucceeded=false;
	init();
	if (!initializationSucceeded)
	{
		return;
	}
	isUpdateData=true;

	cout<<"\nload update data\n"<<endl;

	bool bRlt;
	if (isUpdateData==true)
	{
			ifstream fileu(updatefile.c_str());
			if (!fileu)
			{
				cout<<"read update file error!!!"<<endl;
				initializationSucceeded=false;
				return;
			}

		string str;
			while (getline(fileu,str))
			{
				vector<string> strlist=split(str, ",");
				if (strlist.size() < 2)
				{
					cout<<"invalid update row"<<endl;
					initializationSucceeded=false;
					return;
				}
			if (strlist[1]!="Add Factors(Optional)" ) 
			{
				redata1 reda;
				reda.num=atoi(strlist[0].c_str());
				reda.str=strlist[1];
				qlredat.push_back(reda);
			}
		}
		for (int ii=0;ii<qlredat.size();ii++)
		{
				string tmp=qlredat.at(ii).str;
				bRlt=imageOpen(tmp.c_str());
				if (!bRlt)
				{
					initializationSucceeded=false;
					return;
				}
		}

	}

	cout<<endl;
}

void NNtrain::init()
{
	initializationSucceeded=false;
	isNoDataExit=true;

	if (!getAllParameter() || imgList.empty() || divingList.empty())
	{
		cout<<"FLUS training initialization failed"<<endl;
		return;
	}

	nWidth=imgList[0]->cols();
	nHeight=imgList[0]->rows();
	nData=pathofdivingfactor.size();

	f_allBandData=NULL;
	d_allBandData=NULL;
	us_allBandData=NULL;
	m_PointCoodinateX=NULL;
	m_PointCoodinateY=NULL;
	saveMemF=NULL;
	saveMemD=NULL;
	saveMemUs=NULL;
	f_allBandData_update=NULL;
	d_allBandData_update=NULL;
	us_allBandData_update=NULL;
	initializationSucceeded=true;
}

bool NNtrain::ready() const
{
	return initializationSucceeded;
}

template<typename T>
string num2str(T i)
{
	stringstream ss;
	ss << i;
    return ss.str();
}


bool NNtrain::getcellstatistic()
{

	size_t miHeight=imgList[0]->rows();

	size_t miWidth=imgList[0]->cols();

	double minmax1[2];

	imgList[0]->poDataset()->GetRasterBand(1)->ComputeRasterMinMax(false,minmax1);

	if (minmax1[0]!=1)
	{
		cout<<"The min code of land use type must be 1, for example 1,2,3..."<<endl;
		return false;
	}


	for (int kk=0;kk<minmax1[1];kk++)
	{
		mvLanduseType.push_back(kk+1);
		mvCountType.push_back(0);
	} 

	for (size_t ii=0;ii<miHeight*miWidth;ii++)
	{
		unsigned char temp=*(unsigned char*)(imgList[0]->imgData()+ii*sizeof(unsigned char));

		for (int kk=0;kk<mvLanduseType.size();kk++)
		{
			if (temp==mvLanduseType[kk])
			{
				mvCountType[kk]++;
			}
		} 
	}
	return true;

}



bool NNtrain::getAllParameter()
{
	ifstream filem(configimage.c_str());
	if (!filem)
	{
		cout<<"read config file error!!!"<<endl;
		return false;
	}
	int numofdf;
	int countRight=0;
	bool bRlt;
	string str;
	while (getline(filem,str)) 
	{
// 		if (str=="[NoData Value]" )
// 		{
// 			getline(filem,str);
// 			if (str=="No NoData Value" )
// 			{
// 				isNoDataExit=false;
// 			}
// 			else
// 			{
// 				isNoDataExit=true;
// 				noDataValue=atof(str.c_str());
// 			}
// 			countRight++;
// 		}
		
		if (str=="[Path of land use data]" )
		{
			getline(filem,str);
			bRlt=imageOpenConver2uchar(str.c_str());
			if (!bRlt)
			{
				return false;
			}
			countRight++;
			if (!getcellstatistic() || imgList.empty())
			{
				return false;
			}
			noDataValue=imgList[0]->poDataset()->GetRasterBand(1)->GetNoDataValue();
		}
		if (str=="[Path of saving data]" )
		{
			getline(filem,str);
			pathofsimresult=str;
			countRight++;
		}
		if (str=="[Number of driving data]" )
		{
			getline(filem,str);
			numofdf=atoi(str.c_str());
			countRight++;
		}
		if (str=="[Path of driving data]" )
		{
			for (int ii=0;ii<numofdf;ii++)
			{
				getline(filem,str);
					pathofdivingfactor.push_back(str);
					bRlt=imageOpen(pathofdivingfactor[pathofdivingfactor.size()-1].c_str());
					if (!bRlt)
					{
						return false;
					}
			}
			countRight++;
		}
		if (str=="[Data type]" )
		{
			getline(filem,str);
			datatype=str;
			countRight++;
		}
		if (str=="[Normalization type]" )
		{
			getline(filem,str);
			if (str=="Normalization" )
			{
				isNomalized=true;
			}
			else
			{
				isNomalized=false;
			}
			countRight++;
		}
		if (str=="[Sample type]" )
		{
			getline(filem,str);
			if (str=="Uniform Sampling" )
			{
				isUnifomSam=true;
			}
			else
			{
				isUnifomSam=false;
			}
			countRight++;
		}
		if (str=="[Percentage of Random Points]" )
		{
			getline(filem,str);
			samplingRate=atof(str.c_str());
			countRight++;
		}
		if (str=="[Hidden layer]" )
		{
			getline(filem,str);
			numHiddenLayer=atoi(str.c_str());
			countRight++;
		}
	}
	filem.close();

	if (countRight==9)
	{
		return true;
	}
	else
	{
		return false;
	}

}



bool NNtrain::imageOpen(string filename)
{
	//register
	GDALAllRegister();
	//OGRRegisterAll();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");
	
	TiffDataRead* pread = new TiffDataRead;

	if (isUpdateData==true)
	{
		updatedivingList.push_back(pread);
		string stdfilenamestr=filename; 
		
		if (!updatedivingList.at(updatedivingList.size()-1)->loadFrom(stdfilenamestr.c_str()))
		{
			cout<<"load error!"<<endl;
			return false;
		}
		else
		{
			cout<<"load success!"<<endl;
		}
	}
	else
	{
		divingList.push_back(pread);
		string stdfilenamestr=filename;
		
		if (!divingList.at(divingList.size()-1)->loadFrom(stdfilenamestr.c_str()))
		{
			cout<<"load error!"<<endl;
			return false;
		}
		else
		{
			cout<<"load success!"<<endl;
		}
	}

	return true;
}

bool NNtrain::imageOpenConver2uchar(string filename)
{
	//register
	GDALAllRegister();
	//OGRRegisterAll();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");
	
	TiffDataRead* pread = new TiffDataRead;

	imgList.push_back(pread);

	string stdfilenamestr=filename;

	if (!imgList.at(imgList.size()-1)->loadFrom(stdfilenamestr.c_str()))
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


string putCount2File(double* saveCount,int _length)
{
	string s1;
	string inval(",");
	stringstream out1;
	for (int i=0;i<_length;i++)
	{
		out1<<setiosflags(ios::fixed)<<setprecision(8)<<saveCount[i]<<inval;
		s1=out1.str();
	}	
	out1.clear();	
	return s1;
}

NNtrain::~NNtrain()
{

}


void NNtrain::trainprocess()
{

	bandCount=0;

	for (int ii=0;ii<nData;ii++)
	{
		bandCount+=divingList.at(ii)->bandnum();

		if (divingList[ii]->bandnum()==1)
		{
			int pos = pathofdivingfactor[ii].find_last_of('\\');
			string namestring(pathofdivingfactor[ii].substr(pos + 1));
			namestring=namestring.substr(0,namestring.length()-4);
			bandName.push_back(namestring);
		}
		else
		{

			int pos = pathofdivingfactor[ii].find_last_of('\\');
			string namestring(pathofdivingfactor[ii].substr(pos + 1));
			namestring=namestring.substr(0,namestring.length()-4);

			for (int kk=0;kk<divingList[ii]->bandnum();kk++)
			{  
				string str1=namestring+"_band"+num2str(kk+1);
				bandName.push_back(str1);
			}
		}
	}


	if (bandCount>0)
	{
		if (datatype=="Float" )
		{
			f_allBandData=new float[nWidth*nHeight*bandCount];
		}
		if (datatype=="Double" )
		{
			d_allBandData=new double[nWidth*nHeight*bandCount];
		}
		if (datatype=="Unsigned short" )
		{
			us_allBandData=new unsigned short[nWidth*nHeight*bandCount];
		}
			
		currentPos=0;
		//get data

		double* minmax1=new double[2];

		bool bRlt = false;
		for (int ii=0;ii<divingList.size();ii++)
		{
			divingList.at(ii)->loadData();
			currentBand=divingList.at(ii)->bandnum();

			for (int kk=0;kk<currentBand;kk++)
			{
				divingList.at(ii)->poDataset()->GetRasterBand(1+kk)->ComputeRasterMinMax(1,minmax1);

				float nodatavalue_factor=divingList[ii]->poDataset()->GetRasterBand(1)->GetNoDataValue();
				double nodatavalue_factord=divingList[ii]->poDataset()->GetRasterBand(1)->GetNoDataValue();
				if (minmax1[0]==nodatavalue_factor||minmax1[0]==nodatavalue_factord)
				{
					minmax1[0]=0;
				}
				
				minmaxsave.push_back(minmax1[0]);
				minmaxsave.push_back(minmax1[1]);
				minmaxsave.push_back(divingList.at(ii)->poDataset()->GetRasterBand(1+kk)->GetNoDataValue());
			}

			switch(divingList.at(ii)->datatype())
			{
				case GDT_Byte:
					bRlt = dataCopyConvert<unsigned char>(divingList.at(ii)->imgData());
					break;
				case GDT_UInt16:
					bRlt = dataCopyConvert<unsigned short>(divingList.at(ii)->imgData());
					break;
				case GDT_Int16:
					bRlt = dataCopyConvert<short>(divingList.at(ii)->imgData());
					break;
				case GDT_UInt32:
					bRlt = dataCopyConvert<unsigned int>(divingList.at(ii)->imgData());
					break;
				case GDT_Int32:
					bRlt = dataCopyConvert<int>(divingList.at(ii)->imgData());
					break;
				case GDT_Float32:
					bRlt = dataCopyConvert<float>(divingList.at(ii)->imgData());
					break;
				case GDT_Float64:
					bRlt = dataCopyConvert<double>(divingList.at(ii)->imgData());
					break;
				default:
					cout<<"CGDALRead::loadFrom : unknown data type!"<<endl;

			}

			currentPos+=nWidth*nHeight*currentBand;

			divingList.at(ii)->close();
		}

		delete[] minmax1;


		if (f_allBandData!=NULL||d_allBandData!=NULL||us_allBandData!=NULL)
		{
			nnTrain();
			if (datatype=="Float" )
			{
				if (f_allBandData!=NULL)
				{
					delete[] f_allBandData;
					f_allBandData=NULL;
				}
			}
			if (datatype=="Double" )
			{
				if (d_allBandData!=NULL)
				{
					delete[] d_allBandData;
					d_allBandData=NULL;
				}
			}
			if (datatype=="Unsigned short" )
			{
				if (us_allBandData!=NULL)
				{
					delete[] us_allBandData;
					us_allBandData=NULL;
				}
			}
		}
	}

	/// <����Ӱ��>
	if (pathofsimresult.size()>0&&imgList.size()>0&&divingList.size()>0&&(saveMemD!=NULL||saveMemF!=NULL||saveMemUs!=NULL))
	{
		if (datatype=="Float" )
		{
			size_t i,j,k;
			/// <����tiff�ļ�>
			TiffDataWrite pwrite;
			bool brlt = pwrite.init(pathofsimresult.c_str(), imgList.at(0)->rows(), imgList.at(0)->cols(), mvLanduseType.size(), \
				imgList.at(0)->geotransform(), imgList.at(0)->projectionRef(), GDT_Float32, -1);
			if (!brlt)
			{
				cout<<"write init error!"<<endl;
				cout<<"Save Error: "+pathofsimresult<<endl;
				return ;
			}
			float _val = 0;
			//#pragma omp parallel for private(j, k, _val), num_threads(omp_get_max_threads())
			if (saveMemF!=NULL)
			{
				for (i=0; i<pwrite.rows(); i++)
				{
					for (j=0; j<pwrite.cols(); j++)
					{
						for (k=0; k<pwrite.bandnum(); k++)
						{
							size_t datalen;

							datalen=k*pwrite.rows()*pwrite.cols()+i*pwrite.cols()+j;

							_val = saveMemF[datalen];

							pwrite.write(i, j, k, &_val);
						}
					}
				}
				cout<<"write success!"<<endl;
				pwrite.close();
				delete[] saveMemF;
				saveMemF=NULL;
			}
		}
		if (datatype=="Double" )
		{
			size_t i,j,k;
			
			TiffDataWrite pwrite;
			bool brlt = pwrite.init(pathofsimresult.c_str(), imgList.at(0)->rows(), imgList.at(0)->cols(), mvLanduseType.size(), \
				imgList.at(0)->geotransform(), imgList.at(0)->projectionRef(), GDT_Float64, -1);
			if (!brlt)
			{
				cout<<"write init error!"<<endl;
				return ;
			}
			double _val = 0;
			//#pragma omp parallel for private(j, k, _val), num_threads(omp_get_max_threads())
			if (saveMemD!=NULL)
			{
				for (i=0; i<pwrite.rows(); i++)
				{
					for (j=0; j<pwrite.cols(); j++)
					{
						for (k=0; k<pwrite.bandnum(); k++)
						{
							size_t datalen;
							datalen=k*pwrite.rows()*pwrite.cols()+i*pwrite.cols()+j;
							_val = saveMemD[datalen];
							pwrite.write(i, j, k, &_val);
						}
					}
				}
				cout<<"write success!"<<endl;
				pwrite.close();
				delete[] saveMemD;
				saveMemD=NULL;
			}
		}
		if (datatype=="Unsigned short" )
		{
			size_t i,j,k;
			
			TiffDataWrite pwrite;
			bool brlt = pwrite.init(pathofsimresult.c_str(), imgList.at(0)->rows(), imgList.at(0)->cols(), mvLanduseType.size(), \
				imgList.at(0)->geotransform(), imgList.at(0)->projectionRef(), GDT_UInt16, 0);
			if (!brlt)
			{
				cout<<"write init error!"<<endl;
				return ;
			}
			unsigned short _val = 0;
			//#pragma omp parallel for private(j, k, _val), num_threads(omp_get_max_threads())
			if (saveMemUs!=NULL)
			{
				for (i=0; i<pwrite.rows(); i++)
				{
					for (j=0; j<pwrite.cols(); j++)
					{
						for (k=0; k<pwrite.bandnum(); k++)
						{
							size_t datalen;
							datalen=k*pwrite.rows()*pwrite.cols()+i*pwrite.cols()+j;
							_val = saveMemUs[datalen];
							pwrite.write(i, j, k, &_val);
						}
					}
				}
				cout<<"write success!"<<endl;
				pwrite.close();
				delete[] saveMemUs;
				saveMemD=NULL;
			}
		}
	}
	imgList.at(0)->close();
}


template<class TT> bool NNtrain::dataCopyConvert(unsigned char* buffer)
{
	size_t _sizeofTT;
	_sizeofTT=sizeof(TT);
	TT _temp;
	if (datatype=="Float" )
	{
		float data_temp;
		if (nWidth>0&&nHeight>0&&currentBand>0)
		{
			size_t ii;
			for (ii=0;ii<nWidth*nHeight*currentBand;ii++)
			{
				_temp=*(TT*)(buffer+ii*_sizeofTT);
				data_temp=(float)_temp;
				f_allBandData[currentPos+ii]=data_temp;
			}
		}
	}
	if (datatype=="Double" )
	{
		size_t _sizeofTT;
		_sizeofTT=sizeof(TT);
		double data_temp;
		if (nWidth>0&&nHeight>0&&currentBand>0)
		{
			size_t ii;
			for (ii=0;ii<nWidth*nHeight*currentBand;ii++)
			{
				_temp=*(TT*)(buffer+ii*_sizeofTT);
				data_temp=(double)_temp;
				d_allBandData[currentPos+ii]=data_temp;
			}
		}
	}
	if (datatype=="Unsigned short" )
	{
		size_t _sizeofTT;
		_sizeofTT=sizeof(TT);
		unsigned short data_temp;
		if (nWidth>0&&nHeight>0&&currentBand>0)
		{
			size_t ii;
			for (ii=0;ii<nWidth*nHeight*currentBand;ii++)
			{
				_temp=*(TT*)(buffer+ii*_sizeofTT);
				data_temp=(unsigned short)(_temp*65534+1);
				us_allBandData[currentPos+ii]=data_temp;
			}
		}
	}
	return true;
}

void NNtrain::nnTrain()
{
	if (isNomalized==true)
	{
		string status = string("Normalizing data, please wait...");
		cout<<status<<endl;
		normalizationdata();
	}
	else
	{
		string status_s = string("Set random points, please wait...");
		cout<<status_s<<endl;
	}
	

	if (isUnifomSam==true)
	{
		string status_c = string("Select uniform sampling...");
		cout<<status_c<<endl;
		setRandomPoint(false);
	}
	else
	{
		string status_c = string("Select sampling in proportion...");
		cout<<status_c<<endl;
		setRandomPoint(true);
	}

	//-------------------alglib array-----------------------
	real_2d_array trainarr;
	size_t jj;
	double _filter;

	trainarr.setlength(numof,(bandCount+1));
	if (datatype=="Float" )
	{
		size_t i;
		size_t j;
		size_t datalen;

		for (i=0;i<bandCount;i++)
		{
			jj=0;
			for (j=0;j<numof;j++)
			{
				datalen=i*nWidth*nHeight+m_PointCoodinateX[j]*nWidth+m_PointCoodinateY[j];

				double _filter=f_allBandData[datalen];

				if (_filter<0||_filter>1)
				{
					trainarr[jj][i]=0;
				}
				else
				{
					trainarr[jj][i]=_filter;
				}
				jj++;
			}
		}
	}
	if (datatype=="Double" )
	{
		size_t i;
		size_t j;
		size_t datalen;

		for (i=0;i<bandCount;i++)
		{
			jj=0;
			for (j=0;j<numof;j++)
			{
				datalen=i*nWidth*nHeight+m_PointCoodinateX[j]*nWidth+m_PointCoodinateY[j];

				_filter=d_allBandData[datalen];

				if (_filter<0||_filter>1)
				{
					trainarr[jj][i]=0;
				}
				else
				{
					trainarr[jj][i]=_filter;
				}
				jj++;
			}
		}
	}

	if (datatype=="Unsigned short" )
	{
		size_t i;
		size_t j;
		size_t datalen;

		for (i=0;i<bandCount;i++)
		{
			jj=0;
			for (j=0;j<numof;j++)
			{
				datalen=i*nWidth*nHeight+m_PointCoodinateX[j]*nWidth+m_PointCoodinateY[j];

				_filter=us_allBandData[datalen];

				trainarr[jj][i]=(((double)_filter)-1)/65534.0;

				jj++;
			}
		}
	}

	jj=0;
	size_t j;
	size_t datalen;

	for (j=0;j<numof;j++)
	{
		datalen=m_PointCoodinateX[j]*nWidth+m_PointCoodinateY[j];

		trainarr[jj][bandCount]=(double)imgList[0]->imgData()[datalen]-1;
		jj++;
	}


	delete[] m_PointCoodinateX;
	delete[] m_PointCoodinateY;

	ofstream file;
	file.open( "./FilesGenerate/NetworkInput.csv", ios::out );
	string line;

	for (int ii=0;ii<bandName.size();ii++)
	{
		string str=bandName.at(ii);
		string _str;
		_str=str;
		line=line+_str+", ";
	}
	file << line+"class" << endl;

	double* sa=new double[bandCount+1];

	size_t ii;
	for (ii=0;ii<numof;ii++)
	{
		for (int jj=0;jj<(bandCount+1);jj++)
		{
			sa[jj]=trainarr[ii][jj];
		}
		line=putCount2File(sa,(bandCount+1));
		file <<line.substr(0,line.size()-1)<< endl;
	}
	file.close();

	//----------------------------------------build NN-------------------------------------------
	string status = string("Start training, please wait...");
	cout<<status<<endl;

	//initial net
	mlptrainer trn;
	multilayerperceptron network;
	mlpreport rep;

	//7 features,3 classes
	mlpcreatetrainercls(bandCount, mvLanduseType.size(), trn);
	//7 features, 3 classes, and 10 hidden
	mlpcreatec1(bandCount,numHiddenLayer, mvLanduseType.size(), network);
	//num5 training data
	mlpsetdataset(trn, trainarr, numof);



	//training 5 times
	double TimeStart=GetTickCount();
	mlptrainnetwork(trn, network, 1, rep);
	double TimeEnd=GetTickCount();
	double TimeUsed=(TimeEnd-TimeStart)/1000;
	//output
	string status0 = string("Run time: ");
	status0=status0+num2str(TimeUsed)+" s";
	cout<<status0<<endl;
// 	//training parameters
	string status1 = string("Precision evaluation: ");
	cout<<status1<<endl;
// 	
// 	string status2 = string("relclserror =  ").arg(rep.relclserror);
// 	sendParameter(status2);
// // 	
// 	string status3 = string("avgce =  ").arg(rep.avgce);
// 	sendParameter(status3);
// // 
	string status4 = string("RMSE =  ");
	status4=status4+num2str(rep.rmserror);
	cout<<status4<<endl;
// // 
	string status5 = string("Average error =  ");
	status5=status5+num2str(rep.avgerror);
	cout<<status5<<endl;
// 
	string status6 = string("Average relative error =  ");
	status6=status6+num2str(rep.avgrelerror);
	cout<<status6<<endl;

// 	string status7 = string("ngrad =   ").arg(rep.ngrad);
// 	sendParameter(status7);
// 
// 	string status8 = string("nhess =   ").arg(rep.nhess);
// 	sendParameter(status8);
// 
// 	string status9 = string("ncholesky =   ").arg(rep.ncholesky);
// 	sendParameter(status9);


	trainarr.setlength(0,0);

	string status_F = string("Waiting for prediction...");
	cout<<status_F<<endl;
	//predict classes
	real_1d_array prearr;
	prearr.setlength(bandCount);
	//output array
	real_1d_array dst;
	//3 bands picture



	// Update data--------------------------------------------------------------------------------------------------------------


	if (isUpdateData==true)
	{
		updateDataprepared();
		updatePredictData();
		for (int ii=0;ii<updatedivingList.size();ii++)
		{
			updatedivingList[ii]->close();
		}
		if (f_allBandData_update!=NULL)
		{
			delete[]f_allBandData_update;
			f_allBandData_update=NULL;
		}
		if (d_allBandData_update!=NULL)
		{
			delete[]d_allBandData_update;
			d_allBandData_update=NULL;
		}
		if (us_allBandData_update!=NULL)
		{
			delete[]us_allBandData_update;
			us_allBandData_update=NULL;
		}
	}


	// Update data--------------------------------------------------------------------------------------------------------------




	isNoDataExit=true;
	if (datatype=="Float" )
	{
		size_t _a=mvLanduseType.size();
		saveMemF=new float[nWidth*nHeight*_a];

		size_t i;
		size_t j;
		size_t k;

		for (i=0;i<nHeight;i++)
		{
			for (j=0;j<nWidth;j++)
			{
				if (isNoDataExit==false||(noDataValue!=imgList[0]->imgData()[i*nWidth+j]&&imgList[0]->imgData()[i*nWidth+j]!=0))
				{
					for (k=0;k<bandCount;k++)
					{
						prearr[k]=f_allBandData[k*nHeight*nWidth+i*nWidth+j];
					}
					mlpprocess(network, prearr, dst);
					size_t ii;
					for(ii=0;ii<dst.length();ii++)
					{
						saveMemF[nWidth*nHeight*ii+i*nWidth+j]=(float)dst[ii];
					}
				}
				else
				{
					size_t ii;
					for(ii=0;ii<mvLanduseType.size();ii++)
					{
						saveMemF[nWidth*nHeight*ii+i*nWidth+j]=-1;
					}
				}
			}
		}
	}
	if (datatype=="Double" )
	{
		size_t _a=mvLanduseType.size();
		saveMemD=new double[nWidth*nHeight*_a];

		size_t i;
		size_t j;
		size_t k;

		for (i=0;i<nHeight;i++)
		{
			for (j=0;j<nWidth;j++)
			{
				if (isNoDataExit==false||(noDataValue!=imgList[0]->imgData()[i*nWidth+j]&&imgList[0]->imgData()[i*nWidth+j]!=0))
				{
					for (k=0;k<bandCount;k++)
					{
						prearr[k]=d_allBandData[k*nHeight*nWidth+i*nWidth+j];
					}
					mlpprocess(network, prearr, dst);

					size_t ii;
					for(ii=0;ii<dst.length();ii++)
					{
						saveMemD[nWidth*nHeight*ii+i*nWidth+j]=(double)dst[ii];
					}
				}
				else
				{
					size_t ii;
					for(ii=0;ii<mvLanduseType.size();ii++)
					{
						saveMemD[nWidth*nHeight*ii+i*nWidth+j]=-1;
					}
				}
			}
		}
	}
	if (datatype=="Unsigned short" )
	{
		size_t _a=mvLanduseType.size();
		saveMemUs=new unsigned short[nWidth*nHeight*_a];

		size_t i;
		size_t j;
		size_t k;

		for (i=0;i<nHeight;i++)
		{
			for (j=0;j<nWidth;j++)
			{
				if (isNoDataExit==false||(noDataValue!=imgList[0]->imgData()[i*nWidth+j]&&imgList[0]->imgData()[i*nWidth+j]!=0))
				{
					for (k=0;k<bandCount;k++)
					{
						prearr[k]=((double)us_allBandData[k*nHeight*nWidth+i*nWidth+j]-1)/65534.0;
					}
					mlpprocess(network, prearr, dst);

					size_t ii;
					for(ii=0;ii<dst.length();ii++)
					{
						saveMemUs[nWidth*nHeight*ii+i*nWidth+j]=(unsigned short)(dst[ii]*65534+1);
					}
				}
				else
				{
					size_t ii;
					for(ii=0;ii<mvLanduseType.size();ii++)
					{
						saveMemUs[nWidth*nHeight*ii+i*nWidth+j]=0;
					}
				}
			}
		}
	}
}

void NNtrain::saveNormalizedData()
{


	float* tempNormData=new float[nWidth*nHeight];

	size_t kk,ii;

	for (kk=0;kk<qlredat.size();kk++)
	{
		for (ii=0;ii<nWidth*nHeight;ii++)
		{
			tempNormData[ii]=f_allBandData_update[kk*nWidth*nHeight+ii];
		}
		
		string fname;

		fname=num2str(qlredat.at(kk).num);

		fname=fname+".tif";

		GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
		char **papszMetadata = NULL;
		GDALDataset* poDataset2=poDriver->Create(fname.c_str(),nWidth,nHeight,1,GDT_Float32,papszMetadata);

		poDataset2->SetGeoTransform(imgList.at(0)->geotransform());
 		poDataset2->SetProjection(imgList.at(0)->projectionRef());
		poDataset2->GetRasterBand(1)->SetNoDataValue(-1);

		CPLErr err = poDataset2->RasterIO(GF_Write, 0, 0,nWidth,nHeight,tempNormData,nWidth,nHeight,GDT_Float32, 1, 0, 0, 0, 0);
		GDALClose(poDataset2);

		if (err==CE_None)
		{
			cout<<"Write data successed!"<<endl;

		}
	}
	delete[] tempNormData;
	tempNormData=NULL;

}

bool NNtrain::normalizationdata()
{
	size_t i, j, k=0;
	double temp;
	double max1;
	double min1;
	double nodata;

	if (datatype=="Float" )
	{
		for (k=0; k<bandCount; k++)
		{
			min1=minmaxsave[k*3+0];
			max1=minmaxsave[k*3+1];
			nodata=minmaxsave[k*3+2];

			for (i=0; i<nWidth; i++)
			{
				for (j=0; j<nHeight; j++)
				{	
					if (isNoDataExit==false||(noDataValue!=imgList[0]->imgData()[j*nWidth+i]&&imgList[0]->imgData()[j*nWidth+i]!=0))
					{
						temp=(f_allBandData[k*nWidth*nHeight+j*nWidth+i]-min1)/(max1-min1);
						if (f_allBandData[k*nWidth*nHeight+j*nWidth+i]==nodata)
						{
							temp=0;//in case of strange value
						}
						f_allBandData[k*nWidth*nHeight+j*nWidth+i]=temp;
					}
					else
					{
						f_allBandData[k*nWidth*nHeight+j*nWidth+i]=-1;
					}
				}
			}
		}
	}

	if (datatype=="Double" )
	{
		for (k=0; k<bandCount; k++)
		{
			min1=minmaxsave[k*3+0];
			max1=minmaxsave[k*3+1];
			nodata=minmaxsave[k*3+2];

			for (i=0; i<nWidth; i++)
			{
				for (j=0; j<nHeight; j++)
				{	
					if (isNoDataExit==false||(noDataValue!=imgList[0]->imgData()[j*nWidth+i]&&imgList[0]->imgData()[j*nWidth+i]!=0))
					{
						temp=(d_allBandData[k*nWidth*nHeight+j*nWidth+i]-min1)/(max1-min1);
						if (d_allBandData[k*nWidth*nHeight+j*nWidth+i]==nodata)
						{
							temp=0;//in case of strange value
						}
						d_allBandData[k*nWidth*nHeight+j*nWidth+i]=temp;

					}
					else
					{
						d_allBandData[k*nWidth*nHeight+j*nWidth+i]=-1;
					}
				}
			}
		}
	}

	if (datatype=="Unsigned short" )
	{
		for (k=0; k<bandCount; k++)
		{
			min1=minmaxsave[k*3+0];
			max1=minmaxsave[k*3+1];
			nodata=minmaxsave[k*3+2];

			for (i=0; i<nWidth; i++)
			{
				for (j=0; j<nHeight; j++)
				{	
					if (isNoDataExit==false||(noDataValue!=imgList[0]->imgData()[j*nWidth+i]&&imgList[0]->imgData()[j*nWidth+i]!=0))
					{
						temp=(us_allBandData[k*nWidth*nHeight+j*nWidth+i]-min1)/(max1-min1);
						if (us_allBandData[k*nWidth*nHeight+j*nWidth+i]==nodata)
						{
							temp=0;//in case of strange value
						}
						us_allBandData[k*nWidth*nHeight+j*nWidth+i]=(unsigned short)(temp*65534+1);

					}
					else
					{
						us_allBandData[k*nWidth*nHeight+j*nWidth+i]=0;
					}
				}
			}
		}
	}

	//saveNormalizedData();

	return true;
}

bool NNtrain::normalizeUpdateData()
{
	size_t i, j, k=0;
	double temp;
	double max1;
	double min1;
	double nodata;

	if (datatype=="Float" )
	{
		for (k=0; k<qlredat.size(); k++)
		{
			min1=minmaxsaveupdate[k*3+0];
			max1=minmaxsaveupdate[k*3+1];
			nodata=minmaxsaveupdate[k*3+2];

			for (i=0; i<nWidth; i++)
			{
				for (j=0; j<nHeight; j++)
				{	
					if (isNoDataExit==false||(noDataValue!=imgList[0]->imgData()[j*nWidth+i]&&imgList[0]->imgData()[j*nWidth+i]!=0))
					{
						temp=(f_allBandData_update[k*nWidth*nHeight+j*nWidth+i]-min1)/(max1-min1);
						if (f_allBandData_update[k*nWidth*nHeight+j*nWidth+i]==nodata)
						{
							temp=0;//in case of strange value
						}
						f_allBandData_update[k*nWidth*nHeight+j*nWidth+i]=temp;
					}
					else
					{
						f_allBandData_update[k*nWidth*nHeight+j*nWidth+i]=-1;
					}
				}
			}
		}
	}

	if (datatype=="Double" )
	{
		for (k=0; k<qlredat.size(); k++)
		{
			min1=minmaxsaveupdate[k*3+0];
			max1=minmaxsaveupdate[k*3+1];
			nodata=minmaxsaveupdate[k*3+2];

			for (i=0; i<nWidth; i++)
			{
				for (j=0; j<nHeight; j++)
				{	
					if (isNoDataExit==false||(noDataValue!=imgList[0]->imgData()[j*nWidth+i]&&imgList[0]->imgData()[j*nWidth+i]!=0))
					{
						temp=(d_allBandData_update[k*nWidth*nHeight+j*nWidth+i]-min1)/(max1-min1);
						if (d_allBandData_update[k*nWidth*nHeight+j*nWidth+i]==nodata)
						{
							temp=0;//in case of strange value
						}
						d_allBandData_update[k*nWidth*nHeight+j*nWidth+i]=temp;

					}
					else
					{
						d_allBandData_update[k*nWidth*nHeight+j*nWidth+i]=-1;
					}
				}
			}
		}
	}

	if (datatype=="Unsigned short" )
	{
		for (k=0; k<qlredat.size(); k++)
		{
			min1=minmaxsaveupdate[k*3+0];
			max1=minmaxsaveupdate[k*3+1];
			nodata=minmaxsaveupdate[k*3+2];

			for (i=0; i<nWidth; i++)
			{
				for (j=0; j<nHeight; j++)
				{	
					if (isNoDataExit==false||(noDataValue!=imgList[0]->imgData()[j*nWidth+i]&&imgList[0]->imgData()[j*nWidth+i]!=0))
					{
						temp=(us_allBandData_update[k*nWidth*nHeight+j*nWidth+i]-min1)/(max1-min1);
						if (us_allBandData_update[k*nWidth*nHeight+j*nWidth+i]==nodata)
						{
							temp=0;//in case of strange value
						}
						us_allBandData_update[k*nWidth*nHeight+j*nWidth+i]=(unsigned short)(temp*65534+1);

					}
					else
					{
						us_allBandData_update[k*nWidth*nHeight+j*nWidth+i]=0;
					}
				}
			}
		}
	}

	if (datatype=="Float" )
	{
		saveNormalizedData();
	}

	return true;
}


template<class TT>
bool NNtrain::dataCopyConvertUpdate(unsigned char* buffer)
{
	size_t _sizeofTT;
	_sizeofTT=sizeof(TT);
	TT _temp;
	if (datatype=="Float" )
	{
		float data_temp;
		if (nWidth>0&&nHeight>0&&currentBand>0)
		{
			size_t ii;
			for (ii=0;ii<nWidth*nHeight*currentBand;ii++)
			{
				_temp=*(TT*)(buffer+ii*_sizeofTT);
				data_temp=(float)_temp;
				f_allBandData_update[currentPos+ii]=data_temp;
			}
		}
	}
	if (datatype=="Double" )
	{
		size_t _sizeofTT;
		_sizeofTT=sizeof(TT);
		double data_temp;
		if (nWidth>0&&nHeight>0&&currentBand>0)
		{
			size_t ii;
			for (ii=0;ii<nWidth*nHeight*currentBand;ii++)
			{
				_temp=*(TT*)(buffer+ii*_sizeofTT);
				data_temp=(double)_temp;
				d_allBandData_update[currentPos+ii]=data_temp;
			}
		}
	}
	if (datatype=="Unsigned short" )
	{
		size_t _sizeofTT;
		_sizeofTT=sizeof(TT);
		unsigned short data_temp;
		if (nWidth>0&&nHeight>0&&currentBand>0)
		{
			size_t ii;
			for (ii=0;ii<nWidth*nHeight*currentBand;ii++)
			{
				_temp=*(TT*)(buffer+ii*_sizeofTT);
				data_temp=(unsigned short)(_temp*65534+1);
				us_allBandData_update[currentPos+ii]=data_temp;
			}
		}
	}
	return true;
}


void NNtrain::updateDataprepared()
{
	updatebandCount=0;

	for (int ii=0;ii<qlredat.size();ii++)
	{
		updatebandCount+=updatedivingList.at(ii)->bandnum();

		if (updatedivingList[ii]->bandnum()==1)
		{
			int pos = qlredat[ii].str.find_last_of('\\');
			string namestring(qlredat[ii].str.substr(pos + 1));
			namestring=namestring.substr(0,namestring.length()-4);
			updatebandName.push_back(namestring);
		}
		else
		{
			int pos = qlredat[ii].str.find_last_of('\\');
			string namestring(qlredat[ii].str.substr(pos + 1));

			for (int kk=0;kk<updatedivingList[ii]->bandnum();kk++)
			{  
				string str1=namestring+"_band"+num2str(kk+1);
				updatebandName.push_back(str1);
			}
		}
	}


	if (updatebandCount>0)
	{
		if (datatype=="Float" )
		{
			f_allBandData_update=new float[nWidth*nHeight*updatebandCount];
		}
		if (datatype=="Double" )
		{
			d_allBandData_update=new double[nWidth*nHeight*updatebandCount];
		}
		if (datatype=="Unsigned short" )
		{
			us_allBandData_update=new unsigned short[nWidth*nHeight*updatebandCount];
		}

		currentPos=0;
		currentBand=0;
		//get data

		double* minmax1=new double[2];

		bool bRlt = false;
		for (int ii=0;ii<updatedivingList.size();ii++)
		{
			updatedivingList.at(ii)->loadData();
			currentBand=updatedivingList.at(ii)->bandnum();

			bandInfo bio;
			bio.dataNum=ii;
			bio.bandCom=currentBand;
			qlbio.push_back(bio);

			int a;
			a=qlbio.at(0).dataNum;

			for (int kk=0;kk<currentBand;kk++)
			{
				updatedivingList.at(ii)->poDataset()->GetRasterBand(1+kk)->ComputeRasterMinMax(1,minmax1);

				float nodatavalue_factor=updatedivingList[ii]->poDataset()->GetRasterBand(1)->GetNoDataValue();
				double nodatavalue_factord=updatedivingList[ii]->poDataset()->GetRasterBand(1)->GetNoDataValue();
				if (minmax1[0]==nodatavalue_factor||minmax1[0]==nodatavalue_factord)
				{
					minmax1[0]=0;
				}

				minmaxsaveupdate.push_back(minmax1[0]);
				minmaxsaveupdate.push_back(minmax1[1]);
				minmaxsaveupdate.push_back(updatedivingList.at(ii)->poDataset()->GetRasterBand(1+kk)->GetNoDataValue());
			}

			switch(updatedivingList.at(ii)->datatype())
			{
			case GDT_Byte:
				bRlt = dataCopyConvertUpdate<unsigned char>(updatedivingList.at(ii)->imgData());
				break;
			case GDT_UInt16:
				bRlt = dataCopyConvertUpdate<unsigned short>(updatedivingList.at(ii)->imgData());
				break;
			case GDT_Int16:
				bRlt = dataCopyConvertUpdate<short>(updatedivingList.at(ii)->imgData());
				break;
			case GDT_UInt32:
				bRlt = dataCopyConvertUpdate<unsigned int>(updatedivingList.at(ii)->imgData());
				break;
			case GDT_Int32:
				bRlt = dataCopyConvertUpdate<int>(updatedivingList.at(ii)->imgData());
				break;
			case GDT_Float32:
				bRlt = dataCopyConvertUpdate<float>(updatedivingList.at(ii)->imgData());
				break;
			case GDT_Float64:
				bRlt = dataCopyConvertUpdate<double>(updatedivingList.at(ii)->imgData());
				break;
			default:
				cout<<"CGDALRead::loadFrom : unknown data type!"<<endl;

			}

			currentPos+=nWidth*nHeight*currentBand;

		}

		delete[] minmax1;
	}

	normalizeUpdateData();

}

void NNtrain::updatePredictData()
{
	cout<<"update prediction data"<<endl;

	for (int ii=0;ii<updatedivingList.size();ii++)
	{
	
		int updateband=0;
		for (int jj=0;jj<qlredat.size();jj++)
		{
			int startBandSum=0;
			for (int zz=0;zz<qlbio.size();zz++)
			{
			    if (qlbio.at(zz).dataNum==qlredat.at(jj).num)
			    {
					if (datatype=="Float" )
					{
						for (int kk=0;kk<nWidth*nHeight;kk++)
						{
							for (int xx=0;xx<qlbio.at(zz).bandCom;xx++)
							{
								f_allBandData[startBandSum*nWidth*nHeight+xx*nWidth*nHeight+kk]=f_allBandData_update[updateband*nWidth*nHeight+xx*nWidth*nHeight+kk];
							}
						}
					}
					if (datatype=="Double" )
					{
						for (int kk=0;kk<nWidth*nHeight;kk++)
						{
							for (int xx=0;xx<qlbio.at(zz).bandCom;xx++)
							{
								d_allBandData[startBandSum*nWidth*nHeight+xx*nWidth*nHeight+kk]=d_allBandData_update[updateband*nWidth*nHeight+xx*nWidth*nHeight+kk];
							}
						}
					}
					if (datatype=="Unsigned short" )
					{
						for (int kk=0;kk<nWidth*nHeight;kk++)
						{
							for (int xx=0;xx<qlbio.at(zz).bandCom;xx++)
							{
								us_allBandData[startBandSum*nWidth*nHeight+xx*nWidth*nHeight+kk]=us_allBandData_update[updateband*nWidth*nHeight+xx*nWidth*nHeight+kk];
							}
						}
					}
			    }
				startBandSum+=qlbio.at(zz).bandCom;
			}
			updateband+=updatedivingList.at(ii)->bandnum();
			
		}
	}

}

struct Coor
{
	int nRow;
	int nCol;
};

void coorPushback(vector<Coor> &vecCoor,const int &_row,const int &_col)
{
	Coor coor;
	coor.nRow=_row;
	coor.nCol=_col;
	vecCoor.push_back(coor);
}

bool NNtrain::setRandomPoint(bool _stra)
{

	srand(resolveNNRandomSeed());

	numof=0;

	for (int ii=0;ii<mvCountType.size();ii++)
	{
		numof+=mvCountType[ii];
	}

	sort(mvCountType.begin(),mvCountType.end());

	numof=numof*samplingRate*0.001;

	if (numof/mvCountType.size()>mvCountType[0]&&_stra==false)
	{
		string status = string("The sampling points is too much!  ");
		status=status+num2str(numof);
		cout<<(status)<<endl;
		return false;
	}
	m_PointCoodinateX=new int[numof];
	m_PointCoodinateY=new int[numof];
	bool label=false;
	size_t i=0,j=0,xc,yc;
	int al_num=0,bl_num=0;
	vector<int> l_num;
	for (int ii=0;ii<mvLanduseType.size();ii++)
	{
		l_num.push_back(0);
	}


	if (_stra==true)
	{
		while (i<numof)
		{
			xc=randomFunction(nHeight);
			yc=randomFunction(nWidth);
			label=true;
			if (isNoDataExit==false||(noDataValue!=imgList[0]->imgData()[xc*nWidth+yc]&&imgList[0]->imgData()[xc*nWidth+yc]!=0))
			{
				for (j=0;j<al_num;j++) 
				{
					if (m_PointCoodinateX[j]==xc&&m_PointCoodinateY[j]==yc)
					{
						label=false;
						break;
					}
				}
				if (label==true&&imgList[0]->imgData()[xc*nWidth+yc]>0)
				{
					m_PointCoodinateX[i]=xc;
					m_PointCoodinateY[i]=yc;
					al_num=i;
					i++;
				}
			}
		}
		
		for (i=0;i<numof;i++)
		{
			for (int _ii=0;_ii<mvLanduseType.size();_ii++)
			{
				if ((int)imgList[0]->imgData()[xc*nWidth+yc]==mvLanduseType.at(_ii))
				{
					l_num[_ii]+=1;
				}
			}
		}
	}
	else
	{
		vector<Coor> coorSavior;

		int nType=mvLanduseType.size();

		int numofeach=ceil((double)(numof*1.0/nType));

		int _label;

		size_t nCurrent=0;


		for (int kk=0;kk<nType;kk++)
		{
			size_t ii;
			for (ii=0;ii<nHeight;ii++)
			{
				size_t jj;
				for (jj=0;jj<nWidth;jj++)
				{
					_label=mvLanduseType.at(kk);

					if (_label==imgList[0]->imgData()[ii*nWidth+jj])
					{
						coorPushback(coorSavior,ii,jj);
					}
				}
			}


			random_shuffle(coorSavior.begin(),coorSavior.end());

			for(ii=0;ii<numofeach;ii++)
			{
				if (nCurrent<numof)
				{
					m_PointCoodinateX[nCurrent]=coorSavior[ii].nRow;
					m_PointCoodinateY[nCurrent]=coorSavior[ii].nCol;
					nCurrent++;
				}
			}

			coorSavior.clear();

		}
	}

	return true;
}

size_t NNtrain::randomFunction( size_t _lon )
{

	double dourp=(double)rand()/(double)RAND_MAX;

	size_t inrp=floor((_lon-1)*dourp);

	return inrp;
}
