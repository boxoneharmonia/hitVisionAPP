#ifndef DDS_HPP
#define DDS_HPP

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "C_DDSSub.h"
#include "C_DDSPub.h"
#include "DDS_type.h"
#include "cJSON.h"
#include <stdio.h>
#include <cstring>
#include <mutex>

// 第1个字节
inline bool imageBit = false; // 成像 控制位
inline bool dataTransBit = false; // 数传 控制位
inline bool poseBit = false; // 位姿估计 控制位
inline bool controlBit = false; // 智能控制 控制位
inline bool dExpBit = false; // 曝光时间
inline bool dGainBit = false; // 增益
inline bool dFrameRateBit = false; // 帧率

// 第2个字节
inline uint8_t dExp = 0x00; // 曝光时间
inline uint8_t dGain = 0x00; // 增益
inline uint8_t dFrameRate = 0x00; // 帧率

inline uint8_t TM_receive_cnt; // 遥测接收数

inline std::mutex ddsMutex;

float getExposureTime(uint8_t dExpEx);
float getGain(uint8_t dGainEx);
float getFrameRate(uint8_t dFrameRateEx);

void DDSPub(uint8_t data_message[], uint8_t receive_cnt, DDSPub_t* pub, int topic);
void DDSSubThread();

#endif
