#ifndef CPY_HPP
#define CPY_HPP

#include <Python.h>
#include <string>
#include <iostream>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>
#include <cstdint>
#include <algorithm>
#include <array>
#include <cmath>
#include <opencv2/opencv.hpp>

inline std::atomic_bool visionRunning = false;
inline uint8_t dectResult[3] = {0};
inline float poseResult[10] = {0};
inline uint8_t flyWheel[4] = {0};

class PyCaller 
{
public:
    PyCaller(const std::string& module_dir, const std::string& module_name);
    ~PyCaller();

    bool isInitialized();
    bool callFunction(const std::string& func_name);
    bool callFunction(const std::string& func_name, PyObject* pArgs);
    PyObject* callFunctionWithRet(const std::string& func_name);
    PyObject* callFunctionWithRet(const std::string& func_name, PyObject* pArgs);

private:
    void initialize();
    void finalize();

    std::string module_dir_;
    std::string module_name_;
    PyObject* pModule_;
    bool initialized_;
};

PyObject* toPyTuple(const std::vector<std::string>& vec);
PyObject* toPyTuple(const std::string& str);

void CPyThread();

inline uint8_t quantizeToInt4(float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    return static_cast<uint8_t>(value * 15.0f + 0.5f); // round to nearest
}

inline void packDectResult(float conf, const std::array<float, 4>& bbox, uint8_t (&out_bytes)[3]) 
{
    uint8_t q_conf = quantizeToInt4(conf);
    uint8_t q_xmin = quantizeToInt4(bbox[0]);
    uint8_t q_ymin = quantizeToInt4(bbox[1]);
    uint8_t q_xmax = quantizeToInt4(bbox[2]);
    uint8_t q_ymax = quantizeToInt4(bbox[3]);

    out_bytes[0] = q_conf & 0x0F;
    out_bytes[1] = ((q_xmin & 0x0F) << 4) | (q_ymin & 0x0F);
    out_bytes[2] = ((q_xmax & 0x0F) << 4) | (q_ymax & 0x0F);
}

inline void packPoseResult(const std::array<float, 3>& tmc, const std::array<float, 3>& vel, const std::array<float, 4>& pose, float (&out_bytes)[10])
{
    int offset = 0;
    memcpy(out_bytes + offset, tmc.data(), sizeof(tmc)); offset += sizeof(tmc);
    memcpy(out_bytes + offset, vel.data(), sizeof(vel)); offset += sizeof(vel);
    memcpy(out_bytes + offset, pose.data(), sizeof(pose)); offset += sizeof(pose);
} 

#endif // CPY_HPP
