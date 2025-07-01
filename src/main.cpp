#include "Camera.hpp"
#include "CPy.hpp"
#include "vision.hpp"
// #include "UDPClient.hpp"
#include "TCPClient.hpp"
#include "file.hpp"
#include "dds.hpp"
#include "global_state.hpp"

#include <csignal>

void handleSignal(int)
{
    programRunning = false;
    cameraRunning = false;
    cameraSetting = false;
    // TCPSocketRunning = false;
    // UDPSocketRunning = false;
    visionRunning = false;
}

// Shell scripts can gracefully terminate the program by sending
// SIGINT or SIGTERM to the process. The handler clears all running
// flags so the threads exit cleanly.

using namespace std;

int main()
{
    uint8_t dExpEx = 0x00;
    uint8_t dGainEx = 0x00;
    uint8_t dFrameRateEx = 0x00;
    uint8_t telemetry[50] = {0};

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);
    cameraSetting = false;
    cameraRunning = false;
    // UDPSocketRunning = false;
    // TCPSocketRunning = false;
    visionRunning = false;

    thread t1(writeThread);
    thread t2(readThread);
    // thread t3(UDPThread);
    // thread t3(TCPThread);
    thread t4(DDSSubThread);
    thread t5(visionThread);

    // DDSPub_t* pub = DDSPub_create();
    // DDSPub_set_domain_id(pub, 1);
    // DDSPub_init(pub);
    // printf("DDSPub created.\n");

    while(programRunning)
    {
        {std::lock_guard<std::mutex> lock(ddsMutex);
        bool expSettingBit = (dExpBit && dExpEx != dExp);
        bool gainSettingBit = (dGainBit && dGainEx != dGain);
        bool frameRateSettingBit = (dFrameRateBit && dFrameRateEx != dFrameRate);
        bool settingBit = expSettingBit || gainSettingBit || frameRateSettingBit;
        if (settingBit)
        {
            if (expSettingBit) 
            {
                dExpEx = dExp;
                exposureTime = getExposureTime(dExpEx);
            }
            if (gainSettingBit) 
            {
                dGainEx = dGain;
                gain = getGain(dGainEx);
            }
            if (frameRateSettingBit) 
            {
                dFrameRateEx = dFrameRate;
                frameRate = getFrameRate(dFrameRateEx);
            }            
            cameraSetting = true;
        }
        else
        {
            cameraSetting = false;
        }
        if (imageBit)
        {
            cameraRunning = true;
        }
        else
        {
            cameraRunning = false;
        }
        if (dataTransBit)
        {
            if (!fileClear) {
                lock_guard<mutex> lock(fileMutex);
                fileClear = true;
                if (filesystem::exists(folderPath)) {
                    filesystem::remove_all(folderPath);
                    cout << " Cleared folder: " << folderPath << '\n';
                }
                else {
                    cout << " No such folder: " << folderPath << '\n';
                }
            }
        }
        else
        {
            if (!filesystem::exists(folderPath) && fileClear)
            {
                filesystem::create_directories(folderPath);
            }
            fileClear = false;
            // UDPSocketRunning = false;
            // TCPSocketRunning = false;
        }
        if (poseBit)
        {
            visionRunning = true;
        }
        else
        {
            visionRunning = false;
            const uint8_t tmp1[3] = {0x00, 0x00, 0x00};
            memcpy(dectResult, tmp1, sizeof(tmp1));
            const float tmp2[10] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
            memcpy(poseResult, tmp2, sizeof(tmp2));
        }
        if (controlBit)
        {
            const uint8_t tmp3[4] = {0xFF, 0xFF, 0xFF, 0xFF};
            memcpy(flyWheel, tmp3, sizeof(tmp3));
        }
        else
        {
            const uint8_t tmp3[4] = {0x00, 0x00, 0x00, 0x00};
            memcpy(flyWheel, tmp3, sizeof(tmp3));
        }

        const uint8_t type[2] = {0x00, 0x02};
        uint8_t byte1[1] = {uint8_t(uint8_t(imageBit << 7) | uint8_t(dataTransBit << 6) | uint8_t(poseBit << 5) | uint8_t(controlBit << 4) | uint8_t(dExpEx))};
        dectResult[0] = (dectResult[0]) | ((dFrameRateEx & 0x03) << 4) | ((dGainEx & 0x03) << 6);

        int offset = 0;
        memcpy(telemetry + offset, type, sizeof(type)); offset += sizeof(type);
        memcpy(telemetry + offset, byte1, sizeof(byte1)); offset += sizeof(byte1);
        memcpy(telemetry + offset, dectResult, sizeof(dectResult)); offset += sizeof(dectResult);
        memcpy(telemetry + offset, poseResult, sizeof(poseResult)); offset += sizeof(poseResult);
        memcpy(telemetry + offset, flyWheel, sizeof(flyWheel)); offset += sizeof(flyWheel);}

        DDSPub(telemetry, TM_receive_cnt);
        this_thread::sleep_for(chrono::milliseconds(1000));
    }

    // DDSPub_destroy(pub); //摧毁pub
    // DDSSub_destroy(g_sub); //摧毁sub 
    // pthread_cancel(t4.native_handle());
    
    // cout << "DDS destroyed." << endl;

    if (t1.joinable()) t1.join();
    if (t2.joinable()) t2.join();
    // if (t3.joinable()) t3.join();
    if (t4.joinable()) t4.join();
    if (t5.joinable()) t5.join();

    this_thread::sleep_for(chrono::seconds(1));
    cout << "Program exited." << endl;

    return 0;
}