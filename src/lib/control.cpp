#include <iostream>
#include <algorithm>
#include <fstream>
#include "CPy.hpp"
#include "file.hpp"
#include "dds.hpp"
#include "control.hpp"

using namespace std;

PyObject *toPyList(std::vector<double> &vec) {
    PyObject* pyList = PyList_New(vec.size());
    if (!pyList) {
        std::cerr << "<toPyList> Failed to create PyList\n";
        return nullptr;
    }
    std::cout << "<toPyList> pylist initialized (size: " << vec.size() << ")\n";

    printf("<toPyList> %zu elements waiting to be converted\n", vec.size());
    for (size_t i = 0; i < vec.size(); ++i) {
        PyObject* item = PyFloat_FromDouble(vec[i]);
        if (!item) {
            std::cerr << "<toPyList> Failed to create float at index " << i << "\n";
            Py_DECREF(pyList);
            return nullptr;
        }

        // 设置列表项（偷取item的引用）
        PyList_SetItem(pyList, i, item); 

        // 调试：打印当前状态
        PyObject* pyRepr = PyObject_Repr(pyList);
        if (pyRepr) {
            const char* reprStr = PyUnicode_AsUTF8(pyRepr);
            if (reprStr) {
                printf("<toPyList> After index %zu: %s\n", i, reprStr);
            } else {
                printf("<toPyList> Failed to convert repr to string at index %zu\n", i);
            }
            Py_DECREF(pyRepr);
        } else {
            printf("<toPyList> Failed to get repr at index %zu\n", i);
            PyErr_Clear(); // 清除可能的错误
        }
    }
    return pyList;
}


vector<float> pyListToVector(PyObject* listObj) 
{
    vector<float> result;
    printf("<pyListToVector> C++ vector initialized\n");
    Py_ssize_t len = PyList_Size(listObj);
    result.reserve(len);
    printf("<pyListToVector> pylist size confirmed\n");
    for (Py_ssize_t i = 0; i < len; ++i) 
    {
        PyObject* item = PyList_GetItem(listObj, i);
        printf("<pyListToVector> pylist item %zu taken\n", i);
        if (PyFloat_Check(item)) 
        {
            result.push_back(PyFloat_AsDouble(item));
            printf("<pyListToVector> pylist item %zu pushed into C++ vector\n", i);
        } 
        Py_DECREF(item);
    }
    return result;
}

vector<double> ReadInputData()
{
    vector<double> inputVec(7);
    printf("<ReadInputData> reading pose_data\n ");
    std::ifstream file("/emmc/pose/pose_data", std::ios::binary);
    if (!file.is_open()) 
    {
        std::cerr << "<ReadInputData> Error: could not open pose_data\n " << std::endl;
    }
    else
    {
        printf("<ReadInputData> pose_data opened successfully\n ");
    }
    
    int index0 = 64;
    for (int i = 0; i <7; i++)
    {
        int index = index0 + i*8;
        file.seekg(index, std::ios::beg);
        printf("<ReadInputData> reading data %d at index %d\n ", i, index);
        char buffer[8];
        file.read(buffer, sizeof(buffer));
        double value;
        std::memcpy(&value, buffer, sizeof(double));
        inputVec.push_back(value);
        printf("<ReadInputData> data %d of value %f pushed into inputVec\n ", i, value);
    }
    swap(inputVec[0], inputVec[3]);
    printf("<ReadInputData> quaternions reshaped into standard format\n ");
    return inputVec;
};

