<details>
<summary>Index</summary>

- [Introduction](#Introduction)
- [Features](#Features)

</details>

## Introduction
这是一个基于C++/Python设计的Nvidia Jetson Xavier XN平台部署的视觉导航+智能控制软件

## 功能介绍
- 功能1：与HIKVISION千兆相机MV-CS013-GN通信，设置相机和取流，获取图像数据
- 功能2：输入目标图像数据，通过tensorRT运行图像处理，输出目标视觉导航信息
- 功能3：输入本体姿态信息，通过tensorRT运行智能控制算法，输出控制量

![图片](./img.png)
