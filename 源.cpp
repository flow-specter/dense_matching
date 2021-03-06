#include<numeric>
#include <algorithm> //vector成员函数头文件
#include<windows.h>    //头文件  
#include <iomanip>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "opencv2/imgproc/imgproc.hpp"
#include <vector>
#include "G:/MVLL/MVLL/testYLJ/include/RFMBaseFunction.h"
#include "CubicSplineInterpolation.h"
#define Z_resolution 1
#include <iostream>
#define PATHS_PER_SCAN 8
#define DEBUG false
using namespace std;
using namespace cv;

struct path {
	short rowDiff;
	short colDiff;
	short index;
};

void initializeFirstScanPaths(std::vector<path> &paths, unsigned short pathCount){
	/* 这里用了 4 个或者 8 个 */
	for (unsigned short i = 0; i < pathCount; ++i)
	{
		paths.push_back(path());
	}

	if (paths.size() >= 1)
	{
		paths[0].rowDiff = 0;
		paths[0].colDiff = -1;
		paths[0].index = 1;
	}

	if (paths.size() >= 2)
	{
		paths[1].rowDiff = -1;
		paths[1].colDiff = 0;
		paths[1].index = 2;
	}

	if (paths.size() >= 4) {
		paths[2].rowDiff = -1;
		paths[2].colDiff = 1;
		paths[2].index = 4;

		paths[3].rowDiff = -1;
		paths[3].colDiff = -1;
		paths[3].index = 7;
	}

	if (paths.size() >= 8) {
		paths[4].rowDiff = -2;
		paths[4].colDiff = 1;
		paths[4].index = 8;

		paths[5].rowDiff = -2;
		paths[5].colDiff = -1;
		paths[5].index = 9;

		paths[6].rowDiff = -1;
		paths[6].colDiff = -2;
		paths[6].index = 13;

		paths[7].rowDiff = -1;
		paths[7].colDiff = 2;
		paths[7].index = 15;
	}
}

void initializeSecondScanPaths(std::vector<path> &paths, unsigned short pathCount){
	for (unsigned short i = 0; i < pathCount; ++i)
	{
		paths.push_back(path());
	}

	if (paths.size() >= 1) {
		paths[0].rowDiff = 0;
		paths[0].colDiff = 1;
		paths[0].index = 0;
	}

	if (paths.size() >= 2) {
		paths[1].rowDiff = 1;
		paths[1].colDiff = 0;
		paths[1].index = 3;
	}

	if (paths.size() >= 4) {
		paths[2].rowDiff = 1;
		paths[2].colDiff = 1;
		paths[2].index = 5;

		paths[3].rowDiff = 1;
		paths[3].colDiff = -1;
		paths[3].index = 6;
	}

	if (paths.size() >= 8) {
		paths[4].rowDiff = 2;
		paths[4].colDiff = 1;
		paths[4].index = 10;

		paths[5].rowDiff = 2;
		paths[5].colDiff = -1;
		paths[5].index = 11;

		paths[6].rowDiff = 1;
		paths[6].colDiff = -2;
		paths[6].index = 12;

		paths[7].rowDiff = 1;
		paths[7].colDiff = 2;
		paths[7].index = 14;
	}
}

uchar get_origin_value(cv::Mat& input_img, float sample, float line)
{
	int i = line;
	int j = sample;

	//注意处理边界问题，容易越界,若越界则暂时将灰度值设为0
	if (i + 1 >= input_img.rows || j + 1 >= input_img.cols)
	{
		//uchar* p = input_img.ptr<uchar>(i);
		return 0; //改于 0121.19:04
		//return p[j];
	}
	if (i <= 0 || j <= 0)
	{
		return 0;
	}

	uchar* p = input_img.ptr<uchar>(i);
	return p[j];
}

float printProgress(unsigned int current, unsigned int max, int lastProgressPrinted){
	int progress = floor(100 * current / (float)max);
	if (progress >= lastProgressPrinted + 5) {
		lastProgressPrinted = lastProgressPrinted + 5;
		std::cout << lastProgressPrinted << "%" << std::endl;
	}
	return lastProgressPrinted;
}
float aggregateCost(int row, int col, int ThisZ, path &p, int rows, int cols, float  **TmpThisZMax, float  **TmpThisZMin, float ***C, float ***A, int sufferZ, int largePenalty, int smallPenalty){

	float aggregatedCost = 0;
	aggregatedCost += C[row][col][ThisZ]; /* 像素匹配的 cost 值 */
	/* 处理超出图像边缘外的情况 */
	if (row + p.rowDiff < 0 || row + p.rowDiff >= rows || col + p.colDiff < 0 || col + p.colDiff >= cols)
	{
		// border
		A[row][col][ThisZ] += aggregatedCost;
		return A[row][col][ThisZ];
	}

	/* 没有超出图像边缘外 */
	double minPrev, minPrevOther, prev, prevPlus, prevMinus;

	/* 初始值为最大值  */
	prev = minPrev = minPrevOther = prevPlus = prevMinus = 100000;//取一个最大值

	/*
	minPrev: 对应路径的视差代价最小值
	*/

	/* 在视差之间进行循环，理解其中的意思 */
	int PreheightRange = ceil((TmpThisZMax[row + p.rowDiff][col + p.colDiff] - TmpThisZMin[row + p.rowDiff][col + p.colDiff]) / Z_resolution + 1);
	for (int prevZ = 0; prevZ < PreheightRange; ++prevZ) //heightRange为上一个像素的搜索范围
	{
		unsigned short tmp = A[row + p.rowDiff][col + p.colDiff][prevZ];
		//找到这个路径下，前一个像素取不同disparity值时最小的A，这就是最后减去的那一项
		if (minPrev > tmp)
		{
			minPrev = tmp;
		}
		//前一个像素disparity取值为d是，其最小的A，这是公式中的第一项
		if (prevZ == ThisZ)    /* 视差与当前像素的视差相等 */
		{
			prev = tmp;
		}
		else if (prevZ == ThisZ + sufferZ)  /* 视差与当前像素的视差差一 */   //这是公式中的第三项，最后加的惩罚系数P1
		{
			prevPlus = tmp;
		}
		else if (prevZ == ThisZ - sufferZ) //这是公式中的第二项，最后加的惩罚系数P1
		{
			prevMinus = tmp;
		}
		else
		{
			/* 视差与当前像素视差大于 2 的情况 */
			//这是公式中的第四项，除了前一个像素的disparity取值为d的情况，其他情况下的最小的A，可是为什么加了两边P2  ？？？？？
			if (minPrevOther > tmp + largePenalty)
			{
				minPrevOther = tmp + largePenalty;
			}
		}
	} // for (int disp = 0; disp < heightRange; ++disp) 

	/* 计算最小值 */
	aggregatedCost += min(min((int)prevPlus + smallPenalty, (int)prevMinus + smallPenalty), min((int)prev, (int)minPrevOther + largePenalty));
	aggregatedCost -= minPrev;

	A[row][col][ThisZ] += aggregatedCost;
	return A[row][col][ThisZ];
}

