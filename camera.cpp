
#include <opencv/cv.h>
#include <opencv/highgui.h>

#include <opencv2/opencv.hpp>

#include <unistd.h>
#include <stdio.h>
#include <vector>
#include <time.h>
#include <string>

#include "sendmail.h"
#include "megacli.h"

#define N_Capture 1 // 1 second
#define AVG_COUNT 3
#define THRESHOLD 0.7

std::string getDateString()
{
    time_t rawtime;
    time(&rawtime);
    struct tm* timeinfo;
    timeinfo = localtime(&rawtime);
    char* date = asctime(timeinfo);
    
    std::string str(date);
    int index = 0;
    while (index < str.length())
    {
        if (str[index] == ' ' || str[index] == ':')
        {
            str[index] = '_';
        }
        index++;
    }
    // Replace the last char to 'y'.
    str[str.length()-1] = 'y';
    return str;
}

double compareImgDiff(const cv::Mat &Ref, const cv::Mat &Test)
{
    cv::Mat m_ref = Ref.clone();
    cv::Mat m_test = Test.clone();
    cv::cvtColor(m_ref, m_ref, CV_BGR2HSV);
    cv::cvtColor(m_test, m_test, CV_BGR2HSV);
    
    
    int h_bins = 50; int s_bins = 60;
    int histSize[] = {h_bins, s_bins};
    float h_range[] = {0, 255};
    float s_range[] = {0, 180};
    const float*  ranges[] = {h_range, s_range};
    int channels[] = {0, 1};
    cv::MatND hist_ref, hist_test;
    
    cv::calcHist(&m_ref, 1, channels, cv::Mat(), hist_ref,  2, histSize, ranges, true, false);
    cv::normalize(hist_ref, hist_ref, 0, 1, cv::NORM_MINMAX, -1, cv::Mat());
    
    cv::calcHist(&m_test, 1, channels, cv::Mat(), hist_test, 2, histSize, ranges, true, false);
    cv::normalize(hist_test, hist_test, 0, 1, cv::NORM_MINMAX, -1, cv::Mat());
    
    double re = cv::compareHist(hist_ref, hist_test, CV_COMP_CORREL);
    return re;
}

double average(const double* vector, unsigned long size) {
    double sum = 0.0;
    for(int i = 0; i < size; i++)
    {
        sum += vector[i];
    }
    
    return sum/size;
}

int main(int argc, char** argv)
{
    CvCapture* pCapture = cvCreateCameraCapture(-1);
    if (!pCapture)
    {
        printf("Cannot find camera, exit...\n");
        return 0;
    }
    
    cv::namedWindow("Camera", CV_WINDOW_NORMAL);
    
    bool isRefImageSet = false;
    std::vector<double> img_diff;
    cv::Mat ref_img;
    
    while (true)
    {
        IplImage* pImage = cvQueryFrame(pCapture);
        if (!pImage)
        {
            sleep(10);
            continue;
        }
        
//        if (isRefImageSet) cv::imshow("Camera", ref_img);
//        cv::waitKey();
        
        if(!isRefImageSet)
        {
            ref_img = cv::Mat(pImage).clone();
            isRefImageSet = true;
        }
        else
        {
            cv::Mat cur_img(pImage);
            const double diff = compareImgDiff(ref_img, cur_img);
            img_diff.push_back(diff);
            if (img_diff.size() > AVG_COUNT)
            {
                img_diff.erase(img_diff.begin());
            }
            
            double diff_average = average(&img_diff[0], img_diff.size());
            
            printf("\tdiff = %f\n", diff);
            if (diff_average < THRESHOLD)
            {
                // There is something happen;
                // Save Image
                std::string filename = getDateString() + std::string(".jpg");
                cv::imwrite(filename.c_str(), cur_img);
                // Send notification mail
                sendmail("future_wei@qq.com", "camera@pi", "Camera notification", "The camera have detected something strange.\n");
                // Upload image to Mega
                loginAndUploadFile("sunnyfuture@gmail.com", "cxw@2623810", filename.c_str());
            }
        }
        
        sleep(N_Capture);
    }
    
    cvReleaseCapture(&pCapture);
}