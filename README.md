# KotoCord (V1.0)

## 项目简介

本项目旨在开发一款面向 VRChat 无言势及 VTuber 的综合性直播辅助工具。
核心目标是通过集成语音识别（Speech-to-Text, STT）、大语言模型（Large Language Model, LLM）情绪分析与虚拟形象驱动，
将用户的语音或文本输入转化为带有情绪色彩的艺术字字幕，并实时驱动 Live2D/VRM 虚拟形象的表情，从而增强直播的互动性和表现力。

---

## 架构概览

```
┌──────────────────────────────────────────────────────┐
│                    UI 层 (MainWindow)                  │
│              显示字幕、情绪标签、系统监控数据             │
└──────────────┬───────────────────────────┬────────────┘
               │                           │
         字幕帧信号                    CPU/MEM/延迟数据
               │                           │
┌──────────────▼──────────┐   ┌────────────▼───────────┐
│    AppController        │   │  SystemResourceMonitor  │
│  字幕队列 / 视觉锁       │   │  系统资源周期性采样        │
└──────────┬──────────────┘   └────────────────────────┘
           │
    ┌──────┴──────────┐
    │                 │
    ▼                 ▼
┌───────────┐   ┌───────────┐
│  ASR 语音  │   │  LLM 情绪  │
│  识别引擎  │   │  分析引擎  │
│           │   │           │
│ Vosk /    │   │ MockLLM / │
│ Whisper   │   │ DeepSeek  │
└─────┬─────┘   └─────┬─────┘
      │               │
      ▼               ▼
┌───────────┐   ┌───────────┐
│AudioCapture│   │ Kaomoji   │
│ (麦克风)    │   │ Manager   │
└────────────┘   │ (颜文字)   │
                 └───────────┘
```

### 模块划分

| 目录 | 职责 |
|---|---|
| `src/core/` | 核心控制器 (`AppController`) 与公用数据类型 (`DataTypes.h`) |
| `src/ui/` | Qt 主窗口 (`MainWindow`) |
| `src/modules/input/` | 语音识别抽象层：`VoskTranscriber` / `WhisperTranscriber` / `IAudioTranscriber` 接口 |
| `src/modules/capture/` | 音频采集：`AudioCapture` (麦克风) / `AudioFileSimulator` (文件模拟) |
| `src/modules/llm/` | 大语言模型：`DeepSeekAPIWorker` / `MockLLMWorker` / `KaomojiManager` |
| `src/modules/render/` | 字幕渲染：`SubtitleRenderer` |
| `src/modules/system/` | 系统监控：`SystemResourceMonitor` |
| `src/utils/` | 工具类：`AppPaths` (资源路径解析) |

### 数据流

```
麦克风 → AudioCapture → [PCM原始数据]
                           ↓
           VoskTranscriber / WhisperTranscriber
                           ↓
              AppController (队列调度 + 视觉锁)
                           ↓
           MockLLMWorker / DeepSeekAPIWorker
                           ↓
              SubtitleFrame (带回情绪标签)
                           ↓
              MainWindow → 屏幕渲染
```

---

## 项目文件结构

