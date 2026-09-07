# FLUS Console — Cross-platform Edition

跨平台版本的 FLUS（Future Land Use Simulation）控制台程序，基于原 Windows
Visual Studio 工程改造而来，可在 macOS / Linux / Windows 上构建运行。

> ⚠️ **本仓库不是 FLUS 的原作者发布。** 它以 GeoSOS 团队公开发布的
> FLUS console 源码为基础，包含跨平台移植和用于可复现实验的作者修改。
> 基础 ANN–CA 算法来自 GeoSOS；作者修改新增了 `train` / `train-update`
> 命令行入口，并支持 `FLUS_RANDOM_SEED` 对 ANN 和 CA 阶段确定性播种。
> 因此本仓库不能被描述为“算法一行未改”的官方上游版本。

---

## 版权与原始来源

FLUS 模型与本工程基于的 console 版源码由 **中山大学（Sun Yat-sen
University）地理科学与遥感学院 GeoSOS 团队**开源发布，**仅授权用于非商业的
学术研究**（"open-source the model for non-commercial academic research
purposes"，原文见下方"Source"链接）。本仓库继承同样的使用范围。

- **原作者团队**：Prof. Xia Li（李夏）、Prof. Xiaoping Liu（刘小平）等，中山大学
- **联系人**：Prof. Xiaoping Liu — `Liuxp3 [at] mail.sysu.edu.cn`
- **原始下载**：<http://www.geosimulation.cn/FLUS-source-code.html>
  → <http://geosimulation.cn/download/FLUS_source_code/>
- **GeoSOS 主站**：<http://www.geosimulation.cn/>

### 引用要求

任何使用本工程开展研究、发表论文、或产生衍生作品时，**必须**引用原始论文：

> Liu, X., Liang, X., Li, X., Xu, X., Ou, J., Chen, Y., Li, S., Wang, S.,
> Pei, F. (2017). *A future land use simulation model (FLUS) for simulating
> multiple land use scenarios by coupling human and natural effects.*
> **Landscape and Urban Planning**, 168: 94–116.
> <https://doi.org/10.1016/j.landurbplan.2017.09.019>

### 第三方组件版权

- **ALGLIB**（`src/alglib/`）— Copyright (c) Sergey Bochkanov，遵循 GPL v2
  许可（详见 `src/alglib/ap.h` 头部声明）。本仓库捆绑了原工程一同分发的
  ALGLIB 源码，**未做任何修改**。
- **GDAL** — 通过包管理器（vcpkg / Homebrew / apt）作为外部依赖链接，
  不包含在本仓库中。

---

## 与原 Windows 工程的差异

保留 GeoSOS 的 ANN–CA 算法核心，并加入明确标注的可复现实验修改；这些修改
改变命令行入口和随机流，不能再表述为“算法逻辑一行未改”。

| 项目 | 原版本 | 跨平台版本 |
|---|---|---|
| 入口 | `_tmain(int, _TCHAR*[])` | `main(int, char*[])` |
| 预编译头 | `stdafx.h` / `targetver.h` | 已移除 |
| Windows 头 | `#include <windows.h>` | 已删除（4 处） |
| 计时 | `GetTickCount()` (winmm) | `std::chrono::steady_clock`，封装在 `platform_compat.h` 里以同名函数对外，调用点不变 |
| 已弃用流 | `<strstream>` | `<sstream>` |
| 头文件大小写 | `<LinAlg.h>` | `<linalg.h>`（适配 case-sensitive 文件系统） |
| GDAL 路径编码 | `GDAL_FILENAME_IS_UTF8 = NO` | 保留部分旧调用；Windows 构建要求基准目录使用纯 ASCII 路径 |
| `system("pause")` | 有 | 已删除 |
| 构建系统 | `.vcxproj` (MSBuild) | `CMakeLists.txt` |

---

## 目录结构

```
.
├── CMakeLists.txt          # 跨平台构建脚本
├── vcpkg.json              # Windows 上 vcpkg manifest（GDAL）
├── README.md
└── src/
    ├── main.cpp                  # 入口（替代原 FLUS_console.cpp）
    ├── core/                     # FLUS 核心算法
    │   ├── platform_compat.h     # std::chrono 封装的 GetTickCount() 兼容层
    │   ├── TiffDataRead.{h,cpp}
    │   ├── TiffDataWrite.{h,cpp}
    │   ├── nntrain.{h,cpp}
    │   └── simulationprocess.{h,cpp}
    └── alglib/                   # 第三方 ALGLIB 数学库（原工程已捆绑，未改）
        ├── ap.{h,cpp}
        ├── linalg.{h,cpp}
        └── ... (共 17 对源文件 + 空 stdafx.h)
```

---

## 依赖

- **CMake** ≥ 3.15
- **C++14** 编译器（MSVC 19.x / clang 10+ / gcc 9+ 均可）
- **GDAL** ≥ 2.0（API 上向 3.x 兼容，已在 GDAL 3.12 验证）

OpenCV **不是必需的**——只有在编辑 `CMakeLists.txt` 启用 `_RANK` 模式
（patch geometry / connected-component analysis）时才需要 OpenCV，且这一
分支使用了 OpenCV 2 的 `legacy` / `nonfree` 模块（在 OpenCV 4 已被移除），
若要启用需先迁移到 `cv::connectedComponents`。默认配置下 `_RANK` 是关的。

---

## 构建

### macOS

```bash
brew install cmake gdal
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

可执行文件位于 `build/flus_console`。

### Linux (Debian/Ubuntu)

```bash
sudo apt install cmake build-essential libgdal-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Windows (MSVC + vcpkg)

