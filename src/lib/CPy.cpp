#include "CPy.hpp"
#include "file.hpp"

using namespace std;

static once_flag py_init;

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
    bool is_owner = false;

    if (!Py_IsInitialized()) {
        Py_Initialize();
        is_owner = true;
    }
    interpreter_owner_ = is_owner;

    // GILGuard grd;


    PyRun_SimpleString("import sys");
    string path_cmd = "sys.path.append('" + module_dir_ + "')";
    PyRun_SimpleString(path_cmd.c_str());

    pModule_ = PyImport_ImportModule(module_name_.c_str());

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
    if (Py_IsInitialized() && interpreter_owner_) {

        // GILGuard grd;
        Py_Finalize();
    }
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

    // GILGuard grd;

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

    // GILGuard grd;

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
