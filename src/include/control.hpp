#include <iostream>
#include <algorithm>
#include <thread>
#include <vector>
#include <Python.h>

using namespace std;

inline std::uint8_t controlRunning = 0x00;
inline uint8_t flyWheel[4] = {0};

void ControlThread();

PyObject *toPyList(std::vector<double> &vec);
vector<float> pyListToVector(PyObject* listObj);
vector<double> ReadInputData();
// inline void packRotationWheel(const std::vector<uint8_t>& RW, uint8_t (&out_bytes)[4])
// {
//     int offset = 4;
//     std::copy(RW.begin(), RW.end(), out_bytes + offset);
// } 
inline void packRotationWheel(std::vector<float>& input, uint8_t output[4]) 
{
    // add a zero at the end of 3 RW control outputs
    if (input.size() == 3) 
    {
        input.push_back(0.0f);
    }

    for (int i = 0; i < 4; ++i) {
    // 1. 将值钳制在[0,1]范围（防止越界）
        float clamped = std::clamp(input[i], 0.0f, 1.0f);

    // 2. 映射到0-255并四舍五入
        float scaled = clamped * 255.0f;
        uint8_t value = static_cast<uint8_t>(scaled + 0.5f); // +0.5实现四舍五入

    // 3. 存入输出数组
        output[i] = value;
    }
}

