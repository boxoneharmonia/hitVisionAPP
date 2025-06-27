#include "Camera.hpp"
#include "file.hpp"
#include "global_state.hpp"
using namespace std;
using namespace cv;
namespace fs = std::filesystem;

RawFrame rawFrameA, rawFrameB;
mutex mutexA, mutexB;
atomic_bool bufferAFull = false;
atomic_bool bufferBFull = false;
atomic_bool bufferARefresh = false;

// ch:显示枚举到的设备信息 | en:Print the discovered devices' information
void PrintDeviceInfo(MV_CC_DEVICE_INFO* pstMVDevInfo)
{
    if (NULL == pstMVDevInfo)
    {
        printf("NULL info.\n\n");
        return;
    }

    // 获取图像数据帧仅支持GigE和U3V设备
    if (MV_GIGE_DEVICE == pstMVDevInfo->nTLayerType)
    {
        int nIp1 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
        int nIp2 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
        int nIp3 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
        int nIp4 = (pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);

        // ch:显示IP和设备名 | en:Print current ip and user defined name
        printf("    IP: %d.%d.%d.%d\n", nIp1, nIp2, nIp3, nIp4);
        printf("    UserDefinedName: %s\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
        printf("    Device Model Name: %s\n\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chModelName);
    }
    else if (MV_USB_DEVICE == pstMVDevInfo->nTLayerType)
    {
        printf("    UserDefinedName: %s\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
        printf("    Device Model Name: %s\n\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chModelName);
    }
    else
    {
        printf("    Not support.\n\n");
    }
}

Camera::Camera() 
{
    init();
}

Camera::~Camera() 
{

}

bool Camera::init() 
{
    do
    {
        printf("running...\n");
        initialized = false;
        // ch:初始化SDK | en:Initialize SDK
        nRet = MV_CC_Initialize();
        if (MV_OK != nRet)
        {
            printf("Initialize SDK fail! nRet [0x%x]\n", nRet);
            break;
        }
        // ch:设备枚举 | en:Enum device
        MV_CC_DEVICE_INFO_LIST stDeviceList;
        memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
        nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
        if (MV_OK != nRet)
        {
            printf("Enum Devices fail! nRet [0x%x]\n", nRet);
            break;
        }
        // ch:选择相机 | en:Select device
        unsigned int nIndex = 0;
        // ch:显示设备信息 | en:Show device
        if (stDeviceList.nDeviceNum > 0)
        {
            printf("[device %d]:\n", nIndex);
            MV_CC_DEVICE_INFO* pDeviceInfo = stDeviceList.pDeviceInfo[nIndex];
            if (NULL == pDeviceInfo)
            {
                printf("No Device Info!\n");
                break;
            }
            PrintDeviceInfo(pDeviceInfo);
        }
        else
        {
            printf("Find No Devices!\n");
            break;
        }
        if (false == MV_CC_IsDeviceAccessible(stDeviceList.pDeviceInfo[nIndex], MV_ACCESS_Exclusive))
        {
            printf("Can't connect! ");
            break;
        }
        // ch:创建设备句柄 | en:Create handle
        nRet = MV_CC_CreateHandle(&handle, stDeviceList.pDeviceInfo[nIndex]);
        if (MV_OK != nRet)
        {
            printf("Create Handle fail! nRet [0x%x]\n", nRet);
            break;
        }
        // ch:打开设备 | en:Open device
        nRet = MV_CC_OpenDevice(handle);
        if (MV_OK != nRet)
        {
            printf("Open Device fail! nRet [0x%x]\n", nRet);
            break;
        }
        // ch:探测最佳Packet大小（只支持GigE相机） | en:Detection network optimal package size(It only works for the GigE camera)
        if (MV_GIGE_DEVICE == stDeviceList.pDeviceInfo[nIndex]->nTLayerType)
        {
            int nPacketSize = MV_CC_GetOptimalPacketSize(handle);
            if (nPacketSize > 0)
            {
                // 设置Packet大小
                nRet = MV_CC_SetIntValue(handle, "GevSCPSPacketSize", nPacketSize);
                if (MV_OK != nRet)
                {
                    printf("Warning: Set Packet Size fail! nRet [0x%x]!", nRet);
                }
            }
            else
            {
                printf("Warning: Get Packet Size fail! nRet [0x%x]!", nPacketSize);
            }
        }
        // ch:关闭触发模式 | en:Set trigger mode as off
        nRet = MV_CC_SetEnumValue(handle, "TriggerMode", 0);
        if (MV_OK != nRet)
        {
            printf("Set Trigger Mode fail! nRet [0x%x]\n", nRet);
            break;
        }
        // ch:获取图像大小 | en:Get payload size
        MVCC_INTVALUE stParam;
        memset(&stParam, 0, sizeof(MVCC_INTVALUE));
        nRet = MV_CC_GetIntValue(handle, "PayloadSize", &stParam);
        if (MV_OK != nRet)
        {
            printf("Get PayloadSize fail! nRet [0x%x]\n", nRet);
            break;
        }
        nPayloadSize = stParam.nCurValue;
        // ch:初始化图像信息 | en:Init image info
        memset(&stImageInfo, 0, sizeof(MV_FRAME_OUT_INFO_EX));
        pData = (unsigned char*)malloc(sizeof(unsigned char) * (nPayloadSize));
        if (NULL == pData)
        {
            printf("Allocate memory failed.\n");
            break;
        }
        memset(pData, 0, nPayloadSize);
        setFloatValue("ExposureTime",exposureTime);
        setFloatValue("Gain",gain);
        setFloatValue("AcquisitionFrameRate",frameRate);
        setBoolValue("AcquisitionFrameRateEnable", true);
        // // ch:开始取流 | en:Start grab image
        // nRet = MV_CC_StartGrabbing(handle);
        // if (MV_OK != nRet)
        // {
        //     printf("Start Grabbing fail! nRet [0x%x]\n", nRet);
        //     break;
        // }
        initialized = true;
    } while (0);
    return initialized; 
}

bool Camera::startGrabbing()
{
    if (!initialized)
    {
        printf("Not initialized.\n");
        return false;
    }
    // ch:开始取流 | en:Start grab image
    nRet = MV_CC_StartGrabbing(handle);
    if (MV_OK != nRet)
    {
        printf("Start Grabbing fail! nRet [0x%x]\n", nRet);
        return false;
    }
    grabbing = true;
    return true;
}

bool Camera::getFrame(RawFrame& frame) 
{
    if (!initialized)
    {
        printf("Not initialized.\n");
        return false;
    }
    bool success = false;
    do
    {
        // // ch:开始取流 | en:Start grab image
        // nRet = MV_CC_StartGrabbing(handle);
        // if (MV_OK != nRet)
        // {
        //     printf("Start Grabbing fail! nRet [0x%x]\n", nRet);
        //     break;
        // }
        // ch:获取一帧图像，超时时间500ms | en:Get one frame from camera with timeout=500ms
        nRet = MV_CC_GetOneFrameTimeout(handle, pData, nPayloadSize, &stImageInfo, 500);
        if (MV_OK == nRet)
        {
            // printf("Get One Frame: Width[%d], Height[%d], FrameNum[%d]\n",
            //     stImageInfo.nWidth, stImageInfo.nHeight, stImageInfo.nFrameNum);
        }
        else
        {
            printf("Get Frame fail! nRet [0x%x]\n", nRet);
            this_thread::sleep_for(chrono::seconds(1));
            break;
        }
        // // ch:停止取流 | en:Stop grab image
        // nRet = MV_CC_StopGrabbing(handle);
        // if (MV_OK != nRet)
        // {
        //     printf("Stop Grabbing fail! nRet [0x%x]\n", nRet);
        //     break;
        // }
        frame.data.resize(nPayloadSize);
        memcpy(frame.data.data(), pData, nPayloadSize);
        frame.info = stImageInfo;
        success = true;
        // // ch:数据转换 | en:Convert image data
        // bool bConvertRet = false;
        // bConvertRet = Convert2Mat(&stImageInfo, pData, srcImage);
        // if (bConvertRet)
        // {
        //     printf("OpenCV format convert finished.\n");
        //     success = true;
        // }
        // else
        // {
        //     printf("OpenCV format convert failed.\n");
        //     break;
        // }
    } while (0);
    return success;
}

bool Camera::stopGrabbing()
{
    if (!initialized)
    {
        printf("Not initialized.\n");
        return false;
    }
    // ch:停止取流 | en:Stop grab image
    nRet = MV_CC_StopGrabbing(handle);
    if (MV_OK != nRet)
    {
        printf("Stop Grabbing fail! nRet [0x%x]\n", nRet);
        return false;
    }
    grabbing =false;
    return true;
}

bool Camera::release()
{
    do
    {
        if (!initialized)
        {
            printf("Not initialized.\n");
            break;
        }
        // // ch:停止取流 | en:Stop grab image
        // nRet = MV_CC_StopGrabbing(handle);
        // if (MV_OK != nRet)
        // {
        //     printf("Stop Grabbing fail! nRet [0x%x]\n", nRet);
        //     break;
        // }
        // ch:关闭设备 | en:Close device
        nRet = MV_CC_CloseDevice(handle);
        if (MV_OK != nRet)
        {
            printf("ClosDevice fail! nRet [0x%x]\n", nRet);
            break;
        }
        // ch:销毁句柄 | en:Destroy handle
        if (handle)
        {
            MV_CC_DestroyHandle(handle);
            handle = NULL;
        }
        // ch:释放内存 | en:Free memery
        if (pData)
        {
            free(pData);
            pData = NULL;
        }
        // ch:反初始化SDK | en:Finalize SDK
        MV_CC_Finalize();
        initialized = false;
        printf("Release completed.\n");
    } while (0);
    return !initialized;
}

bool Camera::getInitialized()
{
    return initialized;
}

bool Camera::getGrabbing()
{
    return grabbing;
}

void Camera::setFloatValue(const char* strKey, float& value)
{
    nRet = MV_CC_SetFloatValue(handle, strKey, value);
    if (MV_OK != nRet)
    {
        printf("Set Float fail! nRet [0x%x]\n", nRet);
    }
    else
    {
        printf("set %s as %f\n", strKey, value);
    }
}

void Camera::setBoolValue(const char* strKey, bool value)
{
    nRet = MV_CC_SetBoolValue(handle, strKey, value);
    if (MV_OK != nRet)
    {
        printf("Set Bool fail! nRet [0x%x]\n", nRet);
    }
    else
    {
        printf("set %s as %u\n", strKey, value);
    }
}

float Camera::getFloatvalue(const char* strKey)
{
    MVCC_FLOATVALUE strFloat = {0};
    nRet = MV_CC_GetFloatValue(handle, strKey, &strFloat);
    if (MV_OK != nRet)
    {
        printf("Get Float fail! nRet [0x%x]\n", nRet);
        return 0.0;
    }
    else
    {
        return strFloat.fCurValue;
    }
}

bool Convert2Mat(const RawFrame& frame, Mat& srcImage)
{
    if (frame.data.empty())
    {
        printf("Empty image data.\n");
        return false;
    }
    Mat cvtImage;
    if (PixelType_Gvsp_Mono8 == frame.info.enPixelType)                
    {
        cvtImage = Mat(frame.info.nHeight, frame.info.nWidth, CV_8UC1, const_cast<unsigned char*>(frame.data.data()));
    }
    else
    {
        printf("Unsupported pixel format\n");
        return false;
    }
    if (NULL == cvtImage.data)
    {
        printf("Creat Mat failed.\n");
        return false;
    }
    srcImage = cvtImage.clone();
    return true;
}

bool writeBuffer(const RawFrame& frameBuffer)
{
    if (!bufferAFull) 
    {
        mutexA.lock();
        rawFrameA = frameBuffer;
        bufferAFull = true;
        bufferARefresh = false;
        mutexA.unlock();
    }
    else 
    {
        if (!bufferBFull) 
        {
            mutexB.lock();
            rawFrameB = frameBuffer;
            bufferBFull = true;
            bufferARefresh = true;
            mutexB.unlock();
        }
        else 
        {
            if (bufferARefresh) 
            {
                bufferAFull = false;
                mutexA.lock();
                rawFrameA = frameBuffer;
                bufferAFull = true;
                bufferARefresh = false;
                mutexA.unlock();
            }
            else 
            {
                bufferBFull = false;
                mutexB.lock();
                rawFrameB = frameBuffer;
                bufferBFull = true;
                bufferARefresh = true;
                mutexB.unlock();
            }
        }
    }
    return true;
}

bool readBuffer(RawFrame& rawFrame) 
{
    for (short i = 0; i < 10; i++) 
    {
        if (bufferAFull) 
        {
            mutexA.lock();
            rawFrame = rawFrameA;
            bufferAFull = false;
            mutexA.unlock();
            return true;
        }
        else 
        {
            if (bufferBFull) 
            {
                mutexB.lock();
                rawFrame = rawFrameB;
                bufferBFull = false;
                mutexB.unlock();
                return true;
            }
            else 
            {
                this_thread::sleep_for(chrono::milliseconds(10));
            }
        }
    }
    return false;
}

void writeThread() 
{
    const int writeDelayMs = 200;
    RawFrame rawFrame;
    bool bRawFrame = false;
    Camera camera;
    
    while (programRunning)
    {
        if (cameraSetting)
        {
            if (!camera.getInitialized())
            {
                if (!camera.init())
                {
                    this_thread::sleep_for(chrono::milliseconds(100));
                    continue;
                }
            }
            camera.setFloatValue("ExposureTime",exposureTime);
            camera.setFloatValue("Gain",gain);
            camera.setFloatValue("AcquisitionFrameRate",frameRate);
            camera.setBoolValue("AcquisitionFrameRateEnable", true);
            this_thread::sleep_for(chrono::seconds(1));
        }
        else if (cameraRunning) 
        {
            if (!camera.getInitialized())
            {
                if (!camera.init())
                {
                    this_thread::sleep_for(chrono::seconds(1));
                    continue;
                }
            }
            if (!camera.getGrabbing())
            {
                if(!camera.startGrabbing()) {
                    this_thread::sleep_for(chrono::seconds(1));
                    continue;
                }
            }
            auto start = chrono::steady_clock::now();
            bRawFrame = camera.getFrame(rawFrame);
            if (!bRawFrame)
            {
                continue;
            }
            else
            {
                writeBuffer(rawFrame);
            }
            auto elapsed = chrono::steady_clock::now() - start;
            auto waitMs = writeDelayMs - chrono::duration_cast<chrono::milliseconds>(elapsed).count();
            // cout << "[Write] Frame processed in " << chrono::duration_cast<chrono::milliseconds>(elapsed).count() << " ms\n";
            if (waitMs > 0) {
                this_thread::sleep_for(chrono::milliseconds(waitMs));
            }
        }
        else
        {
            if (camera.getGrabbing())
            {
                if(!camera.stopGrabbing()) {
                    this_thread::sleep_for(chrono::seconds(1));
                    continue;
                }
            }
            this_thread::sleep_for(chrono::seconds(1));            
        }
    }
}

void readThread() 
{
    const int readDelayMs = 500;
    const string baseDir = getExecutableDir() + "/../output/images/";
    Mat srcImage;
    RawFrame rawFrame;
    bool bRawFrame = false;
    int imageIndex = 0;

    time_t t = time(nullptr);
    struct tm now_tm;
    localtime_r(&t, &now_tm);
    char dateStr[20];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &now_tm);
    folderPath = baseDir + dateStr;

    if (!fs::exists(folderPath))
    {
        fs::create_directories(folderPath);
    }
    readIndexFile(imageIndex);

    while (programRunning)
    {
        if (cameraRunning)
        {
            auto start = chrono::steady_clock::now();
            bRawFrame = readBuffer(rawFrame);
            if (!bRawFrame)
            {
                continue;
            }
            else
            {
                ostringstream oss;
                lock_guard<mutex> lock(fileMutex);
                oss << folderPath << "/" << imageIndex << ".bmp";
                if (Convert2Mat(rawFrame, srcImage))
                {
                    imwrite(oss.str(), srcImage);
                    ofstream indexFile(folderPath + "/index.txt", ios::trunc);
                    if (indexFile.is_open()) {
                        indexFile << (imageIndex + 1);
                        indexFile.close();
                    }
                    imageIndex++;
                }
            }
            auto elapsed = chrono::steady_clock::now() - start;
            auto waitMs = readDelayMs - chrono::duration_cast<chrono::milliseconds>(elapsed).count();
            // cout << "[Read] Frame processed in " << chrono::duration_cast<chrono::milliseconds>(elapsed).count() << " ms\n";
            if (waitMs > 0) {
                this_thread::sleep_for(chrono::milliseconds(waitMs));
            }
        }
        else
        {
            this_thread::sleep_for(chrono::seconds(1));
        }
    }
}