```
Kotocord/
│
├── CMakeLists.txt               # 构建系统 (vcpkg 工具链)
├── CMakePresets.json             # 一键构建配置
├── .gitignore
├── README.md
├── LICENSE
│
├── src/                          # 源码
│   ├── main.cpp                  # 程序入口, 组件装配与信号连线
│   ├── core/
│   │   ├── AppController.h/.cpp  # 核心调度器 (字幕队列 + 视觉锁机制)
│   │   └── DataTypes.h           # SubtitleFrame / EmotionType 定义
│   ├── ui/
│   │   ├── MainWindow.h/.cpp     # Qt 主窗口
│   │   └── MainWindow.ui         # Qt Designer 界面文件
│   ├── utils/
│   │   └── AppPaths.h            # 统一资源路径解析
│   └── modules/
│       ├── input/                # 语音识别 (ASR)
│       │   ├── IAudioTranscriber.h       # ASR 抽象接口
│       │   ├── VoskTranscriber.h/.cpp    # Vosk 引擎 (离线, 轻量)
│       │   └── WhisperTranscriber.h/.cpp # Whisper 引擎 (离线, 高精度)
│       ├── capture/              # 音频采集
│       │   ├── AudioCapture.h/.cpp       # Qt Multimedia 麦克风捕获
│       │   └── AudioFileSimulator.h/.cpp # 文件模拟 (调试用)
│       ├── llm/                  # 大语言模型 & 颜文字
│       │   ├── ILanguageModel.h          # LLM 抽象接口
│       │   ├── DeepSeekAPIWorker.h/.cpp  # DeepSeek API 实现
│       │   ├── MockLLMWorker.h/.cpp      # 本地 Mock 实现 (调试用)
│       │   └── KaomojiManager.h/.cpp     # 颜文字 情绪→表情映射
│       ├── render/
│       │   └── SubtitleRenderer.h/.cpp   # 字幕渲染 (含艺术字效果)
│       └── system/
│           └── SystemResourceMonitor.h/.cpp # CPU/MEM 周期采样
│
├── third_party/                  # 手动管理的第三方库
│   └── vosk/                     # Vosk 不在 vcpkg 中, 需手动准备
│       ├── include/
│       │   └── vosk_api.h
│       └── lib/
│           ├── libvosk.dll       # 运行时动态库
│           └── vosk.lib          # MSVC 导入库
│
├── resources/                    # 项目资源与模型文件
│   ├── kaomoji.json              # 颜文字映射表
│   ├── resources.qrc             # Qt 资源文件 (暂未启用)
│   └── model/                    # [不纳入版本控制]
│       ├── vosk-model-small-cn/  # Vosk 中文模型 (~40MB)
│       └── ggml-small.bin        # Whisper 模型 (~500MB)
│
├── build/                        # [不纳入版本控制] CMake 构建产物
│   ├── msvc-debug/               # Debug 配置
│   └── msvc-release/             # Release 配置
│
└── bin/                          # [不纳入版本控制] 可执行文件输出
    ├── Debug/
    │   ├── Kotocord.exe          # ← 直接运行
    │   └── *.dll                 # 自动部署的运行时依赖
    └── Release/
        ├── Kotocord.exe
        └── *.dll
```

---

## 依赖管理

本项目采取 **vcpkg + 手动兜底** 的混合策略：

| 依赖 | 管理方式 | CMake Target | 说明 |
|---|---|---|---|
| **Qt6 Widgets** | vcpkg (`qtbase`) | `Qt6::Widgets` | 跨平台 UI |
| **Qt6 Multimedia** | vcpkg (`qtmultimedia`) | `Qt6::Multimedia` | 麦克风采集 |
| **whisper.cpp** | vcpkg (`whisper-cpp`) | `whisper` | 高精度语音识别 |
| **ggml** | vcpkg (whisper-cpp 自动拉取) | `ggml::ggml` | whisper 推理后端 |
| **Vosk** | **手动** (`third_party/vosk/`) | `${VOSK_LIB}` | 不在 vcpkg 中 |
| **Vosk 模型** | **手动下载** | N/A | 运行时按路径加载 |
| **Whisper 模型** | **手动下载** | N/A | 运行时按路径加载 |
| **DeepSeek API** | 无本地依赖 | N/A | 纯网络调用 |

> **为什么 Vosk 不用 vcpkg？** Vosk 官方没有提交到 vcpkg registry, 且其 Windows 预编译库使用 MinGW 工具链编译, 与 MSVC 工程直接链接存在 ABI 不兼容风险。保留手动管理可以精确控制导入库的生成方式。

---

## 部署方式 A：vcpkg (推荐)

适合新电脑、有足够磁盘空间 (C 盘 ≥ 50GB 空闲)、希望跨平台一致的场景。

### 前置条件

- Visual Studio 2022 (含 "使用 C++ 的桌面开发" 工作负载)
- Git
- CMake 3.16+

