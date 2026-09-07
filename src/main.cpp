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

	if (argc >= 2 && std::string(argv[1]) == "train")
	{
		std::string trainConfig = argc >= 3 ? argv[2] : "CCregiontrainlogCC.txt";
		NNtrain* nn=new NNtrain(trainConfig);
		if (!nn->ready())
		{
			delete nn;
			return 3;
		}
		nn->trainprocess();
		delete nn;
		return 0;
	}
	if (argc >= 2 && std::string(argv[1]) == "train-update")
	{
		if (argc < 4)
		{
			std::cerr << "Usage: flus_console train-update <train-config> <update-csv>" << std::endl;
			return 2;
		}
		NNtrain* nn=new NNtrain(argv[2], argv[3]);
		if (!nn->ready())
		{
			delete nn;
			return 3;
		}
		nn->trainprocess();
		delete nn;
		return 0;
	}

	SimulationProcess* sp=new SimulationProcess("CCregionsimlog.txt","CCregionMakovChain.csv");
	bool ok=sp->runFLUS();
	delete sp;

	return ok ? 0 : 3;
}
