// FLUS_console - Cross-platform version
//

#include <simulationprocess.h>
#include <nntrain.h>
#include <TiffDataRead.h>
#include <TiffDataWrite.h>
#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <map>

#ifdef _RANK
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <stack>
#endif

int main(int argc, char* argv[])
{
	GDALAllRegister();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");

	//NNtrain* nn=new NNtrain("CCregiontrainlogCC.txt");
	//nn->trainprocess();

	SimulationProcess* sp=new SimulationProcess("CCregionsimlog.txt","CCregionMakovChain.csv");
	sp->runFLUS();

	return 0;
}
