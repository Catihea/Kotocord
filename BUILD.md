# 构建与部署指南

## 目录

- [两种构建模式](#两种构建模式)
- [前置条件 (两种模式共用)](#前置条件)
- [模式 A：vcpkg 全自动管理](#模式-avcpkg-全自动管理)
- [模式 B：手动 Qt + third_party](#模式-b手动-qt--third_party)
- [CMake 预设参考](#cmake-预设参考)
- [构建产物](#构建产物)
- [分发给他人 (打包)](#分发给他人)
- [vcpkg 编译提速](#vcpkg-编译提速)
- [跨平台](#跨平台)
- [常见问题](#常见问题)

---

## 两种构建模式

项目使用 **一份 CMakeLists.txt + 一个 `USE_VCPKG` 开关** 支持两种环境。不需要维护多个分支。

| | 模式 A: vcpkg | 模式 B: 手动 |
|---|---|---|
| **适用场景** | 全新 PC、CI 构建、跨平台 | 旧 PC、已有 Qt 安装、C 盘空间不足 |
| **Qt6 来源** | vcpkg 从源码编译 | 官方安装器 |
| **whisper.cpp** | vcpkg 管理，版本锁定 | `third_party/whisper/` 手动放置 |
| **首次耗时** | 1–3 小时 | 10 分钟 |
| **磁盘占用** | 20–30 GB | ~2 GB |
| **`USE_VCPKG`** | `ON` (默认) | `OFF` |

---

## 前置条件 (两种模式共用)

- **Git**
- **CMake 3.16+**
- **编译器**：Visual Studio 2022 (含 "使用 C++ 的桌面开发") 或 MinGW-w64
- **Vosk 预编译库** — `third_party/vosk/lib/` 下需有运行时 DLL 和导入库
- **语音模型文件**：
  - Vosk 中文模型 → `resources/model/vosk-model-small-cn-0.22/`
  - Whisper 模型 → `resources/model/ggml-small.bin`

---

## 模式 A：vcpkg 全自动管理

### A1. 安装 vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
```

> C 盘空间不足？克隆到 D 盘即可。之后修改 `CMakePresets.json` 中的 `toolchainFile` 路径。

### A2. 安装项目依赖 (仅首次，约 1–3 小时)

```powershell
cd C:\vcpkg
.\vcpkg install qtbase[windeployqt] qtmultimedia[widgets] whisper-cpp
```

### A3. 准备 Vosk

Vosk 不在 vcpkg 中，需手动下载。

1. 从 [Vosk Releases](https://github.com/alphacep/vosk-api/releases) 下载 `vosk-win64-0.3.45.zip`
2. 解压后，将以下文件放入 `third_party/vosk/lib/`：

| 文件 | 说明 |
|---|---|
| `libvosk.dll` | Vosk 运行时 |
| `libgcc_s_seh-1.dll` | MinGW GCC 运行时 |
| `libstdc++-6.dll` | MinGW C++ 标准库 |
| `libwinpthread-1.dll` | MinGW POSIX 线程库 |

3. **生成 MSVC 导入库**（Vosk 是 MinGW 编译的，自带 `.lib` 不能用于 MSVC 链接器）：

```powershell
# 创建 DEF 文件
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

# 用 MSVC lib.exe 生成
& "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\lib.exe" `
    /def:third_party\vosk\lib\libvosk.def /machine:x64 /out:third_party\vosk\lib\vosk.lib
```

> **MinGW 用户**：使用 MinGW 编译器时可直接用 zip 中的 `libvosk.lib`，不需要上述步骤。

### A4. 下载模型文件

- **Vosk 模型**：[vosk-model-small-cn-0.22.zip](https://alphacephei.com/vosk/models) → 解压到 `resources/model/vosk-model-small-cn-0.22/`
- **Whisper 模型**：[ggml-small.bin](https://huggingface.co/ggerganov/whisper.cpp) → 放到 `resources/model/`

### A5. 配置并构建

```powershell
cp CMakePresets.json.example CMakePresets.json
# 编辑 CMakePresets.json，确认 toolchainFile 路径正确

# Debug
cmake --preset msvc-debug-vcpkg
cmake --build build/msvc-debug-vcpkg --config Debug

# Release
cmake --preset msvc-release-vcpkg
cmake --build build/msvc-release-vcpkg --config Release
```

产物在 `bin/Debug/Kotocord.exe` 或 `bin/Release/Kotocord.exe`。所有 DLL（Qt、whisper、ggml、Vosk）和 Qt 插件目录（platforms、styles、imageformats、tls、multimedia）已在构建时自动部署到 exe 同级目录，双击即可运行。

---

## 模式 B：手动 Qt + third_party

不依赖 vcpkg。适合已有 Qt 安装的旧电脑。

### B1. 目录结构要求

在 `third_party/` 下手动准备：

```
third_party/
├── vosk/
│   ├── include/vosk_api.h              ← 提交
│   └── lib/                            ← 不提交
│       ├── libvosk.dll
│       ├── libgcc_s_seh-1.dll          (MinGW 运行时)
│       ├── libstdc++-6.dll
│       ├── libwinpthread-1.dll
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
        │   └── ggml.h ...              (全部 ggml 头文件)
        └── lib/
            ├── ggml.dll                (运行时 DLL, 必须)
            ├── ggml-base.dll
            ├── ggml-cpu.dll
            └── ggml.lib                (可选 — 有些包把 ggml 编入了 whisper.lib)
```

> **关于 ggml.lib**：部分 whisper.cpp 预编译包不提供单独的 ggml 导入库——ggml 符号已编入 `whisper.lib`。CMakeLists.txt 会自动检测：找到独立 ggml 就链接，找不到就假定它在 whisper.lib 里。

### B2. 安装 Qt6

通过 [Qt 官方安装器](https://www.qt.io/download) 安装。勾选 **MSVC 或 MinGW** 版本 + **Multimedia** 模块。记下安装路径（如 `H:/Software/Qt/Qt6.9.3/6.9.3/msvc2022_64`）。

### B3. 配置 CMakePresets.json

```powershell
cp CMakePresets.json.example CMakePresets.json
```

保留手动模式预设，确保两个关键变量正确：

```json
{
    "name": "msvc-debug-manual",
    "cacheVariables": {
        "USE_VCPKG": "OFF",
        "CMAKE_PREFIX_PATH": "你的Qt安装路径"
    }
}
```

### B4. 构建

```powershell
cmake --preset msvc-debug-manual
cmake --build build/msvc-debug-manual --config Debug
```

> **Qt 插件**：手动模式下 `platforms/qwindows.dll` 不会自动部署。请在 Qt Creator 中运行（它自动处理），或手动运行 `windeployqt`：
> ```powershell
> 你的Qt路径\bin\windeployqt.exe bin\Debug\Kotocord.exe
> ```

---

## CMake 预设参考

| 预设名 | 模式 | 编译器 | 说明 |
|---|---|---|---|
| `msvc-debug-vcpkg` | vcpkg | MSVC | 新 PC 首选 |
| `msvc-release-vcpkg` | vcpkg | MSVC | 发布构建 |
| `msvc-debug-manual` | 手动 | MSVC | 旧 PC + 官方 Qt |
| `msvc-release-manual` | 手动 | MSVC | 发布构建 |
| `mingw-debug-manual` | 手动 | MinGW | MinGW 版 Qt |

### 日常命令

```powershell
# 首次 configure
cmake --preset <预设名>

# 增量构建
cmake --build build/<目录> --config Debug

# 清理重编译
cmake --build build/<目录> --config Debug --clean-first

# 完全重置
Remove-Item -Recurse -Force build\<目录>
cmake --preset <预设名>
cmake --build build/<目录> --config Debug
```

---

## 构建产物

```
bin/
├── Debug/
│   ├── Kotocord.exe
│   ├── *.dll                       ← Qt/whisper/ggml/Vosk 自动部署
│   ├── platforms/qwindowsd.dll      ← Qt 平台插件 (必须)
│   ├── styles/                     ← Windows 原生风格
│   ├── imageformats/               ← JPEG/GIF/SVG 支持
│   ├── tls/                        ← HTTPS (DeepSeek API 需要)
│   └── multimedia/                 ← 音频采集插件
└── Release/
    └── ...
```

---

## 分发给他人

构建 Release 后，`bin/Release/` 目录即为完整运行包。需同时打包：

```
Kotocord-v0.1.0/
├── Kotocord.exe
├── *.dll
├── platforms/qwindows.dll
├── styles/
├── imageformats/
├── tls/
├── multimedia/
└── resources/
    ├── kaomoji.json
    └── model/
        ├── vosk-model-small-cn-0.22/
        └── ggml-small.bin
```

**发布前检查：**
- [ ] exe 同级有 `platforms/qwindows.dll`
- [ ] `resources/model/` 下有模型文件
- [ ] 在未安装 Qt 的裸机上测试过双击运行

---

## vcpkg 编译提速

| 策略 | 效果 |
|---|---|
| **二进制缓存** | 备份 `C:\vcpkg\packages\` 和 `C:\vcpkg\archives\`，重装后放回 |
| **只装需要的模块** | 本项目已采用 (`qtbase[widgets]` 而非全量 `qt6`) |
| **远程缓存** | NuGet/S3 团队共享，见 [Binary Caching](https://learn.microsoft.com/en-us/vcpkg/users/binarycaching) |

---

## 跨平台

CMakeLists.txt 已预留 macOS/Linux 分支。vcpkg 在 macOS/Linux 上使用方式相同：

```bash
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg && ./bootstrap-vcpkg.sh
./vcpkg install qtbase qtmultimedia[widgets] whisper-cpp
```

---

## 常见问题

### Q: "no Qt platform plugin" 错误

`platforms/qwindows.dll` 缺失。vcpkg 模式下构建自动部署；手动模式需运行一次 `windeployqt`。

### Q: "Could not find GGML_LIB" (旧电脑)

ggml 的 `.lib` 文件缺失——某些 whisper.cpp 预编译包不带单独的 ggml 导入库。新版 CMakeLists.txt 已不再强制要求，会自动降级链接。如果仍有问题，运行 `dir /s third_party\whisper\*.lib` 检查实际文件。

### Q: "Vosk library not found"

`third_party/vosk/lib/` 下缺少 `.lib`。MSVC 需用 `lib.exe` 生成（见模式 A 步骤 3）。

### Q: 运行时 "无法加载 Vosk 模型" / "Whisper 模型未正确初始化"

`resources/model/` 下缺少模型文件。检查 `src/utils/AppPaths.h` 中的路径。

### Q: vcpkg 的 Qt 和官方 Qt 可以共存吗？

可以。它们安装在不同目录，互不干扰。Kit/预设决定了用哪个。

### Q: `CMakePresets.json` 修改后 git 不追踪了？

这是设计如此。`.gitignore` 已忽略 `CMakePresets.json`。每人从 `CMakePresets.json.example` 复制后自行修改。
