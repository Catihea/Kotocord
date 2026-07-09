# KotoCord (V1.0)

面向 VRChat 无言势及 VTuber 的综合性直播辅助工具。集成语音识别（STT）、大语言模型（LLM）情绪分析与虚拟形象驱动，将语音/文本输入转化为带有情绪色彩的艺术字字幕。

---

## 目录

- [架构概览](#架构概览)
- [项目文件结构](#项目文件结构)
- [两种部署模式](#两种部署模式)
- [前置条件](#前置条件)
- [模式 A：vcpkg 自动管理 (新 PC / CI)](#模式-avcpkg-自动管理)
- [模式 B：手动 Qt + third_party (旧 PC / 已有 Qt)](#模式-b手动-qt--third_party)
- [构建与运行](#构建与运行)
- [Qt Creator 集成](#qt-creator-集成)
- [运行时部署 (分发给他人)](#运行时部署)
- [vcpkg 编译提速](#vcpkg-编译提速)
- [跨平台](#跨平台)
- [常见问题](#常见问题)
- [致谢](#致谢)

---

## 架构概览

```
麦克风 → AudioCapture → [VoskTranscriber / WhisperTranscriber]
                                    ↓
                            AppController (队列 + 视觉锁)
                                    ↓
                 [MockLLMWorker / DeepSeekAPIWorker]
                                    ↓
                      SubtitleFrame (情绪 + 文本)
                                    ↓
                              MainWindow → 屏幕渲染
```

### 模块职责

| 目录 | 职责 |
|---|---|
| `src/core/` | `AppController` 核心调度、`DataTypes.h` 公用类型 |
| `src/ui/` | Qt 主窗口 (含 `.ui` 设计师文件) |
| `src/modules/input/` | ASR 抽象层 (`IAudioTranscriber`) + Vosk/Whisper 实现 |
| `src/modules/capture/` | 音频采集 (`AudioCapture` 麦克风 / `AudioFileSimulator` 调试) |
| `src/modules/llm/` | LLM 抽象层 + DeepSeek API / Mock + `KaomojiManager` 颜文字 |
| `src/modules/render/` | 字幕渲染 (`SubtitleRenderer`) |
| `src/modules/system/` | 系统资源监控 (`SystemResourceMonitor`) |
| `src/utils/` | `AppPaths` 统一资源路径 |

---

## 项目文件结构

```
Kotocord/
├── CMakeLists.txt                     # 双模式构建脚本 (提交)
├── CMakePresets.json.example          # Preset 模板 (提交, 新人复制用)
├── CMakePresets.json                  # 本地 Preset (不提交, 每人自己的)
├── CMakeUserPresets.json              # 用户级覆盖 (可选, 不提交)
├── .gitignore
├── README.md
├── LICENSE
│
├── src/                               # 源码 — 与部署模式零耦合
│   ├── main.cpp
│   ├── core/
│   ├── ui/
│   ├── utils/
│   └── modules/
│       ├── input/                     # ASR 引擎
│       ├── capture/                   # 音频捕获
│       ├── llm/                       # LLM + 颜文字
│       ├── render/                    # 字幕渲染
│       └── system/                    # 系统监控
│
├── third_party/
│   └── vosk/                          # 手动依赖 (两种模式都需要)
│       ├── include/vosk_api.h
│       └── lib/                       # [不提交] DLL + .lib
│           ├── libvosk.dll
│           └── vosk.lib               # MSVC 导入库
│
├── resources/
│   ├── kaomoji.json                   # 颜文字映射表 (提交)
│   ├── resources.qrc
│   └── model/                         # [不提交] 语音模型
│       ├── vosk-model-small-cn-0.22/
│       └── ggml-small.bin
│
├── build/                             # [不提交] CMake 构建中间产物
├── bin/                               # [不提交] 可执行文件输出
│   ├── Debug/Kotocord.exe
│   └── Release/Kotocord.exe
│
└── .git/                              # [不提交] 版本控制
```

---

## 两种部署模式

本项目用**一个 CMakeLists.txt + 一个 `USE_VCPKG` 选项**支持两种环境，免除了分支维护的成本。

你的旧设备和新设备 clone 的是同一份代码。差异只在一处：

```
CMakeLists.txt 第 ~30 行:
─────────────────────────────────────
option(USE_VCPKG "..." ON)   ← 默认 vcpkg 模式

if(USE_VCPKG)
    find_package(whisper CONFIG)   # vcpkg 提供
else()
    find_library(WHISPER_LIB ...)  # third_party/whisper/ 提供
endif()
─────────────────────────────────────
src/ 目录下的所有源码、resources/、.gitignore 在两个模式间完全一样。
```

> **不推荐用分支拆分部署方式。** 分支适用的是代码级分叉（feature 开发），配置级差异用 `option()` + 本地 Preset 解决。

### 模式速查

| | 模式 A: vcpkg | 模式 B: 手动 |
|---|---|---|
| **适用场景** | 全新 PC、CI 构建机、跨平台 | 旧 PC、C 盘空间不足、已有 Qt 安装 |
| **Qt6 来源** | vcpkg 从源码编译 | 官方安装器 / 系统包管理器 |
| **whisper.cpp** | vcpkg 管理，版本锁定 | `third_party/whisper/` 手动放置 |
| **首次耗时** | 1–3 小时 | 10 分钟 |
| **磁盘占用** | 20–30 GB | ~2 GB |
| **Preset 例子** | `msvc-debug` (vcpkg) | `msvc-debug-manual` |
| **`USE_VCPKG`** | `ON` | `OFF` |

---

## 前置条件

两种模式都需要：

- **Git**
- **CMake 3.16+**
- **编译器** — 以下任选一种：
  - Visual Studio 2022 (含 "使用 C++ 的桌面开发" 工作负载)
  - MinGW-w64 (通过 MSYS2 或 Qt 安装器附带)
- **Vosk 预编译库** — `third_party/vosk/lib/` 下需有 `libvosk.dll` 和 MSVC 导入库 `vosk.lib`。详见 [模式 A 步骤 3](#3-准备-vosk-两种模式都需要)。
- **语音模型文件**：
  - Vosk 中文模型 → `resources/model/vosk-model-small-cn-0.22/`
  - Whisper 模型 → `resources/model/ggml-small.bin`

---

## 模式 A：vcpkg 自动管理

### 1. 安装 vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
```

> **C 盘空间不足？** 克隆到其他盘即可（如 `D:\vcpkg`），但需同步修改 Preset 中的 `toolchainFile` 路径。

### 2. 安装项目依赖 (仅首次, 约 1–3 小时)

```powershell
cd C:\vcpkg
.\vcpkg install qtbase[windeployqt] qtmultimedia[widgets] whisper-cpp
```

Qt6 需要从源码编译，首次很慢。详见 [vcpkg 编译提速](#vcpkg-编译提速)。

### 3. 准备 Vosk (两种模式都需要)

Vosk 不在 vcpkg 中，必须手动准备。

**下载：**

从 [Vosk Releases](https://github.com/alphacep/vosk-api/releases) 下载 `vosk-win64-0.3.45.zip`（或更新版本）。

**解压后放入 `third_party/vosk/lib/` 的文件：**

| 文件 | 说明 |
|---|---|
| `libvosk.dll` | Vosk 运行时库 |
| `libgcc_s_seh-1.dll` | MinGW GCC 运行时 |
| `libstdc++-6.dll` | MinGW C++ 标准库 |
| `libwinpthread-1.dll` | MinGW POSIX 线程库 |

**生成 MSVC 导入库：**

Vosk 是 MinGW 编译的，附带 `.lib` 无法用于 MSVC。需用 VS 的 `lib.exe` 重新生成：

```powershell
# 创建 DEF 文件 (Vosk C API 符号列表)
@"
EXPORTS
vosk_model_new
vosk_model_free
vosk_model_find_word
vosk_recognizer_new
vosk_recognizer_new_grm
vosk_recognizer_new_spk
vosk_recognizer_free
vosk_recognizer_set_max_alternatives
vosk_recognizer_set_words
vosk_recognizer_set_partial_words
vosk_recognizer_set_grm
vosk_recognizer_set_spk_model
vosk_recognizer_accept_waveform
vosk_recognizer_result
vosk_recognizer_partial_result
vosk_recognizer_final_result
vosk_recognizer_reset
vosk_set_log_level
vosk_gpu_init
vosk_gpu_thread_init
vosk_free
"@ | Out-File -FilePath "third_party\vosk\lib\libvosk.def" -Encoding ascii

# 用 MSVC lib.exe 生成导入库
& "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\lib.exe" `
    /def:third_party\vosk\lib\libvosk.def /machine:x64 /out:third_party\vosk\lib\vosk.lib
```

> **MinGW 用户**：如果使用 MinGW 编译器（模式 B + `mingw-debug-manual` Preset），可以直接使用 zip 中附带的 `libvosk.lib`，不需要上述导入库生成步骤。

### 4. 配置本地 Preset

```powershell
cp CMakePresets.json.example CMakePresets.json
```

编辑 `CMakePresets.json`，确认 `toolchainFile` 指向你的 vcpkg 路径。模板中的预设名 `msvc-debug-vcpkg` 和 `msvc-release-vcpkg` 可直接使用。

### 5. 构建与运行

```powershell
# Debug
cmake --preset msvc-debug-vcpkg
cmake --build build/msvc-debug-vcpkg --config Debug

# Release
cmake --preset msvc-release-vcpkg
cmake --build build/msvc-release-vcpkg --config Release
```

产物位置：`bin/Debug/Kotocord.exe` 或 `bin/Release/Kotocord.exe`。直接双击运行即可，所有 DLL 已在构建时自动部署。

---

## 模式 B：手动 Qt + third_party

此模式不依赖 vcpkg，适合旧电脑或已有 Qt 安装的设备。**同一份代码，只在 Preset 中改一个选项。**

### 1. third_party 目录结构

需在项目根目录下手动准备：

```
third_party/
├── vosk/
│   ├── include/vosk_api.h              ← 提交
│   └── lib/                            ← 不提交
│       ├── libvosk.dll
│       └── vosk.lib                    (MSVC) 或 libvosk.lib (MinGW)
│
└── whisper/                            ← 不提交
    ├── include/
    │   └── whisper.h
    ├── lib/
    │   ├── whisper.dll
    │   └── whisper.lib
    └── ggml/
        ├── include/
        │   └── ggml.h, ggml-cpu.h ...  (全部 ggml 头文件)
        └── lib/
            ├── ggml.dll
            ├── ggml-cpu.dll
            ├── ggml-base.dll
            └── ggml.lib
```

### 2. 安装 Qt6

通过 [Qt 官方安装器](https://www.qt.io/download) 安装，选择 MSVC 或 MinGW 版本，勾选 **Multimedia** 模块。记录安装路径，例如 `C:\Qt\6.5.0\msvc2019_64`。

### 3. 配置本地 Preset

```powershell
cp CMakePresets.json.example CMakePresets.json
```

编辑 `CMakePresets.json`，**保留手动模式的预设**，删掉不需要的。关键是将 `CMAKE_PREFIX_PATH` 指向你的 Qt 安装目录：

```json
{
    "name": "msvc-debug-manual",
    "cacheVariables": {
        "USE_VCPKG": "OFF",
        "CMAKE_PREFIX_PATH": "C:/Qt/6.5.0/msvc2019_64"   // ← 改成你的路径
    }
}
```

> **旧电脑上已有旧 Preset？** 直接复制模板中的 `msvc-debug-manual` 预设块，粘贴到现有 `CMakePresets.json` 中即可。`CMakePresets.json` 支持多个预设并列。

### 4. 构建与运行

```powershell
# MSVC 手动模式
cmake --preset msvc-debug-manual
cmake --build build/msvc-debug-manual --config Debug

# MinGW 手动模式 (如果装了 MinGW 版 Qt)
cmake --preset mingw-debug-manual
cmake --build build/mingw-debug-manual
```

> **注意：** 手动模式下 whisper/ggml/Vosk 的 DLL 会自动从 `third_party/` 拷贝到输出目录。但 **Qt 平台插件**（`platforms/qwindows.dll`）不会自动部署。建议在 Qt Creator 中运行（它会自动处理），或手动运行 windeployqt：
> ```powershell
> C:\Qt\6.5.0\msvc2019_64\bin\windeployqt.exe bin\Debug\Kotocord.exe
> ```

---

## 构建与运行

### 所有可用预设

| 预设名 | 模式 | 编译器 |
|---|---|---|
| `msvc-debug-vcpkg` | vcpkg | MSVC |
| `msvc-release-vcpkg` | vcpkg | MSVC |
| `msvc-debug-manual` | 手动 | MSVC |
| `msvc-release-manual` | 手动 | MSVC |
| `mingw-debug-manual` | 手动 | MinGW |

### 命令速查

```powershell
# 首次 configure
cmake --preset <预设名>

# 后续 build (增量编译, 很快)
cmake --build build/<目录> --config Debug

# 清理重编译
cmake --build build/<目录> --config Debug --clean-first

# 完全重置 (删 build 目录重来)
Remove-Item -Recurse -Force build\<目录>
cmake --preset <预设名>
cmake --build build/<目录> --config Debug
```

### 产物位置

```
bin/
├── Debug/
│   ├── Kotocord.exe
│   ├── *.dll               ← 自动部署
│   ├── platforms/
│   │   └── qwindowsd.dll   ← Qt 平台插件 (必须)
│   ├── styles/
│   ├── imageformats/
│   ├── tls/
│   └── multimedia/
└── Release/
    └── ... (同上)
```

---

## Qt Creator 集成

无论哪种模式，都可以用 Qt Creator 做开发和 `.ui` 可视化编辑。

### vcpkg 模式的 Kit 配置

1. **Qt Creator → 编辑 → Preferences → Qt Versions → 添加**
   - qmake 路径：`C:\vcpkg\installed\x64-windows\tools\Qt6\bin\qmake.exe`

2. **Kits → 添加**
   - 名称：`Kotocord (vcpkg)`
   - Qt version：选步骤 1 的版本
   - CMake Toolchain File：`C:\vcpkg\scripts\buildsystems\vcpkg.cmake`

3. **打开项目**：`File → Open → CMakeLists.txt → 选 Kotocord (vcpkg)`

### 手动模式的 Kit 配置

Kit 指向你的官方 Qt 安装路径。不需要设置 toolchain file。

> **旧电脑 Kit 不变**：你在旧电脑上已有的 Qt Creator Kit 不需要改动。clone 代码后，只需在 `CMakePresets.json` 中把 `USE_VCPKG` 设为 `OFF`，选择你已有的 Kit 即可。

---

## 运行时部署

### 直接运行 (开发)

构建完成后，exe 同级目录已包含所有 DLL + 插件。双击即可运行。

### 分发给他人

```
Kotocord-v0.1.0/
├── Kotocord.exe
├── *.dll                       # 构建自动部署
├── platforms/                  # Qt 平台插件 (必须)
│   └── qwindows.dll
├── styles/                     # Windows 风格
├── imageformats/               # 图片格式
├── tls/                        # HTTPS (DeepSeek API)
├── multimedia/                 # 音频采集
└── resources/
    ├── kaomoji.json            # 颜文字映射 (必须)
    └── model/
        ├── vosk-model-small-cn-0.22/   # Vosk 模型 (必须, ~40MB)
        └── ggml-small.bin             # Whisper 模型 (必须, ~500MB)
```

**检查清单：**
- [ ] exe 同级有 `platforms/qwindows.dll`（不是 `qwindowsd.dll`，Release 没有 `d` 后缀）
- [ ] `resources/model/` 下有模型文件
- [ ] 在未安装 Qt 的裸机上测试过双击运行

打包发布（Release）：

```powershell
cmake --preset msvc-release-vcpkg   # 或 msvc-release-manual
cmake --build build/msvc-release-vcpkg --config Release
# exe 在 bin/Release/，整个目录压缩即可分发
```

---

## vcpkg 编译提速

| 策略 | 效果 | 操作 |
|---|---|---|
| **二进制缓存 (推荐)** | 二次安装秒级完成 | 备份 `C:\vcpkg\packages\` 和 `C:\vcpkg\archives\`，重装后放回 |
| **只装需要的模块** | 减少约 30% 编译量 | 本项目已采用 (`qtbase[widgets]` 而非全量 `qt6`) |
| **多核编译** | 充分利用 CPU | vcpkg 默认开启，无需配置 |
| **NuGet / S3 远程缓存** | 团队共享编译产物 | 参考 [vcpkg Binary Caching](https://learn.microsoft.com/en-us/vcpkg/users/binarycaching) |

---

## 跨平台

CMakeLists.txt 已预留 macOS 和 Linux 分支（DLL/dylib/so 拷贝逻辑、API 刷新等）。vcpkg 在 macOS 和 Linux 上的使用方式相同：

```bash
# macOS
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg && ./bootstrap-vcpkg.sh
./vcpkg install qtbase qtmultimedia[widgets] whisper-cpp

# Linux
sudo apt install build-essential cmake  # 或等效
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg && ./bootstrap-vcpkg.sh
./vcpkg install qtbase qtmultimedia[widgets] whisper-cpp
```

跨平台 Preset 待后续补充。

---

## 常见问题

### Q: "no Qt platform plugin" 错误

A: `platforms/qwindows.dll` 缺失。确认 bin 目录下有 `platforms/` 文件夹。vcpkg 模式下构建会自动部署；手动模式下请运行一次 `windeployqt`。

### Q: CMake configure 报 "Vosk library not found"

A: `third_party/vosk/lib/` 下缺少 `vosk.lib`。MSVC 需用 `lib.exe` 从 MinGW DLL 生成导入库（见模式 A 步骤 3）。

### Q: 运行时 "无法加载 Vosk 模型"

A: `resources/model/vosk-model-small-cn-0.22/` 不存在或路径不对。检查 `src/utils/AppPaths.h` 中的路径定义。

### Q: 运行时 "Whisper 模型未正确初始化"

A: `resources/model/ggml-small.bin` 不存在。

### Q: `CMakePresets.json` 修改后为什么 git 不追踪了？

A: 这是设计如此。`.gitignore` 忽略了 `CMakePresets.json`。每个开发者从 `CMakePresets.json.example` 复制后自行修改。模板文件 `.example` 是提交的。

### Q: 旧电脑已有可用的 Qt Creator 工程，clone 后会冲突吗？

A: 不会。你的 `.pro.user` 文件在 `.gitignore` 中。clone 后在新电脑上直接用 Qt Creator 打开 `CMakeLists.txt`，选择对应的 Kit 即可。

### Q: vcpkg 安装的 Qt 和官方 Qt 可以共存吗？

A: 可以。它们在不同目录下（`C:\vcpkg\installed\` vs `C:\Qt\`），互不干扰。Kit 配置决定了用哪个。

---

## 致谢

| 项目 | 用途 | 协议 |
|---|---|---|
| [Qt 6](https://www.qt.io/) | UI 框架与多媒体处理 | LGPL v3 |
| [Vosk API](https://alphacephei.com/vosk/) | 离线流式语音识别 | Apache 2.0 |
| [Whisper.cpp](https://github.com/ggml-org/whisper.cpp) | 高精度语音识别引擎 | MIT |
| [vcpkg](https://vcpkg.io/) | C/C++ 包管理器 | MIT |
