#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <atomic>
#include <thread>
#include <chrono>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include "MvCameraControl.h"

inline std::atomic_bool cameraRunning = false;
inline std::atomic_bool cameraSetting = false;

inline float exposureTime = 5000;
inline float gain = 0;
inline float frameRate = 10;

struct RawFrame
{
    std::vector<unsigned char> data;
    MV_FRAME_OUT_INFO_EX info;
};

class Camera 
{
    public:
        Camera();
        ~Camera();

        bool init();              // 初始化设备
        bool startGrabbing();
        bool stopGrabbing();
        bool getFrame(RawFrame& frame);
        // bool getFrame(cv::Mat&);  // 获取一帧图像
        bool release();           // 释放设备资源
        bool getInitialized();
        bool getGrabbing();
        void setFloatValue(const char* strKey, float& value);
        void setBoolValue(const char* strKey, bool value);
        float getFloatvalue(const char* strKey);

    private:
        int nRet = MV_OK;
        void* handle = NULL;
        MV_FRAME_OUT_INFO_EX stImageInfo = { 0 };
        unsigned int nPayloadSize = 0;
        unsigned char* pData = NULL;
        std::vector<unsigned char> dataBuffer;
        bool initialized = false;
        bool grabbing = false;
};

void readThread();
void writeThread();

#endif