void aggregateCosts(int rows, int cols, float  **TmpThisZMax, float  **TmpThisZMin, float  ***C, float  ****A, float  ***S, float sufferZ, int largePenalty, int smallPenalty){
	int NumOfPaths = 8;
	std::vector<path> firstScanPaths;
	std::vector<path> secondScanPaths;
	/* 初始化扫描路径           */
	/* #define TheseAggreArgs.NumOfPaths 4 */
	initializeFirstScanPaths(firstScanPaths, NumOfPaths);
	initializeSecondScanPaths(secondScanPaths, NumOfPaths);

	/*
	9       8
	13  7   2   4    15
	1       0
	12  6   3   5    14
	11      10

	first : 1,2,4,7, 8, 9,13,15
	second:	0,3,5,6,10,11,12,14
	*/

	int lastProgressPrinted = 0;
	std::cout << "First scan..." << std::endl;
	/* 第一次扫描 */
	for (int row = 0; row < rows; ++row)
	{
		for (int col = 0; col < cols; ++col)
		{
			for (unsigned int path = 0; path < firstScanPaths.size(); ++path)
			{
				int heightRange = ceil((TmpThisZMax[row][col] - TmpThisZMin[row][col]) / Z_resolution + 1);

				for (int z = 0; z < heightRange; ++z)
				{
					S[row][col][z] += aggregateCost(row, col, z, firstScanPaths[path], rows, cols, TmpThisZMax, TmpThisZMin, C, A[path], sufferZ, largePenalty, smallPenalty);
				}
			}
		}
		lastProgressPrinted = printProgress(row, rows - 1, lastProgressPrinted);
	}

	lastProgressPrinted = 0;
	std::cout << "Second scan..." << std::endl;
	/* 第二次扫描，顺序与第一次扫描不一样 */
	for (int row = rows - 1; row >= 0; --row)
	{
		for (int col = cols - 1; col >= 0; --col)
		{
			for (unsigned int path = 0; path < secondScanPaths.size(); ++path)
			{
				int heightRange = ceil((TmpThisZMax[row][col] - TmpThisZMin[row][col]) / Z_resolution + 1);

				for (int z = 0; z < heightRange; ++z)
				{
					S[row][col][z] += aggregateCost(row, col, z, secondScanPaths[path], rows, cols, TmpThisZMax, TmpThisZMin, C, A[path], sufferZ, largePenalty, smallPenalty);
				}
			}
		}
		//打印进度条
		lastProgressPrinted = printProgress(rows - 1 - row, rows - 1, lastProgressPrinted);
	}
}

struct P3D{
	double lat;
	double lon;
	float z;
	float upHei;
	float downHei;
};

struct ImgPoint{
	double line;
	double sample;
};

