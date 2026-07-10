# KotoCord

面向 VRChat 无言势及 VTuber 的综合性直播辅助工具。集成语音识别（STT）、大语言模型（LLM）情绪分析，将语音/文本输入转化为带有情绪色彩的艺术字字幕，实时驱动虚拟形象的表情，增强直播互动性和表现力。

## 功能

- **双引擎语音识别** — Vosk（轻量离线流式）+ Whisper.cpp（高精度离线），可运行时切换
- **LLM 情绪分析** — 接入 DeepSeek API（或兼容 OpenAI 接口的本地模型），自动识别情绪并附加颜文字
- **可视化字幕** — 带情绪标签的艺术字渲染，支持队列调度与视觉锁机制
- **系统资源监控** — CPU / 内存实时面板，辅助调试和性能调优
- **双模式构建** — 同一份代码，vcpkg 全自动 或 手动 Qt 均可构建。详见 [BUILD.md](BUILD.md)

## 架构

```
麦克风 → AudioCapture → [VoskTranscriber / WhisperTranscriber]
                                    ↓
                            AppController (队列 + 视觉锁)
                                    ↓
                 [MockLLMWorker / DeepSeekAPIWorker]
                                    ↓
                      SubtitleFrame (情绪 + 文本 + 颜文字)
                                    ↓
                              MainWindow → 屏幕渲染
```

### 模块

| 目录 | 职责 |
|---|---|
| `src/core/` | `AppController` 核心调度、`DataTypes.h` 公用类型 |
| `src/ui/` | Qt 主窗口 (`.ui` 设计师文件) |
| `src/modules/input/` | ASR 抽象层 (`IAudioTranscriber`) + Vosk/Whisper 实现 |
| `src/modules/capture/` | 音频采集 (`AudioCapture` 麦克风 / `AudioFileSimulator` 调试) |
| `src/modules/llm/` | LLM 抽象层 + DeepSeek API / Mock + `KaomojiManager` 颜文字 |
| `src/modules/render/` | 字幕渲染 (`SubtitleRenderer`) |
| `src/modules/system/` | 系统资源监控 (`SystemResourceMonitor`) |
| `src/utils/` | `AppPaths` 统一资源路径 |

### 依赖

| 依赖 | 用途 | 管理方式 |
|---|---|---|
| Qt 6 (Widgets, Multimedia) | UI + 音频采集 | vcpkg 或 官方安装器 |
| whisper.cpp + ggml | 高精度语音识别 | vcpkg 或 `third_party/whisper/` |
| Vosk API | 轻量流式语音识别 | `third_party/vosk/` (手动) |
| DeepSeek API | LLM 情绪分析 | 纯网络, 无本地依赖 |

## 快速开始

```powershell
# 新电脑 (vcpkg 模式)
cp CMakePresets.json.example CMakePresets.json
cmake --preset msvc-debug-vcpkg
cmake --build build/msvc-debug-vcpkg --config Debug

# 旧电脑 (手动 Qt 模式)
# 编辑 CMakePresets.json, 设 USE_VCPKG=OFF, CMAKE_PREFIX_PATH 指向你的 Qt
cmake --preset msvc-debug-manual
cmake --build build/msvc-debug-manual --config Debug
```

> 完整部署指南、旧电脑迁移步骤、运行时打包方法见 **[BUILD.md](BUILD.md)**。
> IDE 配置实践记录见 **[DEV_GUIDE.md](DEV_GUIDE.md)**。

## 项目结构

```
Kotocord/
├── CMakeLists.txt                     # 双模式构建脚本
├── CMakePresets.json.example          # Preset 模板 (复制为 CMakePresets.json)
├── BUILD.md                           # 构建与部署完整指南
├── DEV_GUIDE.md                       # IDE 配置实践记录
├── README.md                          # 本文件
│
├── src/                               # 源码
├── third_party/vosk/                  # Vosk (手动)
├── resources/                         # 颜文字 + 模型文件
├── build/                             # CMake 中间产物 (不提交)
└── bin/                               # 可执行文件输出 (不提交)
```

## 致谢

| 项目 | 用途 | 协议 |
|---|---|---|
| [Qt 6](https://www.qt.io/) | UI 框架与多媒体 | LGPL v3 |
| [Vosk API](https://alphacephei.com/vosk/) | 离线流式语音识别 | Apache 2.0 |
| [Whisper.cpp](https://github.com/ggml-org/whisper.cpp) | 高精度语音识别 | MIT |
| [vcpkg](https://vcpkg.io/) | C/C++ 包管理器 | MIT |