void ControlThread() {
    printf("<ControlThread>connected to ControlThread\n");
    const string baseDir = getExecutableDir();
    printf("<ControlThread>python path generated\n");
    PyCaller py(baseDir, "control");
    printf("<ControlThread>connected to control.py\n");
    static bool modelsloaded = false;

    vector<double> inputVec_old(7);
    inputVec_old.assign({0.984536, -0.00566, 0.15888, -0.073557, 0.0002701, -0.000227, 0.0003019});
    while (1)
    {
        printf("<ControlThread>entered loop\n");
        if (controlRunning == 0x01)
        {
            printf("<branch 0x01>entered control branch 0x01\n");

            if (!modelsloaded)
            {
                printf("<branch 0x01>no models\n");
                py.callFunction("load_model");
                printf("<branch 0x01>models initialized\n");
                modelsloaded = true;
            }
            else if (modelsloaded)
            {
                printf("<branch 0x01>models have been loaded\n");
            }

            vector<double> inputVec = ReadInputData();
            printf("<branch 0x01>generated input C++ vector\n");
            std::cout << "<branch 0x01>input is: ";
            for (auto& v : inputVec) {
                std::cout << v << " ";
            }
            std::cout << "\n";
        
            if (inputVec == inputVec_old)
            {
                printf("<branch 0x01>inputVec not updated, thread delaying\n");
                this_thread::sleep_for(chrono::milliseconds(500));
            }
            else
            {
                GILGuard grd;
                printf("<branch 0x01>inputVec updated, execate new output\n");
                PyObject *pyInput = toPyList(inputVec);
                printf("<branch 0x01>C++ vector converted to pylist\n");
                PyObject *args = PyTuple_Pack(1, pyInput); 
                printf("<branch 0x01>pylist packed into tuple\n");
                PyObject *ret = py.callFunctionWithRet("control_mode1", args);
                printf("<branch 0x01>control_mode1 network executed successfully\n");
                Py_DECREF(args);

                vector<float> outputVec;
                if (ret && PyList_Check(ret) && PyList_Size(ret) == 3) 
                {
                    outputVec = pyListToVector(ret);
                    printf("<branch 0x01>pylist output converted to C++ vector successfully\n");
                    // 打印出输出值
                    std::cout << "<branch 0x01>Output is: ";
                    for (auto& v : outputVec) {
                        std::cout << v << " ";
                    }
                    std::cout << "\n";
                } 
                
                Py_DECREF(ret);
                Py_DECREF(pyInput);
                printf("pyObjects decrefed\n");
                {
                    std::lock_guard<std::mutex> lock(ddsMutex);
                    packRotationWheel(outputVec, flyWheel);
                    printf("<branch 0x01>expected RW speed loaded to variable 'flyWheel'\n");
                }
                inputVec_old = inputVec;
            }
        }
        else if (controlRunning == 0x10)
        {
            printf("entered control branch 0x10\n");
            
            if (!modelsloaded)
            {
                printf("<branch 0x10>no models\n");
                py.callFunction("load_model");
                printf("<branch 0x10>models initialized\n");
                modelsloaded = true;
            }
            else if (modelsloaded)
            {
                printf("<branch 0x10>models have been loaded\n");
            }

            vector<double> inputVec = ReadInputData();
            printf("<branch 0x10>generated input C++ vector\n");
            std::cout << "<branch 0x10>input is: ";
            for (auto& v : inputVec) {
                std::cout << v << " ";
            }
            std::cout << "\n";

            if (inputVec == inputVec_old)
            {
                printf("<branch 0x10>inputVec not updated, thread delaying\n");
                this_thread::sleep_for(chrono::milliseconds(500));
            }
            else
            {
                GILGuard grd;
                printf("<branch 0x10>inputVec updated, execate new output\n");
                PyObject *pyInput = toPyList(inputVec);
                printf("<branch 0x10>C++ vector converted to pylist\n");
                PyObject *args = PyTuple_Pack(1, pyInput); 
                printf("<branch 0x10>pylist packed into tuple\n");
                PyObject *ret = py.callFunctionWithRet("control_mode2", args);
                printf("<branch 0x10>control_mode2 network executed successfully\n");
                Py_DECREF(args);

                vector<float> outputVec;
                if (ret && PyList_Check(ret) && PyList_Size(ret) == 4) 
                {
                    outputVec = pyListToVector(ret);
                    printf("<branch 0x10>pylist output converted to C++ vector successfully\n");
                    // 打印出输出值
                    std::cout << "<branch 0x10>Output is: ";
                    for (auto& v : outputVec) {
                        std::cout << v << " ";
                    }
                    std::cout << "\n";
                } 
                
                Py_DECREF(ret);
                Py_DECREF(pyInput);
                printf("<branch 0x10>pyObjects decrefed\n");
                {
                    std::lock_guard<std::mutex> lock(ddsMutex);
                    packRotationWheel(outputVec, flyWheel);
                    printf("<branch 0x10>expected RW speed loaded to variable 'flyWheel'\n");
                }
                inputVec_old = inputVec;
            }
        }
        else if (controlRunning == 0x11)
        {
            printf("entered control branch 0x11\n");

            if (!modelsloaded)
            {
                printf("<branch 0x11>no models\n");
                py.callFunction("load_model");
                printf("<branch 0x11>models initialized\n");
                modelsloaded = true;
            }
            else if (modelsloaded)
            {
                printf("<branch 0x11>models have been loaded\n");
            }

            vector<double> inputVec = ReadInputData();
            printf("<branch 0x11>generated input C++ vector\n");
            std::cout << "<branch 0x11>input is: ";
            for (auto& v : inputVec) {
                std::cout << v << " ";
            }
            std::cout << "\n";

            if (inputVec == inputVec_old)
            {
                printf("<branch 0x11>inputVec not updated, thread delaying\n");
                this_thread::sleep_for(chrono::milliseconds(500));
            }
            else
            {
                GILGuard grd;
                printf("<branch 0x11>inputVec updated, execate new output\n");
                PyObject *pyInput = toPyList(inputVec);
                printf("<branch 0x11>C++ vector converted to pylist\n");
                PyObject *args = PyTuple_Pack(1, pyInput); 
                printf("<branch 0x11>pylist packed into tuple\n");
                PyObject *ret = py.callFunctionWithRet("control_mode3", args);
                printf("<branch 0x11>control_mode3 network executed successfully\n");
                Py_DECREF(args);

                vector<float> outputVec;
                if (ret && PyList_Check(ret) && PyList_Size(ret) == 4) 
                {
                    outputVec = pyListToVector(ret);
                    printf("<branch 0x11>pylist output converted to C++ vector successfully\n");
                    // 打印出输出值
                    std::cout << "<branch 0x11>Output is: ";
                    for (auto& v : outputVec) {
                        std::cout << v << " ";
                    }
                    std::cout << "\n";
                } 
                
                Py_DECREF(ret);
                Py_DECREF(pyInput);
                printf("<branch 0x11>pyObjects decrefed\n");
                {
                    std::lock_guard<std::mutex> lock(ddsMutex);
                    packRotationWheel(outputVec, flyWheel);
                    printf("<branch 0x11>expected RW speed loaded to variable 'flyWheel'\n");
                }
                inputVec_old = inputVec;
            }
        }
        else
        {
            printf("<branch 0x00>No command, thread delaying\n");
            if (modelsloaded)
            {
                py.callFunction("release");
                modelsloaded = false;
                printf("<branch 0x00>Detected occupied resources, auto released.\n");
            }
            this_thread::sleep_for(chrono::seconds(1));
        }
    }
}
