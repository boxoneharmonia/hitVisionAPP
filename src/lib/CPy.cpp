#include "CPy.hpp"
#include "file.hpp"
#include "dds.hpp"
#include "Camera.hpp"
#include <opencv2/aruco.hpp>
using namespace std;
using namespace cv;

PyCaller::PyCaller(const string &module_dir, const string &module_name)
    : module_dir_(module_dir), module_name_(module_name), pModule_(nullptr), initialized_(false)
{
    initialize();
}

PyCaller::~PyCaller()
{
    finalize();
}

void PyCaller::initialize()
{
    Py_Initialize();

    PyRun_SimpleString("import sys");
    string path_cmd = "sys.path.append('" + module_dir_ + "')";
    PyRun_SimpleString(path_cmd.c_str());

    PyObject *pName = PyUnicode_DecodeFSDefault(module_name_.c_str());
    pModule_ = PyImport_Import(pName);
    Py_XDECREF(pName);

    if (pModule_ == nullptr)
    {
        PyErr_Print();
        initialized_ = false;
    }
    else
    {
        initialized_ = true;
    }
}

void PyCaller::finalize()
{
    Py_XDECREF(pModule_);
    if (Py_IsInitialized())
        Py_Finalize();
}

bool PyCaller::isInitialized()
{
    return initialized_;
}

bool PyCaller::callFunction(const std::string &func_name)
{
    return callFunction(func_name, nullptr);
}

bool PyCaller::callFunction(const std::string &func_name, PyObject *pArgs)
{
    if (!initialized_)
        return false;

    PyObject *pFunc = PyObject_GetAttrString(pModule_, func_name.c_str());
    if (pFunc && PyCallable_Check(pFunc))
    {
        PyObject *pResult = PyObject_CallObject(pFunc, pArgs);
        if (pResult)
        {
            Py_DECREF(pResult);
            Py_XDECREF(pFunc);
            return true;
        }
        else
        {
            PyErr_Print();
        }
    }
    else
    {
        PyErr_Print();
    }

    Py_XDECREF(pFunc);
    return false;
}

PyObject *PyCaller::callFunctionWithRet(const std::string &func_name)
{
    return callFunctionWithRet(func_name, nullptr);
}

PyObject *PyCaller::callFunctionWithRet(const std::string &func_name, PyObject *pArgs)
{
    if (!initialized_)
        return nullptr;

    PyObject *pFunc = PyObject_GetAttrString(pModule_, func_name.c_str());
    if (pFunc && PyCallable_Check(pFunc))
    {
        PyObject *pResult = PyObject_CallObject(pFunc, pArgs);
        Py_XDECREF(pFunc);
        if (pResult)
        {
            return pResult; // must Py_DECREF by caller
        }
        else
        {
            PyErr_Print();
            return nullptr;
        }
    }
    else
    {
        PyErr_Print();
        Py_XDECREF(pFunc);
        return nullptr;
    }
}

PyObject *toPyTuple(const vector<string> &vec)
{
    PyObject *tuple = PyTuple_New(vec.size());
    for (size_t i = 0; i < vec.size(); ++i)
    {
        PyObject *item = PyUnicode_FromString(vec[i].c_str());
        PyTuple_SetItem(tuple, i, item);
    }
    return tuple;
}

PyObject *toPyTuple(const string &str)
{
    return Py_BuildValue("(s)", str.c_str());
}

void CPyThread()
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
    VelocityEstimator vest(0.1f, 0.3f);
    PyCaller py(baseDir, "model");
    py.callFunction("load_model");
    while (1)
    {
        if (visionRunning)
        {
            vest.updateParam(1.0f/frameRate, 0.3f);
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

                    if (tmc[2] < 5.0f) {
                        bool arucoDetected = false;
                        static array<float, 3> tMarker;
                        static array<float, 4> qMarker;
                        arucoDetected = arucoDetect(imagePath, cameraMatrix, distCoeffs, tMarker, qMarker);
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

bool arucoDetect(const string &path, const Mat& cameraMatrix, const Mat& distCoeffs, 
    array<float, 3> &tAvg, array<float, 4> &qAvg) {
    Mat image, gray;
    Ptr<aruco::DetectorParameters> parameters = aruco::DetectorParameters::create();
    Ptr<aruco::Dictionary> dictionary = aruco::getPredefinedDictionary(aruco::DICT_4X4_250);
    vector<int> markerIds, validIds;
    vector<vector<Point2f>> markerCorners, rejectedCandidates, validCorners;
    vector<Vec3d> rvecs, tvecs;

    image = imread(path);
    cvtColor(image, gray, COLOR_BGR2GRAY);
    gray.convertTo(gray, CV_8UC1);
    aruco::detectMarkers(gray, dictionary, markerCorners, markerIds, parameters, rejectedCandidates);
    if (markerIds.empty()) return false;

    for (size_t i=0; i < markerIds.size(); i++) {
        if (markerIds[i] > 3) continue;
        else {
            validIds.push_back(markerIds[i]);
            validCorners.push_back(markerCorners[i]);
        }
    }

    if (validIds.empty()) return false;
    aruco::estimatePoseSingleMarkers(validCorners, 0.07, cameraMatrix, distCoeffs, rvecs, tvecs);
    vector<array<float,4>> quats;

    size_t numMarkers = validIds.size();
    for (uint i=0; i < numMarkers; i++) {
        Mat R;
        array<float, 4> qMarker;
        Rodrigues(rvecs[i], R);
        dcmToQuat(R, qMarker);
        quats.push_back(qMarker);
    }

    if (numMarkers == 1) {
        for (int j = 0; j < 3; ++j)
            tAvg[j] = static_cast<float>(tvecs[0][j]);

        qAvg = quats[0];
    }
    else {
        Vec3d t_sum(0, 0, 0);
        for (const auto& tvec : tvecs)
            t_sum += Vec3d(tvec[0], tvec[1], tvec[2]);

        t_sum = Vec3d(t_sum[0] / numMarkers, t_sum[1] / numMarkers, t_sum[2] / numMarkers);
        for (int j = 0; j < 3; ++j)
            tAvg[j] = static_cast<float>(t_sum[j]);
        averageQuat(quats, qAvg);
    }
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

    for (size_t i = 0; i < 4; i++)
    {
        Quat[i] = Quat[i] / sqrt(Quat[0] * Quat[0] + Quat[1] * Quat[1] + Quat[2] * Quat[2] + Quat[3] * Quat[3]);
    }

    qOut = Quat;
}

void averageQuat(vector<array<float, 4>> &quats, array<float, 4> &qOut) {

}