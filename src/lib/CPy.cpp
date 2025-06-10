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
                PyObject *ret = py.callFunctionWithRet("infer_1", args);
                Py_DECREF(args);
                if (ret && PyTuple_Check(ret) && PyTuple_Size(ret) == 3)
                {
                    double confidence = PyFloat_AsDouble(PyTuple_GetItem(ret, 0));

                    PyObject *bbox_list = PyTuple_GetItem(ret, 1); // list of floats
                    PyObject *tmc_list = PyTuple_GetItem(ret, 2); // list of floats

                    static array<float, 4> bbox;
                    static array<float, 3> tmc;
                    static array<float, 3> vel;
                    static array<float, 4> pose;

                    vel.fill(1.0f);

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