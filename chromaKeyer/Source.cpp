/*
 *some info
 */

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>

using namespace cv;
using namespace std;

Mat frame, frameHSV, frameSpillSuppress, background, mask1, blurMask1, out;
Vec3b chromaColorLOW(180, 255, 255), chromaColorHIGH(0, 0, 0);
int width, height, blurMaskVal, adjustSpillVal;

//Points to store the two corners of the color selection rectangle
Point p1, p2;

bool mousePressed;

void selectChroma(int action, int x, int y, int flags, void* userdata);
void thresholdHUE(int, void*);
void thresholdSAT(int, void*);
void thresholdVAL(int, void*);
void softenMask(int, void*);
void maskOperations();
void adjustSpillSuppression(int, void*);
void spillSuppression();

string windowName = "Frame";
int trackBarCount = 5;
int hueThreshold = 1;
int hueThresholdPrev = 0;
int satThreshold = 1;
int satThresholdPrev = 0;
int valThreshold = 1;
int valThresholdPrev = 0;
int maxThreshold = 40;
int soften = 1;
int maxSoften = 20;
int spillVal = 0;
int maxSpillVal = 100;


int main() {
	
	string filename = "./greenscreen-demo.mp4";

	VideoCapture cap(filename);

	if (!cap.isOpened()) {
		cout << "Couldn't open video file." << endl;
		return -1;
	}

	cap >> frame;
	width = frame.cols/2;
	height = frame.rows/2;
	background = frame.clone();
	background.setTo(Scalar(255, 0, 255));
	mask1 = Mat::zeros(Size(frame.rows, frame.cols), CV_8UC1);
	blurMaskVal = 1;
	adjustSpillVal = 0;
	


	namedWindow("Frame", WINDOW_NORMAL);
	namedWindow("Mask", WINDOW_NORMAL);
	resizeWindow("Frame", width, height + trackBarCount * 60);
	resizeWindow("Mask", width, height);


	createTrackbar("hue", windowName, &hueThreshold, maxThreshold, thresholdHUE);
	createTrackbar("sat", windowName, &satThreshold, maxThreshold, thresholdSAT);
	createTrackbar("val", windowName, &valThreshold, maxThreshold, thresholdVAL);
	createTrackbar("soften", windowName, &soften, maxSoften, softenMask);
	createTrackbar("spill", windowName, &spillVal, maxSpillVal, adjustSpillSuppression);

	out = frame.clone();

	setMouseCallback("Frame", selectChroma, NULL);

	cout << "press > to step forward" << endl;
	cout << "press r to reset mask" << endl;
	cout << "press esc to exit" << endl;

	cvtColor(frame, frameHSV, COLOR_BGR2HSV);
	frameSpillSuppress = frameHSV.clone();
	blurMask1 = mask1.clone();

	while (1) {

		if (frame.empty())
			break;
		
		inRange(frameHSV, chromaColorLOW, chromaColorHIGH, mask1);

		spillSuppression();
		maskOperations();
		imshow("Frame", out);
		
		char c = (char)waitKey(25);
		if (c == 'r') {
			chromaColorLOW[0] = 180;
			chromaColorLOW[1] = 255;
			chromaColorLOW[2] = 255;
			chromaColorHIGH[0] = 0;
			chromaColorHIGH[1] = 0;
			chromaColorHIGH[2] = 0;
			cvtColor(frame, frameHSV, COLOR_BGR2HSV);
			frameSpillSuppress = frameHSV.clone();
			blurMask1 = mask1.clone();
		}
		if (c == '>')
			cap >> frame;
		if (c == 27)
			break;
	}

	destroyAllWindows();

	return 0;
}

void selectChroma(int action, int x, int y, int flags, void* userdata) {
	Mat temp = frame.clone();
	if (x < 0)
		x = 0;
	if (x >= frame.cols)
		x = frame.cols - 1;
	if (y < 0)
		y = 0;
	if (y >= frame.rows)
		y = frame.rows - 1;

	if (action == EVENT_LBUTTONDOWN)
	{
		mousePressed = true;

		p1 = Point(x, y);
	}
	else if (action == EVENT_MOUSEMOVE)
	{
		if (mousePressed == true)
		{
			p2 = Point(x, y);
			rectangle(out, p1, p2, Scalar(255, 255, 0), 2, LINE_AA);
			imshow("Frame", out);
		}
	}
	else if (action == EVENT_LBUTTONUP)
	{
		mousePressed = false;
		p2 = Point(x, y);

		//create a patch
		Mat patch = frameHSV(Rect(p1, p2));
		vector<Mat> splitHSV;
		split(patch, splitHSV);
		
		//step through every pixel of the patch
		for (int i = 0; i < patch.rows; i++) {
			for (int j = 0; j < patch.cols; j++) {
				if (splitHSV[0].at<uchar>(i, j) < chromaColorLOW[0])
					chromaColorLOW[0] = splitHSV[0].at<uchar>(i, j);
				else if (splitHSV[0].at<uchar>(i, j) > chromaColorHIGH[0])
					chromaColorHIGH[0] = splitHSV[0].at<uchar>(i, j);
				if (splitHSV[1].at<uchar>(i, j) < chromaColorLOW[1])
					chromaColorLOW[1] = splitHSV[1].at<uchar>(i, j);
				else if (splitHSV[1].at<uchar>(i, j) > chromaColorHIGH[1])
					chromaColorHIGH[1] = splitHSV[1].at<uchar>(i, j);
				if (splitHSV[2].at<uchar>(i, j) < chromaColorLOW[2])
					chromaColorLOW[2] = splitHSV[2].at<uchar>(i, j);
				else if (splitHSV[2].at<uchar>(i, j) > chromaColorHIGH[2])
					chromaColorHIGH[2] = splitHSV[2].at<uchar>(i, j);
			}
		}
		
	}
	frame = temp.clone();
}