double groundCalCorr(int LengthOfWin, P3D ground,double roughRes,
	double UL_lon, double UL_lat,Mat img1, Mat img2, Mat RoughDem, vector<RPCMODEL>  rpcs, int baseindex, int searchIndex){
	/*
	调用示例：


	*/

	double thinRes = roughRes / 3;
	//定义两个小窗口Mat
	cv::Mat win1(LengthOfWin, LengthOfWin, CV_8UC1); win1.setTo(0);
	cv::Mat win2(LengthOfWin, LengthOfWin, CV_8UC1); win2.setTo(0);

	//给win1和win2赋值
	for (int i = 0; i < LengthOfWin; i++)
	{
		uchar* p_win1 = win1.ptr<uchar>(i);
		uchar* p_win2 = win2.ptr<uchar>(i); //指针用于赋值

		for (int j = 0; j <= LengthOfWin; j++)
		{
			double tmpSampleBase, tmpLineBase, tmpSampleSearch, tmpLineSearch;
			P3D tmpGroundP3D; int tmpX_ind, tmpY_ind;
			tmpGroundP3D.lat = thinRes*i + ground.lat - thinRes*(LengthOfWin - 1) / 2;
			tmpGroundP3D.lon = thinRes*j + ground.lon - thinRes*(LengthOfWin - 1) / 2;
			tmpX_ind = (tmpGroundP3D.lon - UL_lon) / roughRes;
			tmpY_ind = (UL_lat - tmpGroundP3D.lat) / roughRes;
			/*float* p = RoughDem.ptr<float>(tmpY_ind);*/

			tmpGroundP3D.z = ground.z; //和ground的Z值一致
			//tmpGroundP3D.z = p[tmpX_ind]; //roughDem行列处的高程

			if (tmpX_ind < 0 || tmpX_ind > RoughDem.cols
				|| tmpY_ind < 0 || tmpY_ind> RoughDem.rows || abs(tmpGroundP3D.z)>100000)
			{
				return 0;
			}

			//1. 确定地面点的三维坐标
			API_LATLONGHEIFHT2LineSample(rpcs[baseindex], tmpGroundP3D.lat, tmpGroundP3D.lon, tmpGroundP3D.z, tmpSampleBase, tmpLineBase);
			API_LATLONGHEIFHT2LineSample(rpcs[searchIndex], tmpGroundP3D.lat, tmpGroundP3D.lon, tmpGroundP3D.z, tmpSampleSearch, tmpLineSearch);

			//2. 投影至像方，得到像方点坐标，进而得到像方点的灰度值，赋给小mat
			p_win1[j] = get_origin_value(img1, tmpSampleBase, tmpLineBase);
			p_win2[j] = get_origin_value(img2, tmpSampleSearch, tmpLineSearch);
			//测试
			//cout << (int)p_win1[j] << " ";
		}
		//cout << endl;
	}

	//计算win1和win2的相关系数并返回
	double corr2 = 0;
	double Amean2 = 0;
	double Bmean2 = 0;
	for (int m = 0; m < win1.rows; m++) {
		uchar* dataA = win1.ptr<uchar>(m);
		uchar* dataB = win2.ptr<uchar>(m);
		for (int n = 0; n < win1.cols; n++) {
			Amean2 = Amean2 + dataA[n];
			Bmean2 = Bmean2 + dataB[n];
		}
	}
	Amean2 = Amean2 / (win1.rows * win1.cols);
	Bmean2 = Bmean2 / (win2.rows * win2.cols);
	double Cov = 0;
	double Astd = 0;
	double Bstd = 0;
	for (int m = 0; m < win1.rows; m++) {
		uchar* dataA = win1.ptr<uchar>(m);
		uchar* dataB = win2.ptr<uchar>(m);
		for (int n = 0; n < win1.cols; n++) {
			//协方差
			Cov = Cov + (dataA[n] - Amean2) * (dataB[n] - Bmean2);
			//A的方差
			Astd = Astd + (dataA[n] - Amean2) * (dataA[n] - Amean2);
			//B的方差
			Bstd = Bstd + (dataB[n] - Bmean2) * (dataB[n] - Bmean2);
		}
	}
	corr2 = Cov / (sqrt(Astd * Bstd));
	return corr2;
	//https://blog.csdn.net/u013162930/article/details/50887019
};

double CalCorr(int LengthOfWin, Mat img1, Mat img2, ImgPoint P1, ImgPoint P2, float subpixelLength){
	//若出边界，则暂时舍弃该点，之后再考虑减小窗口等
	if (P1.sample - (LengthOfWin - 1) / 2 < 0 || P1.sample + (LengthOfWin - 1) / 2 > img1.cols
		|| P1.line - (LengthOfWin - 1) / 2 < 0 || P1.line + (LengthOfWin - 1) / 2 > img1.rows)
	{
		return 0;
	}

	//定义两个小窗口Mat
	cv::Mat win1(LengthOfWin, LengthOfWin, CV_8UC1); win1.setTo(0);
	cv::Mat win2(LengthOfWin, LengthOfWin, CV_8UC1); win2.setTo(0);

	//给win1和win2赋值
	for (int i = 0; i <LengthOfWin; i++)
	{
		uchar* p_win1 = win1.ptr<uchar>(i);
		uchar* p_win2 = win2.ptr<uchar>(i); //指针用于赋值

		for (int j = 0; j <= LengthOfWin; j++)
		{
			p_win1[j] = get_origin_value(img1, subpixelLength*i + P1.sample - subpixelLength*(LengthOfWin - 1) / 2, subpixelLength*j + P1.line - subpixelLength*(LengthOfWin - 1) / 2);
			p_win2[j] = get_origin_value(img2, subpixelLength*i + P2.sample - subpixelLength*(LengthOfWin - 1) / 2, subpixelLength*j + P2.line - subpixelLength*(LengthOfWin - 1) / 2);
			//测试
			//cout << (int)p_win1[j] << " ";
		}
		//cout << endl;
	}

	/*对比测试是否子像素
	cout << "-------------------------------" << endl;
	for (int i = 0; i <LengthOfWin; i++)
	{
	uchar* p_win1 = win1.ptr<uchar>(i);
	uchar* p_win2 = win2.ptr<uchar>(i); //指针用于赋值

	for (int j = 0; j <= LengthOfWin; j++)
	{
	p_win1[j] = get_origin_value(img1, i + P1.sample - (LengthOfWin - 1) / 2, j + P1.line - (LengthOfWin - 1) / 2);
	p_win2[j] = get_origin_value(img2, i + P2.sample - (LengthOfWin - 1) / 2, j + P2.line - (LengthOfWin - 1) / 2);
	//测试
	cout << (int)p_win1[j] << " ";
	}
	cout << endl;
	}

	cout << "-------------------------------" << endl;
	*/

	//计算win1和win2的相关系数并返回
	double corr2 = 0;
	double Amean2 = 0;
	double Bmean2 = 0;
	for (int m = 0; m < win1.rows; m++) {
		uchar* dataA = win1.ptr<uchar>(m);
		uchar* dataB = win2.ptr<uchar>(m);
		for (int n = 0; n < win1.cols; n++) {
			Amean2 = Amean2 + dataA[n];
			Bmean2 = Bmean2 + dataB[n];
		}
	}
	Amean2 = Amean2 / (win1.rows * win1.cols);
	Bmean2 = Bmean2 / (win2.rows * win2.cols);
	double Cov = 0;
	double Astd = 0;
	double Bstd = 0;
	for (int m = 0; m < win1.rows; m++) {
		uchar* dataA = win1.ptr<uchar>(m);
		uchar* dataB = win2.ptr<uchar>(m);
		for (int n = 0; n < win1.cols; n++) {
			//协方差
			Cov = Cov + (dataA[n] - Amean2) * (dataB[n] - Bmean2);
			//A的方差
			Astd = Astd + (dataA[n] - Amean2) * (dataA[n] - Amean2);
			//B的方差
			Bstd = Bstd + (dataB[n] - Bmean2) * (dataB[n] - Bmean2);
		}
	}
	corr2 = Cov / (sqrt(Astd * Bstd));
	return corr2;
	//https://blog.csdn.net/u013162930/article/details/50887019
};

