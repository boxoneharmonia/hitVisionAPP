#ifndef CPY_HPP
#define CPY_HPP

#include <Python.h>
#include <string>
#include <vector>

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
    bool interpreter_owner_;
};

class GILGuard {
public:
    GILGuard() { gstate_ = PyGILState_Ensure(); }
    ~GILGuard() { PyGILState_Release(gstate_); }
private:
    PyGILState_STATE gstate_;
};

PyObject* toPyTuple(const std::vector<std::string>& vec);
PyObject* toPyTuple(const std::string& str);

#endif // CPY_HPP