void thresholdHUE(int, void*) {
	if (hueThreshold > hueThresholdPrev) {
		if (chromaColorLOW[0] - hueThreshold > 0)
			chromaColorLOW[0] = chromaColorLOW[0] - hueThreshold;
		else
			chromaColorLOW[0] = 0;

		if (chromaColorHIGH[0] + hueThreshold < 181)
			chromaColorHIGH[0] = chromaColorHIGH[0] + hueThreshold;
		else
			chromaColorHIGH[0] = 180;

	}
	if (hueThreshold < hueThresholdPrev) {
		chromaColorLOW[0] = chromaColorLOW[0] + hueThreshold;

	}	

	hueThresholdPrev = hueThreshold;
}

void thresholdSAT(int, void*) {
	if (satThreshold > satThresholdPrev) {
		if (chromaColorLOW[1] - satThreshold > 0)
			chromaColorLOW[1] = chromaColorLOW[1] - satThreshold;
		else
			chromaColorLOW[1] = 0;

		if (chromaColorHIGH[1] + satThreshold < 256)
			chromaColorHIGH[1] = chromaColorHIGH[1] + satThreshold;
		else
			chromaColorHIGH[1] = 255;

	}
	if (satThreshold < satThresholdPrev) {
		chromaColorLOW[1] = chromaColorLOW[1] + satThreshold;

	}

	satThresholdPrev = satThreshold;
}

void thresholdVAL(int, void*) {
	if (valThreshold > valThresholdPrev) {
		if (chromaColorLOW[2] - valThreshold > 0)
			chromaColorLOW[2] = chromaColorLOW[2] - valThreshold;
		else
			chromaColorLOW[2] = 0;

		if (chromaColorHIGH[2] + valThreshold < 256)
			chromaColorHIGH[2] = chromaColorHIGH[2] + valThreshold;
		else
			chromaColorHIGH[2] = 255;
	}
	if (valThreshold < valThresholdPrev) {
		chromaColorLOW[2] = chromaColorLOW[2] + valThreshold;

	}

	valThresholdPrev = valThreshold;
}

void softenMask(int, void*) {

		blurMaskVal = soften*2 + 1;
}

void maskOperations() {
	GaussianBlur(mask1, blurMask1, Size(blurMaskVal, blurMaskVal), 0, 0);
	//adapted from https://stackoverflow.com/questions/36216702/combining-2-images-with-transparent-mask-in-opencv
	for (int y = 0; y < frame.rows; ++y) {
		for (int x = 0; x < frame.cols; ++x) {
			Vec3b pixelOrig = out.at<Vec3b>(y, x);
			Vec3b pixelBG = background.at<Vec3b>(y, x);
			float blurVal = blurMask1.at<uchar>(y, x) / 255.0f;
			Vec3b pixelOut = blurVal * pixelBG + (1.0f - blurVal) * pixelOrig;

			out.at<Vec3b>(y, x) = pixelOut;
		}
	}
}

void adjustSpillSuppression(int, void*) {
	adjustSpillVal = spillVal;
}

void spillSuppression() {
	
	for (int y = 0; y < frame.rows; ++y) {
		for (int x = 0; x < frame.cols; ++x) {
			if (frameHSV.at<Vec3b>(y, x)[0] > chromaColorLOW[0] &&
				frameHSV.at<Vec3b>(y, x)[0] < chromaColorHIGH[0]) {
				if(frameHSV.at<Vec3b>(y, x)[1] - adjustSpillVal > 0)
					frameSpillSuppress.at<Vec3b>(y, x)[1] = frameHSV.at<Vec3b>(y, x)[1] - adjustSpillVal;
			}
		}
	}
	cvtColor(frameSpillSuppress, out, COLOR_HSV2BGR);
}