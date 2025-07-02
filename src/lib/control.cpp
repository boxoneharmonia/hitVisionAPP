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
    for (size_t i = 0; i < vec.size(); ++i) {
        PyObject* item = PyFloat_FromDouble(vec[i]);
        if (!item) {
            std::cerr << "<toPyList> Failed to create float at index " << i << "\n";
            Py_DECREF(pyList);
            return nullptr;
        }

        PyList_SetItem(pyList, i, item); 

    }
    return pyList;
}


vector<float> pyListToVector(PyObject* listObj) 
{
    vector<float> result;
    Py_ssize_t len = PyList_Size(listObj);
    result.reserve(len);
    for (Py_ssize_t i = 0; i < len; ++i) 
    {
        PyObject* item = PyList_GetItem(listObj, i);
        if (PyFloat_Check(item)) 
        {
            result.push_back(PyFloat_AsDouble(item));
        } 
        Py_DECREF(item);
    }
    return result;
}

vector<double> ReadInputData()
{
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
    vector<double> inputVec;
    
    for (int i = 0; i <7; i++)
    {
        int index = index0 + i*8;
        file.seekg(index, std::ios::beg);
        char buffer[8];
        file.read(buffer, sizeof(buffer));
        double value;
        std::memcpy(&value, buffer, sizeof(double));
        inputVec.push_back(value);
    }
    return inputVec;
};

void ControlThread() {
    const string baseDir = getExecutableDir();
    PyCaller py(baseDir, "control");
    static bool modelsloaded = false;

    vector<double> inputVec_old(7);
    inputVec_old.assign({0.984536, -0.00566, 0.15888, -0.073557, 0.0002701, -0.000227, 0.0003019});
    while (controlRunning != 0x00)
    {
        printf("<ControlThread>entered loop, controlRunning=%u\n", controlRunning);
        if (controlRunning == 1)
        {
            printf("<branch 0x01>entered control branch 0x01\n");

            if (!modelsloaded)
            {
                printf("<branch 0x01>no models\n");
                py.callFunction("load_model");
                printf("<branch 0x01>models initialized\n");
                modelsloaded = true;
            }

            vector<double> inputVec = ReadInputData();
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
                printf("<branch 0x01>inputVec updated, execate new output\n");
                PyObject *pyInput = toPyList(inputVec);
                PyObject *args = PyTuple_Pack(1, pyInput); 
                PyObject *ret = py.callFunctionWithRet("control_mode1", args);
                Py_DECREF(args);

                vector<float> outputVec;
                if (ret && PyList_Check(ret) && PyList_Size(ret) == 3) 
                {
                    outputVec = pyListToVector(ret);
                    // 打印出输出值
                    std::cout << "<branch 0x01>Output is: ";
                    for (auto& v : outputVec) {
                        std::cout << v << " ";
                    }
                    std::cout << "\n";
                } 
                
                Py_DECREF(ret);
                Py_DECREF(pyInput);
                {
                    std::lock_guard<std::mutex> lock(ddsMutex);
                    packRotationWheel(outputVec, flyWheel);
                    printf("<branch 0x11>flywheel is: ");
                    for (int j = 0; j < 4; j++)
                    {
                        printf("%u ", flyWheel[j]);
                    }
                    std::cout << "\n";
                }
                inputVec_old = inputVec;
            }
        }
        else if (controlRunning == 2)
        {
            printf("entered control branch 0x10\n");
            
            if (!modelsloaded)
            {
                printf("<branch 0x10>no models\n");
                py.callFunction("load_model");
                printf("<branch 0x10>models initialized\n");
                modelsloaded = true;
            }

            vector<double> inputVec = ReadInputData();
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
                printf("<branch 0x10>inputVec updated, execate new output\n");
                PyObject *pyInput = toPyList(inputVec);
                PyObject *args = PyTuple_Pack(1, pyInput); 
                PyObject *ret = py.callFunctionWithRet("control_mode2", args);
                Py_DECREF(args);

                vector<float> outputVec;
                if (ret && PyList_Check(ret) && PyList_Size(ret) == 4) 
                {
                    outputVec = pyListToVector(ret);
                    // 打印出输出值
                    std::cout << "<branch 0x10>Output is: ";
                    for (auto& v : outputVec) {
                        std::cout << v << " ";
                    }
                    std::cout << "\n";
                } 
                
                Py_DECREF(ret);
                Py_DECREF(pyInput);
                {
                    std::lock_guard<std::mutex> lock(ddsMutex);
                    packRotationWheel(outputVec, flyWheel);
                    printf("<branch 0x11>flywheel is: ");
                    for (int j = 0; j < 4; j++)
                    {
                        printf("%u ", flyWheel[j]);
                    }
                    std::cout << "\n";
                }
                inputVec_old = inputVec;
            }
        }
        else if (controlRunning == 3)
        {
            printf("entered control branch 0x11\n");

            if (!modelsloaded)
            {
                printf("<branch 0x11>no models\n");
                py.callFunction("load_model");
                printf("<branch 0x11>models initialized\n");
                modelsloaded = true;
            }

            vector<double> inputVec = ReadInputData();
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
                printf("<branch 0x11>inputVec updated, execate new output\n");
                PyObject *pyInput = toPyList(inputVec);
                PyObject *args = PyTuple_Pack(1, pyInput); 
                PyObject *ret = py.callFunctionWithRet("control_mode3", args);
                Py_DECREF(args);

                vector<float> outputVec;
                if (ret && PyList_Check(ret) && PyList_Size(ret) == 4) 
                {
                    outputVec = pyListToVector(ret);
                    // 打印出输出值
                    std::cout << "<branch 0x11>Output is: ";
                    for (auto& v : outputVec) {
                        std::cout << v << " ";
                    }
                    std::cout << "\n";
                } 
                
                Py_DECREF(ret);
                Py_DECREF(pyInput);
                {
                    std::lock_guard<std::mutex> lock(ddsMutex);
                    packRotationWheel(outputVec, flyWheel);
                    printf("<branch 0x11>flywheel is: ");
                    for (int j = 0; j < 4; j++)
                    {
                        printf("%u", flyWheel[j]);
                    }
                    std::cout << "\n";
                }
                inputVec_old = inputVec;
            }
        }
    }
    printf("<ControlThread>No command, thread shutdown\n");
    if (modelsloaded)
    {
        py.callFunction("release");
        modelsloaded = false;
        printf("<branch 0x00>Detected occupied resources, auto released.\n");
    }
}
