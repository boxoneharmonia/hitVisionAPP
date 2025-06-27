#include "vision.hpp"
#include "file.hpp"
#include "CPy.hpp"
#include "dds.hpp"
#include "Camera.hpp"
#include "global_state.hpp"
#include <opencv2/aruco.hpp>
#include <Eigen/Core>
#include <Eigen/Geometry>

using namespace std;
using namespace cv;

void visionThread()
{
    const string baseDir = getExecutableDir();
    string imagePath;
    int imageIndex = -1;
    int imageIndexNew = 0;
    static array<float, 4> bbox;
    static array<float, 3> tmc;
    static array<float, 3> vel;
    static array<float, 4> pose;
    Mat cameraMatrix, distCoeffs;
    cameraMatrix = (Mat_<double>(3,3) << 1814.6, 0, 619.7631, 0, 1813.0, 480.8544, 0, 0, 1);
    distCoeffs = (Mat_<double>(1,5) << 0.0374, -0.0373, 0.0, 0.0, -0.01);
    VelocityEstimator vest(0.5f, 0.3f);
    PyCaller py(baseDir, "model");
    py.callFunction("load_model");
    while (programRunning)
    {
        if (visionRunning)
        {
            // vest.updateParam(1.0f/frameRate, 0.3f);
            readIndexFile(imageIndexNew);
            imageIndexNew--;
            if (imageIndexNew > imageIndex)
            {
                imageIndex = imageIndexNew;
                imagePath = folderPath + "/" + to_string(imageIndexNew) + ".bmp";
                PyObject *args = toPyTuple(imagePath);
                PyObject *ret = py.callFunctionWithRet("infer_1", args);
                Py_DECREF(args);
                if (ret && PyTuple_Check(ret) && PyTuple_Size(ret) == 3)
                {
                    double confidence = PyFloat_AsDouble(PyTuple_GetItem(ret, 0));

                    PyObject *bbox_list = PyTuple_GetItem(ret, 1); // list of floats
                    PyObject *tmc_list = PyTuple_GetItem(ret, 2); // list of floats

                    if (PyList_Check(bbox_list) && PyList_Size(bbox_list) == 4)
                    {
                        for (Py_ssize_t i = 0; i < 4; ++i)
                        {
                            bbox[i] = PyFloat_AsDouble(PyList_GetItem(bbox_list, i));
                        }
                    }

                    if (PyList_Check(tmc_list) && PyList_Size(tmc_list) == 3)
                    {
                        for (Py_ssize_t i = 0; i < 3; ++i)
                        {
                            tmc[i] = PyFloat_AsDouble(PyList_GetItem(tmc_list, i));
                        }
                    }

                    if (tmc[2] < 10.0f) {
                        bool arucoDetected = false;
                        static array<float, 3> tMarker;
                        static array<float, 4> qMarker;
                        arucoDetected = arucoDetect(imagePath, cameraMatrix, distCoeffs, tMarker, qMarker);
                        if (arucoDetected) {
                            confidence = 1.0;
                            tmc = tMarker;
                            pose = qMarker;
                        }
                    }
                
                    vel = vest.update(tmc);

                    Py_DECREF(bbox_list);
                    Py_DECREF(tmc_list);
                    Py_DECREF(ret);
                    {
                        std::lock_guard<std::mutex> lock(ddsMutex);
                        packDectResult(confidence, bbox, dectResult);
                        packPoseResult(tmc, vel, pose, poseResult);
                    }
                }
                else
                {
                    PyErr_Print();
                }
            }
        }
        else
        {
            this_thread::sleep_for(chrono::seconds(1));
        }
    }
    py.callFunction("release");
}

bool arucoDetect(const string &path, const Mat& cameraMatrix, const Mat& distCoeffs, 
    array<float, 3> &tAvg, array<float, 4> &qAvg) {
    Mat image, gray;
    Ptr<aruco::DetectorParameters> parameters = aruco::DetectorParameters::create();
    Ptr<aruco::Dictionary> dictionary = aruco::getPredefinedDictionary(aruco::DICT_4X4_250);
    vector<int> markerIds, validIds;
    vector<vector<Point2f>> markerCorners, rejectedCandidates, validCorners;
    vector<Vec3d> rvecs, tvecs;

    image = imread(path, IMREAD_GRAYSCALE);
    gray = image.clone();						
    
    for (int y = 0; y < gray.rows; y++) {
        uint8_t* ptr = gray.ptr<uint8_t>(y);
        for (int x = 0; x < gray.cols; x++) {
            uint8_t v = ptr[x];
            if (v < 150)
                ptr[x] = saturate_cast<uchar>(v / 150.0f * 255.0f);
            else
                ptr[x] = 255;
        }
    }

    aruco::detectMarkers(gray, dictionary, markerCorners, markerIds, parameters, rejectedCandidates);
    if (markerIds.empty()) return false;

    for (uint i = 0; i < markerIds.size(); i++) {
        if (markerIds[i] <= 3) {
            validIds.push_back(markerIds[i]);
            validCorners.push_back(markerCorners[i]);
        }
    }

    if (validIds.empty()) return false;
    aruco::estimatePoseSingleMarkers(validCorners, 0.07, cameraMatrix, distCoeffs, rvecs, tvecs);
    vector<array<float,4>> quats;

    size_t numMarkers = validIds.size();
    for (uint i = 0; i < numMarkers; i++) {
        Mat R;
        array<float, 4> qMarker;
        Rodrigues(rvecs[i], R);
        dcmToQuat(R, qMarker);
        quats.push_back(qMarker);
    }

    if (numMarkers == 1) {
        for (uint j = 0; j < 3; ++j)
            tAvg[j] = static_cast<float>(tvecs[0][j]);

        qAvg = quats[0];
    }

    else {
        Vec3d t_sum(0, 0, 0);
        for (const auto& tvec : tvecs)
            t_sum += Vec3d(tvec[0], tvec[1], tvec[2]);

        t_sum = Vec3d(t_sum[0] / numMarkers, t_sum[1] / numMarkers, t_sum[2] / numMarkers);
        for (uint j = 0; j < 3; ++j)
            tAvg[j] = static_cast<float>(t_sum[j]);
        averageQuat(quats, qAvg);
    }

    for (size_t i = 0; i < numMarkers; i++) {
        printf("Marker ID: %d | Position: [%.3f, %.3f, %.3f] | Quaternion: [%.3f, %.3f, %.3f, %.3f]\n",
               validIds[i],
               tvecs[i][0], tvecs[i][1], tvecs[i][2],
               quats[i][0], quats[i][1], quats[i][2], quats[i][3]);
    }

    return true;
}

