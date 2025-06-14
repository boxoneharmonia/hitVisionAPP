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
    static array<float, 4> bbox;
    static array<float, 3> tmc;
    static array<float, 3> vel;
    static array<float, 4> pose;
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

PyObject* toPyList(const vector<float>& vec) 
{
    PyObject* pyList = PyList_New(vec.size());
    printf("<toPyList> pylist initialized");
    for (size_t i = 0; i < vec.size(); ++i) 
    {
        PyObject* item = PyFloat_FromDouble(vec[i]);
        PyList_SetItem(pyList, i, item); 
        Py_DECREF(item);
        printf("<toPyList> C++ vector item %d converted", i);
    }
    return pyList;
}

vector<float> pyListToVector(PyObject* listObj) 
{
    vector<float> result;
    printf("<pyListToVector> C++ vector initialized");
    Py_ssize_t len = PyList_Size(listObj);
    result.reserve(len);
    printf("<pyListToVector> pylist size confirmed");
    for (Py_ssize_t i = 0; i < len; ++i) 
    {
        PyObject* item = PyList_GetItem(listObj, i);
        printf("<pyListToVector> pylist item %d taken", i);
        if (PyFloat_Check(item)) 
        {
            result.push_back(PyFloat_AsDouble(item));
            printf("<pyListToVector> pylist item %d pushed into C++ vector", i);
        } 
        Py_DECREF(item);
    }
    return result;
}

void ControlThread() {
    printf("connected to ControlThread");
    const string baseDir = getExecutableDir();
    PyCaller py(baseDir, "control");
    printf("connected to control.py");
    py.callFunction("load_model");
    printf("models have been loaded");
    while (1)
    {
        if (controlRunning == 0x10)
        {
            printf("entered control branch");
            vector<float> inputVec(7);
            inputVec.assign({0.984536f, -0.00566f, 0.15888f, -0.073557f, 0.0002701f, -0.000227f, 0.0003019f});
            printf("generated input C++ vector");
            PyObject *pyInput = toPyList(inputVec);
            printf("C++ vector converted to pylist");
            PyObject *args = PyTuple_Pack(1, pyInput); 
            printf("pylist packed into tuple");
            PyObject *ret = py.callFunctionWithRet("control_mode2", args);
            printf("network executed successfully");
            Py_DECREF(args);

            vector<float> outputVec;
            if (ret && PyList_Check(ret) && PyList_Size(ret) == 4) 
            {
                outputVec = pyListToVector(ret);
                printf("pylist output converted to C++ vector successfully");
            } 
            
            Py_DECREF(ret);
            Py_DECREF(pyInput);
            printf("pycaller decrefed");
            {
                std::lock_guard<std::mutex> lock(ddsMutex);
                packFlyWheel(outputVec, flyWheel);
            }
        }
        else
        {
            this_thread::sleep_for(chrono::seconds(1));
        }
    }
}