需要 Visual Studio Build Tools 2022 或更新版本（自带 cmake / ninja / vcpkg）：

```powershell
# 在 "x64 Native Tools Command Prompt for VS" 或加载了 VsDevShell 的 PowerShell 中：
$VS = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"  # 路径按你的版本
cmake -S . -B build -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_TOOLCHAIN_FILE="$VS\VC\vcpkg\scripts\buildsystems\vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

vcpkg 第一次会编译 GDAL 及其依赖（约 17 个 ports），耗时 15–30 分钟；
后续构建走二进制缓存，几秒钟完成。

---

## 运行

可执行文件在**当前工作目录**读取配置文件 `CCregionsimlog.txt` 与
`CCregionMakovChain.csv`。把输入数据放进数据目录后再运行：

```bash
cd /path/to/your/data    # 包含 *.tif、CCregionsimlog.txt、CCregionMakovChain.csv
/path/to/build/flus_console
```

原始示例配置在原 GeoSOS 工程的 `FLUS_console/ccsy/` 目录下，本仓库未包含
（避免对原始数据的二次分发）：
- `CCregionsimlog.txt` — 模拟参数主配置
- `CCregiontrainlogCC.txt` — 训练参数主配置
- `CCregionMakovChain.csv` — 多年需求 / Markov 链表

如需，请从 GeoSOS 官网下载原始 `FLUS_source_code` 包获取示例数据。

训练命令可以显式指定训练配置。若要使用同一个网络对另一组、尺寸一致的驱动因子
生成适宜性概率，可额外传入更新表；更新表每行格式为 `原驱动因子序号,新栅格路径`：

```bash
flus_console train CCregiontrainlogCC.txt
flus_console train-update CCregiontrainlogCC.txt update_drivers.csv
```

`train-update` 暴露原始 `NNtrain(config, update)` 接口；它不改变网络结构或
CA 转移规则，但新增的 `FLUS_RANDOM_SEED` 会改变 ANN 采样和 CA 轮盘赌的
随机流。论文应将本程序称为 **GeoSOS-derived FLUS-style ANN–CA console
(author-modified build)**，而不是官方 GeoSOS 可执行文件。

---

## 已验证的构建环境

| 平台 | 编译器 | GDAL | 状态 |
|---|---|---|---|
| Windows 11 + MSVC v144 (VS 18 BuildTools) | cl 19.5x (19.50–19.51) | 3.12.4 (vcpkg) | 已通过；需纯 ASCII 基准路径 |
| macOS arm64 | AppleClang 21.0 | 3.12.3 (conda-forge) | 已通过；提交 `deb0a54` 重建结果与论文内置二进制 SHA-256 一致（作者自证） |
| Linux | gcc / clang | 3.x (apt / conda-forge) | 设计兼容、待物理机验证 |

---

## 随机性与跨平台复现

`FLUS_RANDOM_SEED` 保证同一平台、同一构建环境下的确定性。ANN 采样和 CA
轮盘赌使用 C 标准库 `rand()` / `srand()`；其序列由平台运行库定义，因此同一
整数种子不保证在 macOS、Linux 和 MSVC 上产生相同随机流或逐像元结果。论文基准
的 macOS arm64 与 Windows x86_64 复核显示，七驱动控制的跨平台差异约为
2–4% 有效像元；25 特征匹配模式在 Windows 的三个种子均退化为零变化。

## Abu Dhabi benchmark releases

- `paper-benchmark-flus-v1` points to `deb0a54`, the source provenance for the
  original valid-input benchmark binary.
- `paper-benchmark-flus-v1.1` points to `47e65b3`. It adds fail-closed handling
  for invalid training, driver-update and simulation inputs: an invalid read
  terminates with exit code 3 rather than continuing into an invalid state.

Version v1.1 does not change the valid-input ANN or CA algorithmic path. On
macOS arm64, the v1.1 rebuild reproduced the Abu Dhabi `baseline_7`, seed-31
2023 and 2024 prediction GeoTIFFs byte for byte (SHA-256 respectively
`45a5f319c87599adca867255dfd3699c6477b2fc14bf854197ae878625d86d64` and
`c4ebfa6bc640ab5ebceb003a9253ec63d875c04431048b75d2c25f1728b16287`). The
benchmark manuscript therefore cites v1.1 as the retained source release while
preserving v1 binary provenance in the archived output reports.

## 已知事项

- ALGLIB 在 clang / gcc 下会有 narrowing-conversion 警告，CMake 里已用
  `-Wno-narrowing` 抑制；MSVC 下用 `_CRT_SECURE_NO_WARNINGS` 和 `/utf-8`。
  功能完全不受影响。
- 原 ALGLIB 注释中含 GBK 字符（被 `/utf-8` 检测为非 UTF-8 后会报 C4828
  警告），是良性的，可忽略。
- Windows 上请将基准数据、配置文件和工作目录放在纯 ASCII 路径；仅将配置文件
  保存为 UTF-8 不能修复 GDAL 路径被本地代码页解读的问题。读取失败现在会返回
  非零状态并停止，不再继续执行到空指针访问。
- `_RANK` 分支保持原样，未启用。如需在跨平台环境启用，需把代码迁移到
  OpenCV 4 的 `cv::connectedComponents` API。

---

## 致谢

感谢中山大学 GeoSOS 团队（Prof. Xia Li、Prof. Xiaoping Liu 等）将 FLUS
模型源码开放给学术界。本跨平台版本仅做工程层面的可移植化改造，所有模型
原理、算法实现、参数与精度归原作者所有。