void dcmToQuat(Mat &R, array<float, 4> &qOut) {
    array<float, 4> Quat;

    float Trace = R.at<double>(0, 0) + R.at<double>(1, 1) + R.at<double>(2, 2);
    if (Trace >= 0.0)
    {
        Quat[0] = float(sqrt(1.0 + Trace) / 2.0);
        Quat[1] = (R.at<double>(1, 2) - R.at<double>(2, 1)) / Quat[0] / 4.0;
        Quat[2] = (R.at<double>(2, 0) - R.at<double>(0, 2)) / Quat[0] / 4.0;
        Quat[3] = (R.at<double>(0, 1) - R.at<double>(1, 0)) / Quat[0] / 4.0;
    }
    else
    {
        if ((R.at<double>(1, 1) > R.at<double>(0, 0)) && (R.at<double>(1, 1) > R.at<double>(2, 2)))
        {
            Quat[2] = sqrt(1.0 - R.at<double>(0, 0) + R.at<double>(1, 1) - R.at<double>(2, 2)) / 2.0;
            Quat[0] = (R.at<double>(2, 0) - R.at<double>(0, 2)) / Quat[2] / 4.0;
            Quat[1] = (R.at<double>(1, 0) + R.at<double>(0, 1)) / Quat[2] / 4.0;
            Quat[3] = (R.at<double>(2, 1) + R.at<double>(1, 2)) / Quat[2] / 4.0;
        }
        else if (R.at<double>(2, 2) > R.at<double>(0, 0))
        {
            Quat[3] = sqrt(1.0 - R.at<double>(0, 0) - R.at<double>(1, 1) + R.at<double>(2, 2)) / 2.0;
            Quat[0] = (R.at<double>(0, 1) - R.at<double>(1, 0)) / Quat[3] / 4.0;
            Quat[1] = (R.at<double>(2, 0) + R.at<double>(0, 2)) / Quat[3] / 4.0;
            Quat[2] = (R.at<double>(2, 1) + R.at<double>(1, 2)) / Quat[3] / 4.0;
        }
        else
        {
            Quat[1] = sqrt(1.0 + R.at<double>(0, 0) - R.at<double>(1, 1) - R.at<double>(2, 2)) / 2.0;
            Quat[0] = (R.at<double>(1, 2) - R.at<double>(2, 1)) / Quat[1] / 4.0;
            Quat[2] = (R.at<double>(1, 0) + R.at<double>(0, 1)) / Quat[1] / 4.0;
            Quat[3] = (R.at<double>(2, 0) + R.at<double>(0, 2)) / Quat[1] / 4.0;
        }
    }

    double squareSum = sqrt(Quat[0] * Quat[0] + Quat[1] * Quat[1] + Quat[2] * Quat[2] + Quat[3] * Quat[3]);

    for (size_t i = 0; i < 4; i++) {
        Quat[i] = Quat[i] / squareSum;
    }

    qOut = Quat;
}

void averageQuat(vector<array<float, 4>> &quats, array<float, 4> &qOut) {
    vector<Eigen::Quaternionf> qvecs;
    size_t numQuat = quats.size();
    for (uint i = 0; i < numQuat; i++) {
        Eigen::Quaternionf qi(quats[i][0], quats[i][1], quats[i][2], quats[i][3]);
        if (qi.w() < 0) qi.coeffs() *= -1.0f;
        qvecs.push_back(qi);
    }

    Eigen::Quaternionf qBest;
    float errBest = 100;
    bool isBest = false;

    for (uint i = 0; i < numQuat; i++) {
        for (uint j = i + 1; j < numQuat; j++) {
            if (qvecs[i].dot(qvecs[j]) < 0.98) continue;
            Eigen::Quaternionf qAvg = qvecs[i].slerp(0.5, qvecs[j]);
            float err = 0;
            for (uint k = 0; k < numQuat; k++) {
                err += 1 - qAvg.dot(qvecs[k]);
            }
            if (err < errBest) {
                errBest = err;
                isBest = true;
                qBest = qAvg;
            }
        }
    }

    if (isBest) 
        qOut = {qBest.w(), qBest.x(), qBest.y(), qBest.z()};
    else
        qOut = quats[0];
}