### 步骤 1：安装 vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
```

> **💡 C 盘空间不足？** 如果你想把 vcpkg 放到其他盘（如 D 盘），只需把路径改为 `D:\vcpkg`。但请注意后续 `CMakePresets.json` 中的 `toolchainFile` 路径也需要同步修改。

### 步骤 2：安装项目依赖 (仅首次)

```powershell
cd C:\vcpkg
.\vcpkg install qtbase[windeployqt] qtmultimedia[widgets] whisper-cpp
```

**重要说明 — 编译时间：**

Qt6 需要 **从源码编译**, 首次安装约 **1–3 小时**, 占用 **20–30 GB 磁盘空间**（主要是 `buildtrees/` 编译中间产物）。这是 vcpkg 的"入场费"——只需要付一次。

`windeployqt` 功能会在构建后自动将 Qt6 运行时 DLL 复制到 exe 目录，无需手动操作。

### 步骤 3：准备 Vosk

Vosk 不在 vcpkg 中, 需手动准备。

1. 从 [Vosk Releases](https://github.com/alphacep/vosk-api/releases) 下载 `vosk-win64-x.x.x.zip`
2. 将 `libvosk.dll`, `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll` 放入 `third_party/vosk/lib/`
3. 将 `vosk_api.h` 放入 `third_party/vosk/include/` (项目已包含此头文件)
4. **生成 MSVC 导入库** (Vosk 使用 MinGW 编译, 其 `.lib` 不能直接用于 MSVC)：

```powershell
# 找到 VS2022 的 lib.exe (路径中 MSVC 版本号可能不同)
$libExe = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\lib.exe"

# 创建 .def 文件 (导出 Vosk 的 C API 符号)
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

# 生成 MSVC 兼容的导入库
& $libExe /def:third_party\vosk\lib\libvosk.def /machine:x64 /out:third_party\vosk\lib\vosk.lib
```

> **注意：** CMakeLists.txt 中 `find_library` 查找的是 `vosk` 或 `libvosk`。这里生成 `vosk.lib` 即可被 CMake 识别。

### 步骤 4：下载模型文件

模型文件是运行时按路径加载的，不参与编译。

- **Vosk 中文模型**：从 [alphacephei.com/vosk/models](https://alphacephei.com/vosk/models) 下载 `vosk-model-small-cn-0.22.zip`, 解压至 `resources/model/vosk-model-small-cn-0.22/`
- **Whisper 模型**：从 [huggingface.co/ggerganov/whisper.cpp](https://huggingface.co/ggerganov/whisper.cpp) 下载 `ggml-small.bin`, 放置于 `resources/model/`

### 步骤 5：配置 CMakePresets.json

项目根目录的 `CMakePresets.json` 已预先配置好。**如果你的 vcpkg 不在 `C:\vcpkg`，需要修改 `toolchainFile` 路径。**

```json
{
    "toolchainFile": "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
    //               ^^^^^^^ 改为你的 vcpkg 安装路径
}
```

### 步骤 6：构建

```powershell
# Debug (开发调试用)
cmake --preset msvc-debug
cmake --build build/msvc-debug --config Debug

# Release (发布用)
cmake --preset msvc-release
cmake --build build/msvc-release --config Release
```

### 步骤 7：运行

产物位置：
```
bin/Debug/Kotocord.exe    (Debug)
bin/Release/Kotocord.exe  (Release)
```

**直接双击 exe 即可运行，无需手动拷贝任何 DLL。** `windeployqt` 和 CMake post-build 脚本已自动把所有运行时依赖部署到 exe 所在目录。

```powershell
# 也可以通过命令行启动查看日志
.\bin\Debug\Kotocord.exe
```

---

## 部署方式 B：手动管理 (无 vcpkg)

适合旧电脑（C 盘空间不足、不想花数小时编译 Qt）、使用 Qt 官方安装器的场景。

### 前置条件

- Visual Studio 2022 或 MinGW-w64
- [Qt 6.x 官方安装器](https://www.qt.io/download) (安装时勾选 MSVC 或 MinGW 版本，以及 Multimedia 模块)
- CMake 3.16+

### 步骤 1：准备第三方库

```
third_party/
├── vosk/
│   ├── include/vosk_api.h
│   └── lib/
│       ├── libvosk.dll
│       ├── libvosk.lib          # MinGW 项目直接用这个
│       └── vosk.lib             # MSVC 项目需按方式 A 步骤 3 生成
└── whisper/
    ├── include/
    │   └── whisper.h             # 从 whisper.cpp 项目获取
    └── lib/
        ├── whisper.dll           # 从 whisper.cpp Releases 下载
        ├── whisper.lib
        └── ggml/                 # ggml 头文件与库
            ├── include/          # 全部 ggml-*.h 头文件
            └── lib/              # ggml.dll, ggml-cpu.dll, ggml-base.dll
```

### 步骤 2：构建 (无 CMakePresets)

手动传递 Qt 路径给 CMake：

```powershell
cd Kotocord