void WriteArguPath(ofstream &writeArguPath, int sufferZ, int largePenalty, int smallPenalty, int N_times_res, int corrWin, int NumOfImgs){
	if (writeArguPath){
		writeArguPath << "sufferZ: " << sufferZ << endl <<
			"largePenalty: " << largePenalty << endl <<
			"smallPenalty: " << smallPenalty << endl <<
			"细化倍数 N_times_res: " << N_times_res << endl <<
			"相关系数窗口：" << corrWin << endl <<
			NumOfImgs << "张影像" << endl <<
			" z搜索间距: " << Z_resolution << "米" <<
			endl << "备注： 四片影像细化倍数为3倍，roughDem为imread(F:/A_工作笔记/开题/论文/已有工作/zhinvRoughDem.tif, -1);" << endl;
	}
}

int main(){
	clock_t startTime, endTime;
	startTime = clock();

	//------------------------------------------------------------------------------------0. 定义参数并记录
	float sufferZ = Z_resolution;
	int corrWin = 45;
	int largePenalty = 16;
	int smallPenalty = 2;
	int N_times_res = 3; //细化倍数 _ 

	string dirName = "G:\\A_daily\\0719\\test2";  //创建test文件夹
	bool flag = CreateDirectory(dirName.c_str(), NULL);
	if (flag == false)
	{
		cout << "创建文件夹失败" << endl;
		system("pause");
		return 0;
	}

	string path = dirName + "\\points3d.txt"; //3D点路径及名称
	string arguPath = dirName + "\\parameter.txt"; //参数路径
	string costPath = dirName + "\\cost.txt"; //参数路径
	string initialDemPath = dirName + "\\initialDem.txt";
	ofstream writeArguPath(arguPath);
	ofstream Cost(costPath);

	//------------------------------------------------------------------------------------1. sldem取细格网并膨胀腐蚀自适应高程范围
	//Mat roughDem = imread("F:/A_工作笔记/computers_and_geosciences/roughSLDEM/ce3_1.tif", -1);
	//double UL_lon = -19.692114111, UL_lat = 45.5775044989343;
	//double rough_lat_res = 0.0019531249, rough_lon_res = 0.0019531249;

	//Mat roughDem = imread("F:/A_工作笔记/computers_and_geosciences/test/ap11Small.tif", -1);
	//double UL_lon = 23.4840485930001, UL_lat = 1.34888891397407;
	//double rough_lat_res = 0.0019531249, rough_lon_res = 0.0019531249;



	//Mat roughDem = imread("F:/A_工作笔记/开题/实验/sldemFilter/Float_roughd11.tif", -1);
	//double UL_lon = 176.012553117, UL_lat = -45.0620845950352;
	//double rough_lat_res = 0.0019531249, rough_lon_res = 0.0019531249;

	//Mat roughDem = imread("F:/A_工作笔记/computers_and_geosciences/test/ap11RoughDem.tif", -1);
	//double UL_lon = 23.4769905540001, UL_lat = 1.36744589405889;
	//double rough_lat_res = 0.0019531249, rough_lon_res = 0.0019531249;

	//sldem Nac
	Mat roughDem = imread("G:/A_工作笔记/computers_and_geosciences/roughSLDEM/sldemNacAp1.tif", -1);
	double UL_lon = 23.3722757552958, UL_lat = 1.23648399230611;
	double rough_lat_res = 0.0019531249, rough_lon_res = 0.0019531249;

	// 预处理，对sldem进行膨胀和腐蚀，自适应P3D的搜索范围，即得到每个待求格网的搜索最高点以及最低点
	Mat dilated_up_dem;
	Mat element_dilated = getStructuringElement(MORPH_RECT, Size(3, 3));
	dilate(roughDem, dilated_up_dem, element_dilated);

	//查看膨胀高程最大值

	double max, min;
	cv::Point min_loc, max_loc;
	cv::minMaxLoc(dilated_up_dem, &min, &max, &min_loc, &max_loc);

	Mat erode_down_dem;
	Mat element_erode = getStructuringElement(MORPH_RECT, Size(3, 3));
	erode(roughDem, erode_down_dem, element_erode);

	//查看腐蚀高程最小值

	double maxErode, minErode;
	cv::Point min_erode_loc, max_erode_loc;
	cv::minMaxLoc(erode_down_dem, &minErode, &maxErode, &min_erode_loc, &max_erode_loc);

	int rough_rows = roughDem.rows, rough_cols = roughDem.cols;
	int detail_rows = rough_rows*N_times_res;
	int detail_cols = rough_cols*N_times_res;

	vector<P3D> P3Ds; P3D tmpP3D;
	double tmp_lat, tmp_lon; float tmp_z;
	float tmp_upHei, tmp_downHei;
	ofstream writeinitialDemPath(initialDemPath);
	int lat_ind, lon_ind;
	for (double lat = UL_lat; lat > UL_lat - rough_rows*rough_lat_res; lat -= rough_lat_res){
		lat_ind = (UL_lat - lat) / rough_lat_res;
		for (double lon = UL_lon; lon < UL_lon + rough_cols*rough_lon_res; lon += rough_lon_res)	{
			lon_ind = (lon - UL_lon) / rough_lat_res;
			float* p = roughDem.ptr<float>(lat_ind);
			tmp_z = p[lon_ind]; //roughDem行列处的高程
			float* p_upHei = dilated_up_dem.ptr<float>(lat_ind);
			float* p_downHei = erode_down_dem.ptr<float>(lat_ind);

			tmp_upHei = p_upHei[lon_ind] + Z_resolution;
			tmp_downHei = p_downHei[lon_ind] - Z_resolution;

			if (abs(tmp_z) >= 100000){ tmp_z = 0; };
			//if (abs(tmp_upHei) >= 100000 || abs(tmp_downHei) >= 100000){ tmp_upHei = roughZmean + Z_resolution; tmp_downHei = roughZmean - Z_resolution; };
			if (abs(tmp_upHei) >= 100000 || abs(tmp_downHei) >= 100000){ tmp_upHei = 0; tmp_downHei = 0; };


			//一个格网细化为9个格网，形成第一层金字塔
			for (int i = 0; i <= (N_times_res - 1); i++){
				for (int j = 0; j <= (N_times_res - 1); j++){
					tmp_lat = lat - i*rough_lat_res / N_times_res;
					tmp_lon = lon + j*rough_lat_res / N_times_res;
					tmpP3D.lat = tmp_lat;
					tmpP3D.lon = tmp_lon;
					tmpP3D.z = tmp_z; //即为精细格网点的初值，应在该z值上下一定范围内进行遍历，如temp_zvalue-40*deltaz:temp_zvalue+40*deltaz
					tmpP3D.downHei = tmp_downHei;
					tmpP3D.upHei = tmp_upHei;
					P3Ds.push_back(tmpP3D);
					/*
					//if (writeinitialDemPath){
					//	writeinitialDemPath << fixed << setprecision(17) << tmp_lon << " " << fixed << setprecision(17) << tmp_lat << " "
					//		<< fixed << setprecision(17) << tmp_z <<" "<<tmp_downHei<<" "<<tmp_upHei << endl;
					//}
					*/
				}
			}
		}
	}
	//------------------------------------------------------------------------------------2. 输入影像以及rpc，并开辟内存，用于存储C，S和A以及自适应的高程范围

	//输入影像以及rpc
	vector<Mat> imgs;
	vector<RPCMODEL> rpcs;
	RPCMODEL rpc1, rpc2, rpc3, rpc4, rpc5, rpc6, rpc7;
	//Mat img1 = imread("F:/A_工作笔记/computers_and_geosciences/data/ce3/m1144929211le.tif", 0);
	//Mat img2 = imread("F:/A_工作笔记/computers_and_geosciences/data/ce3/m1144936321le.tif", 0);
	//Mat img3 = imread("F:/A_工作笔记/computers_and_geosciences/data/ce3/m1144943432le.tif", 0);
	//API_GetRPCMODEL("F:/A_工作笔记/computers_and_geosciences/data/ce3/m1144929211le_rpc.txt", rpc1);
	//API_GetRPCMODEL("F:/A_工作笔记/computers_and_geosciences/data/ce3/m1144936321le_rpc.txt", rpc2);
	//API_GetRPCMODEL("F:/A_工作笔记/computers_and_geosciences/data/ce3/m1144943432le_rpc.txt", rpc3);

	//Mat img1 = imread("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1114021499re.tif", 0);
	//Mat img2 = imread("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1126986303re.tif", 0);
	//Mat img3 = imread("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1129340193re.tif", 0);
	//API_GetRPCMODEL("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1114021499re_rpc.txt", rpc1);
	//API_GetRPCMODEL("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1126986303re_rpc.txt", rpc2);
	//API_GetRPCMODEL("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1129340193re_rpc.txt", rpc3);

	//Mat img1 = imread("F:/MVLL/MVLL/data/baijie_tif_rpc/M184732140LE.tif", 0);
	//Mat img2 = imread("F:/MVLL/MVLL/data/baijie_tif_rpc/M184724991LE.tif", 0);
	//Mat img3 = imread("F:/MVLL/MVLL/data/baijie_tif_rpc/M184732140RE.tif", 0);
	//Mat img4 = imread("F:/MVLL/MVLL/data/baijie_tif_rpc/M184724991RE.tif", 0);
	//API_GetRPCMODEL("F:/bajie/data/M184732140le_rpc.txt", rpc1);
	//API_GetRPCMODEL("F:/bajie/data/M184724991le_rpc.txt", rpc2);
	//API_GetRPCMODEL("F:/bajie/data/M184732140re_rpc.txt", rpc3);
	//API_GetRPCMODEL("F:/bajie/data/M184724991re_rpc.txt", rpc4);

	//Mat img1 = imread("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1114021499re.tif", 0);
	//Mat img2 = imread("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1126986303re.tif", 0);
	//Mat img3 = imread("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1129340193re.tif", 0);
	//API_GetRPCMODEL("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1114021499re_rpc.txt", rpc1);
	//API_GetRPCMODEL("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1126986303re_rpc.txt", rpc2);
	//API_GetRPCMODEL("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1129340193re_rpc.txt", rpc3);

	//sldem Nac
	//Mat img1 = imread("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1114007294re.tif", 0);
	//Mat img2 = imread("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1114014396re.tif", 0);
	//Mat img3 = imread("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1114021499re.tif", 0);
	//API_GetRPCMODEL("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1114007294re_rpc.txt", rpc1);
	//API_GetRPCMODEL("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1114014396re_rpc.txt", rpc2);
	//API_GetRPCMODEL("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1114021499re_rpc.txt", rpc3);

	//sldem Nac和定位保持一致的两片与三片
	Mat img1 = imread("G:/A_工作笔记/computers_and_geosciences/test/ap11/m1126972080le.tif", 0);
	Mat img2 = imread("G:/A_工作笔记/computers_and_geosciences/test/ap11/m1114021499re.tif", 0);
	//Mat img3 = imread("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1126986303re.tif", 0);
	//Mat img4 = imread("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1114007294re.tif", 0);
	//Mat img5 = imread("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1129340193re.tif", 0);
	//Mat img3 = imread("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1114014396re.tif", 0);
	//Mat img7 = imread("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1159956344re.tif", 0);



	API_GetRPCMODEL("G:/A_工作笔记/computers_and_geosciences/test/ap11/m1126972080le_rpc.txt", rpc1);
	API_GetRPCMODEL("G:/A_工作笔记/computers_and_geosciences/test/ap11/m1114021499re_rpc.txt", rpc2);
	//API_GetRPCMODEL("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1126986303re_rpc.txt", rpc3);
	//API_GetRPCMODEL("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1114007294re_rpc.txt", rpc4);
	//API_GetRPCMODEL("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1129340193re_rpc.txt", rpc5);
	//API_GetRPCMODEL("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1114014396re_rpc.txt", rpc3);
	//API_GetRPCMODEL("F:/A_工作笔记/computers_and_geosciences/test/ap11/m1159956344re_rpc.txt", rpc7);


	imgs.push_back(img1); imgs.push_back(img2);
	rpcs.push_back(rpc1); rpcs.push_back(rpc2);
	//imgs.push_back(img3); rpcs.push_back(rpc3);
	//imgs.push_back(img4); rpcs.push_back(rpc4);
	//imgs.push_back(img5); rpcs.push_back(rpc5);
	//imgs.push_back(img6); rpcs.push_back(rpc6);
	//imgs.push_back(img7); rpcs.push_back(rpc7);

	int NumOfImgs = imgs.size();
	WriteArguPath(writeArguPath, sufferZ, largePenalty, smallPenalty, N_times_res, corrWin, NumOfImgs);

	//3. 分配缓冲区
	float ***C;  // pixel cost array W x H x D
	float ***S;  // aggregated cost array W x H x D
	float **TmpThisZ_max;   //存储每一个平面格网的搜索的Z_max和Z_min,理应和p3d相对应
	float **TmpThisZ_min;   //存储每一个平面格网的搜索的Z_max和Z_min
	float  ****A; // single path cost array 2 x W x H x D

	C = new float **[detail_rows];
	S = new float **[detail_rows];
	TmpThisZ_max = new float *[detail_rows];
	TmpThisZ_min = new float *[detail_rows];

	//int row = detail_rows+1 ;
	//int k = row / N_times_res-1 ;
	//float* p_dilated = dilated_up_dem.ptr<float>(k);
	for (int row = 0; row < detail_rows; ++row)
	{

		C[row] = new float *[detail_cols]();
		S[row] = new float *[detail_cols]();
		TmpThisZ_max[row] = new float[detail_cols]();
		TmpThisZ_min[row] = new float[detail_cols]();

		for (int col = 0; col < detail_cols; ++col)
		{

			float* p_dilated; float* p_erode;
			p_dilated = dilated_up_dem.ptr<float>(row / N_times_res);
			p_erode = erode_down_dem.ptr<float>(row / N_times_res);

			float ThisZ_max, ThisZ_min;

			ThisZ_max = p_dilated[(col / N_times_res)] + Z_resolution;
			ThisZ_min = p_erode[(col / N_times_res)] - Z_resolution;

			//if (abs(ThisZ_max) >= 100000 || (abs(ThisZ_max) >= 100000)){ ThisZ_max = roughZmean + Z_resolution; ThisZ_min = roughZmean - Z_resolution; }
			if ((abs(ThisZ_max) >= 100000) || ((abs(ThisZ_min) >= 100000))){ ThisZ_max = 0; ThisZ_min = 0; }

			TmpThisZ_min[row][col] = ThisZ_min;
			TmpThisZ_max[row][col] = ThisZ_max;

			int adjustThisZ = (ThisZ_max - ThisZ_min) / Z_resolution + 1;
			C[row][col] = new float[adjustThisZ]();
			S[row][col] = new float[adjustThisZ](); // initialize to 0
		}
	}

	// #define PATHS_PER_SCAN 4
	A = new float  ***[PATHS_PER_SCAN];
	for (int path = 0; path < PATHS_PER_SCAN; ++path)
	{
		A[path] = new float **[detail_rows];
		for (int row = 0; row < detail_rows; ++row)
		{
			A[path][row] = new float *[detail_cols];
			for (int col = 0; col < detail_cols; ++col)
			{
				float* p_dilated; float* p_erode;

				p_dilated = dilated_up_dem.ptr<float>(row / N_times_res);
				p_erode = erode_down_dem.ptr<float>(row / N_times_res);


				float ThisZ_max, ThisZ_min;

				ThisZ_max = p_dilated[(col / N_times_res)] + Z_resolution;
				ThisZ_min = p_erode[(col / N_times_res)] - Z_resolution;

				//if (abs(ThisZ_max) >= 100000 || (abs(ThisZ_max) <= 100000)){ ThisZ_max = roughZmean + Z_resolution; ThisZ_min = roughZmean - Z_resolution; }
				if (abs(ThisZ_max) >= 100000 || (abs(ThisZ_min) >= 100000)){ ThisZ_max = 0; ThisZ_min = 0; }

				int adjustThisZ = (ThisZ_max - ThisZ_min) / Z_resolution + 1;

				A[path][row][col] = new float[adjustThisZ];
				for (unsigned int d = 0; d < adjustThisZ; ++d)
				{
					A[path][row][col][d] = 0;
				}
			}
		}
	}


	//------------------------------------------------------------------------------------3. calculate the cost of every voxel，像方子像素相关系数窗口

	double X, Y, Z;
	double Sample, Line, BaseSample, BaseLine;
	ImgPoint Pbase;
	vector<ImgPoint> P(NumOfImgs);
	double TempSample, TempLine;
	int lastProgressPrinted = 0;
	int highCorrNum = 0;
	int trashcount = 0;
	for (int i = 0; i < P3Ds.size(); i++){
		P3D tmpP3D;
		X = P3Ds[i].lon;
		tmpP3D.lon = X;
		//if (X < min_x){ min_x = X; }
		int X_ind = (X - UL_lon) / rough_lat_res * N_times_res;
		if (X_ind >= detail_cols){ trashcount += 1; continue; }
		Y = P3Ds[i].lat;
		tmpP3D.lat = Y;

		//if (Y < min_y){ min_y = Y; }
		int Y_ind = (UL_lat - Y) / rough_lat_res * N_times_res;
		if (Y_ind >= detail_rows){ trashcount += 1; continue; }
		float Z_init = P3Ds[i].downHei;
		float Z_max = P3Ds[i].upHei;
		if (Z_max - Z_init == 0){ continue; }
		vector<double> AllZValueOfThisXY((Z_max - Z_init) / Z_resolution + 1);
		vector<double> AllCorrOfThisXY((Z_max - Z_init) / Z_resolution + 1);
		for (float Z = Z_init; Z <= Z_max; Z += Z_resolution){ //是否给定高程初始范围的实验
			tmpP3D.z = Z;
			//for (float Z = ThisCubioInfo.Z_min; Z <= ThisCubioInfo.Z_max; Z += Z_resolution){
			int Z_ind = (Z - Z_init) / Z_resolution;
			double SumCorrOfThisZ = 0;
			double AveCorrOfThisZ = 0;
			//double maxCorr = -1; //初始化为相关系数最小值
			double avgCorr = 0; //初始化为0,第二个选择，选择平均相关系数
			//1. 对多幅影像进行遍历，看有几张影像上有候选同名窗口
			int TempInNum = 0;
			for (int i = 0; i<NumOfImgs; i++){
				API_LATLONGHEIFHT2LineSample(rpcs[i], Y, X, Z, TempSample, TempLine);
				if (TempSample - ceil(corrWin / 2)>0 && TempSample + ceil(corrWin / 2)< img1.cols && TempLine - ceil(corrWin / 2)>0 && TempLine + ceil(corrWin / 2) < img1.rows){
					//记录内点影像+1，以及记录相应内点信息
					TempInNum += 1;
					P[i].line = TempLine;
					P[i].sample = TempSample;
				}
				else{
					//否则，将点置为-1
					P[i].line = -1;
					P[i].sample = -1;
				}
			}
			//2. 若有两度重叠以上，则选定基准影像计算相关系数，否则舍弃该高程点
			int baseIndex;
			if (TempInNum >= 2){
				//2.1 遍历，按顺序选定base影像
				for (int i = 0; i<NumOfImgs; i++){
					if (P[i].sample - ceil(corrWin / 2)>0 && P[i].sample + ceil(corrWin / 2)< img1.cols && P[i].line - ceil(corrWin / 2)>0 && P[i].line + ceil(corrWin / 2) < img1.rows){
						Pbase.sample = P[i].sample;
						Pbase.line = P[i].line;
						baseIndex = i;
						break;
					}
				}
				//2.2 选定base后，计算其与其他影像之间的相关系数之和，并求平均或最大值。

				for (int i = baseIndex + 1; i < NumOfImgs; i++){
					if (P[i].sample - ceil(corrWin / 2)>0 && P[i].sample + ceil(corrWin / 2)< img1.cols && P[i].line - ceil(corrWin / 2)>0 && P[i].line + ceil(corrWin / 2) < img1.rows){
						//double tempCorr = groundCalCorr(corrWin, tmpP3D, rough_lat_res, UL_lon, UL_lat, imgs[baseIndex], imgs[i], roughDem, rpcs, baseIndex, i);

						double tempCorr = CalCorr(corrWin, imgs[baseIndex], imgs[i], Pbase, P[i], 0.3);
						//double tempCorr = (tempCorr1 + tempCorr2) / 2;
						avgCorr += tempCorr;
						//if (maxCorr <= tempCorr){
						//	maxCorr = tempCorr;
						//	//}
						//}
					}
				}

				avgCorr = avgCorr / (NumOfImgs - 1);
				AllZValueOfThisXY[Z_ind] = Z;
				AllCorrOfThisXY[Z_ind] = avgCorr; //可以选择avgCorr或者maxCorr，即可以另外选择AllCorrOfThisXY[Z_ind] = maxCorr; 
				if (avgCorr >= 0.7){ highCorrNum++; }

				C[Y_ind][X_ind][Z_ind] = 1 - avgCorr; //即可以另外选择C[Y_ind][X_ind][Z_ind] = 1 - maxCorr;
				//AllCorrOfThisXY[Z_ind] = maxCorr; //可以选择avgCorr或者maxCorr，即可以另外选择AllCorrOfThisXY[Z_ind] = maxCorr; 
				//C[Y_ind][X_ind][Z_ind] = 1 - maxCorr; //即可以另外选择C[Y_ind][X_ind][Z_ind] = 1 - maxCorr;
				//if (maxCorr >= 0.7){ highCorrNum++; }

				if (Cost){
					Cost << fixed << setprecision(17) << X << " " << X_ind << " " << fixed << setprecision(17) << Y << " " << Y_ind << " " << Z << " " << Z_ind << " "
						<< fixed << setprecision(17) << C[Y_ind][X_ind][Z_ind] << endl;
				}
			}
		}
	}

		cout << highCorrNum << endl;

		//------------------------------------------------------------------------------------4. 自适应高程范围内代价聚集
		aggregateCosts(detail_rows, detail_cols, TmpThisZ_max, TmpThisZ_min, C, A, S, sufferZ, largePenalty, smallPenalty);

		//------------------------------------------------------------------------------------5. 样条函数插值，取0.1米高程精度，写入三维点txt

		// 通过聚集的代价中搜索得到最佳的高程，并写入txt文件
		float bestZ = 0, minCost;
		ofstream Points3D(path);
		vector<float> Delta;
		vector<P3D> OSGMP3Ds; //用来存储计算出来的三维坐标
		P3D tmpOSGMP3D;
		for (int i = 0; i < P3Ds.size(); i++){
			minCost = FLT_MAX;
			X = P3Ds[i].lon;
			int X_ind = (X - UL_lon) / rough_lat_res * N_times_res;
			if (X_ind >= detail_cols){ continue; };
			Y = P3Ds[i].lat;
			int Y_ind = (UL_lat - Y) / rough_lat_res * N_times_res;
			if (Y_ind >= detail_rows){ continue; };
			float Z_init = P3Ds[i].downHei;
			float Z_max = P3Ds[i].upHei;
			if (Z_max - Z_init == 0){ continue; }

			int ThisZHeightRange = ((Z_max - Z_init) / Z_resolution + 1);
			std::vector<double> input_x(ThisZHeightRange), input_y(ThisZHeightRange);
			for (float Z = Z_init; Z <= Z_max; Z += Z_resolution){
				int Z_ind = (Z - Z_init) / Z_resolution;
				//将高程以及代价，分别作为x和y vector输入函数求解样条函数，再求解代价最小的y所对应的x，即高程值。
				//第二种方式拟合样条函数，求0.1米间隔的局部最小值
				input_x[Z_ind] = Z;
				input_y[Z_ind] = S[Y_ind][X_ind][Z_ind];
			}

			CubicSplineCoeffs *cubicCoeffs;
			CubicSplineInterpolation cubicSpline;
			cubicSpline.calCubicSplineCoeffs(input_x, input_y, cubicCoeffs, CUBIC_NATURAL, CUBIC_WITHOUT_FILTER);
			std::vector<double> output_x, output_y;
			cubicSpline.cubicSplineInterpolation(cubicCoeffs, input_x, output_x, output_y, 0.1);

			// --------------------------找到最小和次小的cost值，计算其比值，称之为显著性比值。

			//找到output_y的最小值的位置，该位置的outputx即为0.1米精度的高程值，设为bestZ。
			vector<double>::iterator smallest = min_element(begin(output_y), end(output_y));
			int position = distance(begin(output_y), smallest);
			float smallestScost, secondSmallScost, signifiRatio; // 定义最小Scost值以及次小Cost值，以及其比值signifiRatio
			smallestScost = output_y[position]; //暂时定义最小值

			//删除掉outputy中的最小值，再求其最小值即为次小值
			output_y.erase(smallest);
			vector<double>::iterator secondSmallest = min_element(begin(output_y), end(output_y));
			int secondPosition = distance(begin(output_y), smallest);
			secondSmallScost = output_y[secondPosition];
			signifiRatio = secondSmallScost / smallestScost;
			cout << signifiRatio << endl;
			if (signifiRatio >= 1.2) {
				tmpOSGMP3D.lat = P3Ds[i].lat;
				tmpOSGMP3D.lon = P3Ds[i].lon;
				bestZ = output_x[position];
				tmpOSGMP3D.z = bestZ;
				OSGMP3Ds.push_back(tmpOSGMP3D);
				int tmpDelta = bestZ - P3Ds[i].z;
				Delta.push_back(tmpDelta);
			}
		}

		//与SLDEM初值相较，三倍中误差以外视为粗差
		//（1）计算Delta的中误差
		double sum = std::accumulate(std::begin(Delta), std::end(Delta), 0.0);
		double mean = sum / Delta.size(); //均值
		double accum = 0.0;
		std::for_each(std::begin(Delta), std::end(Delta), [&](const double d) {accum += (d - mean)*(d - mean); });
		double stdev = sqrt(accum / (Delta.size() - 1)); //高差方差
		//（2）遍历三维点，若小于等于三倍中误差，则输出到 Points3D的txt中。
		float tolerance = 3 * stdev;
		for (int i = 0; i < OSGMP3Ds.size(); i++){

			if (abs(Delta[i]) > tolerance){ continue; }


			if (Points3D){
				Points3D << fixed << setprecision(17) << OSGMP3Ds[i].lon << " " << fixed << setprecision(17) << \
					OSGMP3Ds[i].lat << " " << fixed << setprecision(17) << OSGMP3Ds[i].z << endl;
			}
		}

		//------------------------------------------------------------------------------------6. 输出整体时间	
		endTime = clock();
		cout << "Totle Time : " << (double)(endTime - startTime) / (CLOCKS_PER_SEC * 60) << "min";
		if (writeArguPath){
			writeArguPath << endl << "耗时" << (double)(endTime - startTime) / (CLOCKS_PER_SEC * 60) << "min" << endl;
			writeArguPath << "共" << P3Ds.size() << "个点。 " <<
				"相关系数大于0.6的点数有" << highCorrNum << "个点" << endl;
		}

		system("pause");

		return 0;
	}
