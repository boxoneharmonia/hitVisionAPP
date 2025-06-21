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

void visionThread();
bool arucoDetect(const std::string &path, const cv::Mat& cameraMatrix, const cv::Mat& distCoeffs, std::array<float, 3> &tAvg, std::array<float, 4> &qAvg);
void dcmToQuat(cv::Mat &R, std::array<float, 4> &qOut);
void averageQuat(std::vector<std::array<float, 4>> &quats, std::array<float, 4> &qOut);
class VelocityEstimator;

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

    out_bytes[0] = static_cast<uint8_t>(conf > 0x0D);
    out_bytes[1] = ((q_xmin & 0x0F) << 4) | (q_ymin & 0x0F);
    out_bytes[2] = ((q_xmax & 0x0F) << 4) | (q_ymax & 0x0F);
}

inline void packPoseResult(const std::array<float, 3>& tmc, const std::array<float, 3>& vel, const std::array<float, 4>& pos, float (&out_bytes)[10])
{
    int offset = 0;
    std::copy(tmc.begin(), tmc.end(), out_bytes + offset); offset += tmc.size();
    std::copy(vel.begin(), vel.end(), out_bytes + offset); offset += vel.size();
    std::copy(pos.begin(), pos.end(), out_bytes + offset); offset += pos.size();
} 

class VelocityEstimator {
public:
    VelocityEstimator(float dt, float alpha)
        : dt_(dt), alpha_(alpha), first_frame_(true) {
        previous_position_.fill(0.0f);
        filtered_velocity_.fill(0.0f);
    }

    void updateParam(float dt, float alpha){
        dt_ = dt;
        alpha_ = alpha;
    }

    std::array<float, 3> update(const std::array<float, 3>& position) {
        std::array<float, 3> out_velocity{};
        if (first_frame_) {
            filtered_velocity_.fill(0.0f);
            first_frame_ = false;
        } else {
            std::array<float, 3> raw_velocity;
            for (int i = 0; i < 3; ++i) {
                raw_velocity[i] = (position[i] - previous_position_[i]) / dt_;
                filtered_velocity_[i] = alpha_ * raw_velocity[i] + (1.0f - alpha_) * filtered_velocity_[i];
            }
        }

        previous_position_ = position;
        out_velocity = filtered_velocity_;
        return out_velocity;
    }

private:
    float dt_;
    float alpha_;
    bool first_frame_;
    std::array<float, 3> previous_position_;
    std::array<float, 3> filtered_velocity_;
};