# MSVC 示例 (Qt 安装路径按实际情况修改)
cmake -B build -G "Visual Studio 17 2022" `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2019_64" `
  -DCMAKE_CXX_STANDARD=17

cmake --build build --config Debug
```

```bash
# MinGW 示例 (Qt 安装路径按实际情况修改)
cmake -B build -G "MinGW Makefiles" \
  -DCMAKE_PREFIX_PATH="/c/Qt/6.x.x/mingw_64" \
  -DCMAKE_CXX_STANDARD=17

cmake --build build
```

> **注意：** 手动模式下，CMakeLists.txt 中 `find_package(whisper CONFIG)` 会失败（因为没有 vcpkg 提供的 whisper-config.cmake）。你需要改用旧版 CMakeLists.txt（见下述"无 vcpkg 的 CMakeLists.txt 备选"），或自行编写 whisper 的 Find 模块。

---

## 💡 vcpkg 编译时间与加速方案

Qt6 源码编译是 vcpkg 部署中最耗时的环节。以下是实测数据和加速策略：

| 策略 | 效果 | 操作复杂度 |
|---|---|---|
| **什么都不做** (从源码编译) | 基准: 1–3 小时 | 无 |
| **使用二进制缓存 (推荐)** | 秒级安装 (从缓存拉取) | 中 — 需要配置缓存服务 |
| **只装需要的模块** (`qtbase[widgets]` 而非全量 `qt6`) | 减少约 30% 编译量 | 无 (本项目已采用) |
| **多核编译** (vcpkg 默认已开) | 充分利用 CPU | 无 |

### 使用 vcpkg 二进制缓存

vcpkg 支持将已编译的包缓存到本地目录或远程服务。首次编译后，`packages/` 下会保留编译产物。重装系统前备份以下目录：

```
C:\vcpkg\packages\     ← 编译完成的包
C:\vcpkg\archives\     ← vcpkg 自动归档缓存
```

恢复时将这两个目录放回原位，下次 `vcpkg install` 会直接命中缓存，跳过编译。

团队协作时可搭建 NuGet 或 S3 远程缓存，参考 [vcpkg Binary Caching 文档](https://learn.microsoft.com/en-us/vcpkg/users/binarycaching)。

### 使用环境变量加速

```powershell
# 允许 vcpkg 使用更多 CPU 核心编译
$env:VCPKG_MAX_CONCURRENCY = 8

# 如果内存充足, 启用并行安装
$env:VCPKG_INSTALL_OPTIONS = "--x-use-aria2"
```

---

## Qt Creator 集成 vcpkg

即使使用 vcpkg 管理 Qt，你依然可以（也应该）使用 Qt Creator 进行开发，特别是 `.ui` 文件的可视化编辑。

### 配置 Kit

1. 打开 Qt Creator → **编辑 → Preferences → Qt Versions**
2. 点击 **添加**，指向 vcpkg 安装的 qmake：
   ```
   C:\vcpkg\installed\x64-windows\tools\Qt6\bin\qmake.exe
   ```
3. **Kits** 标签页 → **添加**：
   - 名称：`Kotocord (vcpkg MSVC)`
   - Compiler：选 VS2022 的 MSVC
   - Qt version：选步骤 2 注册的版本
   - **CMake Toolchain File**：填 `C:\vcpkg\scripts\buildsystems\vcpkg.cmake`
4. 保存

### 打开项目

`File → Open File or Project... →` 选择 `CMakeLists.txt` → 在 Kit 选择界面勾选 `Kotocord (vcpkg MSVC)` → 完成。

之后 Qt Creator 的所有功能（代码补全、调试、`.ui` 可视化编辑器）都正常工作。Qt Creator 只是 IDE，它通过 Kit 知道去 vcpkg 的安装目录找 Qt，和你用官方 Qt 安装器时的体验完全一样。

---

## 运行时部署 (分发给他人)

开发阶段的 `windeployqt` 已经自动把 DLL 拷贝到 `bin/` 下，你可以直接双击 exe 运行。但如果要把程序发给别人，需要确保以下几点：

### 完整运行包需包含

```
Kotocord/
├── Kotocord.exe
├── *.dll                    # windeployqt + post-build 自动部署
├── resources/
│   ├── kaomoji.json        # 颜文字映射 (必须)
│   └── model/
│       ├── vosk-model-small-cn-0.22/   # Vosk 模型 (必须, ~40MB)
│       └── ggml-small.bin             # Whisper 模型 (必须, ~500MB)
└── platforms/
    └── qwindows.dll         # Qt 平台插件 (windeployqt 自动部署)
