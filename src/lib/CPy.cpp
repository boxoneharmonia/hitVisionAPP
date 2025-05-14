#include "CPy.hpp"
#include "file.hpp"
#include "dds.hpp"
using namespace std;

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
    PyCaller py(baseDir, "model");
    py.callFunction("load_model");
    while (1)
    {
        if (visionRunning)
        {
            readIndexFile(imageIndexNew);
            imageIndexNew--;
            if (imageIndexNew > imageIndex)
            {
                imageIndex = imageIndexNew;
                imagePath = folderPath + "/" + to_string(imageIndexNew) + ".bmp";
                PyObject *args = toPyTuple(imagePath);
                PyObject *ret = py.callFunctionWithRet("infer", args);
                Py_DECREF(args);
                if (ret && PyTuple_Check(ret) && PyTuple_Size(ret) == 3)
                {
                    double confidence = PyFloat_AsDouble(PyTuple_GetItem(ret, 0));

                    PyObject *bbox_list = PyTuple_GetItem(ret, 1); // list of floats
                    PyObject *kpts_list = PyTuple_GetItem(ret, 2); // list of floats

                    vector<float> bbox, keypoints;

                    if (PyList_Check(bbox_list))
                    {
                        for (Py_ssize_t i = 0; i < PyList_Size(bbox_list); ++i)
                        {
                            bbox.push_back(PyFloat_AsDouble(PyList_GetItem(bbox_list, i)));
                        }
                    }

                    if (PyList_Check(kpts_list))
                    {
                        for (Py_ssize_t i = 0; i < PyList_Size(kpts_list); ++i)
                        {
                            keypoints.push_back(PyFloat_AsDouble(PyList_GetItem(kpts_list, i)));
                        }
                    }

                    Py_DECREF(bbox_list);
                    Py_DECREF(kpts_list);
                    Py_DECREF(ret);

                    // 提取出有效的11个二维坐标
                    uint8_t i = 0, j = 0;
                    for (i = 0; i < 33; i++)
                    {
                        if (i % 3 == 0)
                        {
                            ;
                        }
                        else
                        {
                            keypoints.at(j) = keypoints.at(i);
                            j++;
                        }
                    }
                    keypoints.resize(22);

                    // 特征点物理坐标
                    std::vector<float> keypoints_3D = {
                        0, 0, 0,
                        1, 0, 0,
                        1, 1, 0,
                        0, 1, 0,
                        1, 0, 0,
                        1, 1, 0,
                        0, 1, 0,
                        1, 0, 0,
                        1, 1, 0,
                        0, 1, 0,
                        1, 0, 0};

                    // 相机内参
                    cv::Mat cameraMatrix = (cv::Mat_<float>(3, 3) << 600.f, 0.f, 320.f,
                                            0.f, 600.f, 240.f,
                                            0.f, 0.f, 1.f);

                    // 畸变参数
                    cv::Mat distCoeffs = (cv::Mat_<float>(5, 1) << -0.1f, // k1
                                          0.05f,                          // k2
                                          0.01f,                          // p1
                                          0.02f,                          // p2
                                          -0.001f);                       // k3

                    // 输出 旋转向量 平移向量
                    cv::Mat rvec, tvec;

                    // 转换为 OpenCV 所需的数据结构
                    std::vector<cv::Point3f> objectPoints;
                    for (size_t i = 0; i + 2 < keypoints_3D.size(); i += 3)
                    {
                        objectPoints.emplace_back(keypoints_3D[i], keypoints_3D[i + 1], keypoints_3D[i + 2]);
                    }

                    std::vector<cv::Point2f> imagePoints;
                    for (size_t i = 0; i + 1 < keypoints.size(); i += 2)
                    {
                        imagePoints.emplace_back(keypoints[i], keypoints[i + 1]);
                    }

                    bool success = cv::solvePnP(objectPoints, imagePoints, cameraMatrix, distCoeffs, rvec, tvec);

                    if (success)
                    {
                        std::cout << "solvePnP 成功！" << std::endl;
                        std::cout << "旋转向量 rvec:\n"
                                  << rvec << std::endl;
                        std::cout << "平移向量 tvec:\n"
                                  << tvec << std::endl;
                    }
                    else
                    {
                        std::cout << "solvePnP 失败！" << std::endl;
                    }

                    cout << "Infer OK: conf=" << confidence << ", bbox.size=" << bbox.size() << ", keypoints.size=" << keypoints.size() << endl;
                    {
                        std::lock_guard<std::mutex> lock(ddsMutex);
                        packDectResult(confidence, bbox, dectResult);
                        packPoseResult(keypoints, poseResult);
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