```

> **检查清单：**
> - `bin/Debug/Kotocord.exe` 同级是否有一个 `platforms/` 文件夹？如果没有，说明 windeployqt 未正确运行。检查 CMake 输出日志中是否有 "deploying dependencies" 字样。
> - `resources/model/` 下的模型文件必须存在，否则程序启动后 ASR 引擎无法初始化。

### 生成独立安装包

```powershell
# 1. Release 构建
cmake --preset msvc-release
cmake --build build/msvc-release --config Release

# 2. 将 bin/Release/ 整个目录打包
# 其中已包含所有 DLL, 直接压缩为 zip 即可分发
Compress-Archive -Path bin\Release\* -DestinationPath Kotocord-v0.1.0.zip
```

---

## 跨平台构建

CMakeLists.txt 已预留 macOS 和 Linux 的分支逻辑。vcpkg 也支持这两个平台：

```bash
# macOS
brew install cmake
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg && ./bootstrap-vcpkg.sh
./vcpkg install qtbase[windeployqt] qtmultimedia[widgets] whisper-cpp

# Linux (Ubuntu/Debian)
sudo apt install build-essential cmake
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg && ./bootstrap-vcpkg.sh
./vcpkg install qtbase qtmultimedia[widgets] whisper-cpp
```

具体 preset 待后续补充。核心 CMakeLists.txt 代码对平台差异已做处理（DLL/dylib/so 分支、API key 刷新等）。

---

## 开发工作流速查

```powershell
# ===== 日常开发 (假设已完成首次部署) =====

# 改完代码后:
cmake --build build/msvc-debug --config Debug

# 全新 clone 后首次构建:
cmake --preset msvc-debug
cmake --build build/msvc-debug --config Debug

# Release 构建:
cmake --preset msvc-release
cmake --build build/msvc-release --config Release

# 清理重编译:
cmake --build build/msvc-debug --config Debug --clean-first

# 完全重置:
Remove-Item -Recurse -Force build\
cmake --preset msvc-debug
cmake --build build/msvc-debug --config Debug
```

---

## 常见问题

### Q: vcpkg install 报错 "Could not find a package configuration file"

A: 确认 triplet 匹配。Windows MSVC 默认 triplet 是 `x64-windows`。如果使用了其他 triplet，需要在 CMakePresets.json 中设置 `VCPKG_TARGET_TRIPLET`。

### Q: CMake configure 报错 "Vosk library not found"

A: 检查 `third_party/vosk/lib/` 下是否有 `vosk.lib` 或 `libvosk.lib`。如果是 MinGW 版的 `.lib`，需要用 MSVC `lib.exe` 重新生成（见部署方式 A 步骤 3）。

### Q: 运行时提示 "无法加载 Vosk 模型"

A: 检查 `resources/model/vosk-model-small-cn-0.22/` 目录是否存在且包含模型文件（至少应有 `am/` 和 `conf/` 子目录）。路径由 `src/utils/AppPaths.h` 定义。

### Q: 运行时提示 "Whisper 模型未正确初始化"

A: 检查 `resources/model/ggml-small.bin` 文件是否存在。

### Q: `third_party/whisper/` 目录为什么还在？

A: 迁移到 vcpkg 后，`third_party/whisper/` 不再被 CMakeLists.txt 引用。它保留仅仅是为了兼容旧分支或作为头文件参考。确认不需要后可安全删除。

---

## 开源协议与致谢

Kotocord 的诞生离不开以下优秀的开源项目：

| 项目 | 用途 | 协议 |
|---|---|---|
| [Qt 6](https://www.qt.io/) | UI 框架与多媒体处理 | LGPL v3 |
| [Vosk API](https://alphacephei.com/vosk/) | 离线流式语音识别 | Apache 2.0 |
| [Whisper.cpp](https://github.com/ggml-org/whisper.cpp) | 高精度语音识别引擎 | MIT |
| [vcpkg](https://vcpkg.io/) | C/C++ 包管理器 | MIT |

感谢这些伟大的开发者为开源社区做出的贡献！
