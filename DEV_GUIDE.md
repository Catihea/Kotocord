# IDE 配置实践记录

> 本文记录了一台典型"旧设备"开发环境的完整 CMake + IDE 配置，供之后学习和参考。
> 环境特征：Qt 装在 H 盘（节约 C 盘）、无 vcpkg、VS Code 与 VS2022 双 IDE 并存。

## 目录

- [旧设备环境参数](#旧设备环境参数)
- [CMakeUserPresets.json — 构建预设](#cmakeuserpresetsjson--构建预设)
- [VS Code 配置](#vs-code-配置)
- [Visual Studio 2022 配置](#visual-studio-2022-配置)
- [双模式 CMakeLists.txt 工作原理](#双模式-cmakeliststxt-工作原理)
- [从旧代码迁移到双模式 (git pull 后只需改一行)](#从旧代码迁移到双模式)

---

## 旧设备环境参数

| 项目 | 值 |
|---|---|
| 操作系统 | Windows 10 |
| Qt 版本 | 6.9.3 (官方安装器) |
| Qt 路径 | `H:/Software/Qt/Qt6.9.3/6.9.3/msvc2022_64` |
| 编译器 | MSVC (Visual Studio 2022 Community) |
| CMake | 3.30.5 |
| IDE | VS Code (CMake Tools 插件) + VS2022 |
| 项目路径 | `H:\Laboratory\Kotocord` |
| vcpkg | 未安装 |

---

## CMakeUserPresets.json — 构建预设

旧设备使用 `CMakeUserPresets.json` 来保存本地路径，避免将机器相关的绝对路径提交到仓库。

```json
{
    "version": 3,
    "configurePresets": [
        {
            "name": "kotocord-msvc-local",
            "displayName": "Local MSVC x64 with Qt6",
            "generator": "Visual Studio 17 2022",
            "architecture": "x64",
            "binaryDir": "${sourceDir}/build",
            "cacheVariables": {
                "CMAKE_EXPORT_COMPILE_COMMANDS": true,
                "CMAKE_PREFIX_PATH": "H:/Software/Qt/Qt6.9.3/6.9.3/msvc2022_64"
            }
        }
    ],
    "buildPresets": [
        {
            "name": "build-debug",
            "configurePreset": "kotocord-msvc-local",
            "configuration": "Debug"
        },
        {
            "name": "build-release",
            "configurePreset": "kotocord-msvc-local",
            "configuration": "Release"
        }
    ]
}
```

**关键变量解析：**

| 变量 | 作用 |
|---|---|
| `CMAKE_PREFIX_PATH` | 告诉 CMake 去哪找 Qt6。`find_package(Qt6 ...)` 会在此路径下搜索 `lib/cmake/Qt6*Config.cmake` |
| `CMAKE_EXPORT_COMPILE_COMMANDS` | 生成 `compile_commands.json`，供 clangd / IntelliSense 使用 |
| `generator: "Visual Studio 17 2022"` | 使用 VS2022 的 MSBuild 生成器（不能用 Ninja——Qt 的 AUTOMOC 需要 MSBuild 的依赖扫描） |

> **Preset vs UserPreset**：`CMakePresets.json` 放通用模板，`CMakeUserPresets.json` 放个人路径。后者被 `.gitignore` 排除，不会提交。CMake 运行时会合并两者。

---

## VS Code 配置

### settings.json

```json
{
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "cmake.sourceDirectory": "${workspaceFolder}",
    "cmake.buildDirectory": "${workspaceFolder}/build"
}
```

> `configurationProvider` 设为 `ms-vscode.cmake-tools` 后，C/C++ 扩展的 IntelliSense 会直接从 CMake 获取 include 路径和宏定义，不需要手动配置 `.vscode/c_cpp_properties.json`。

### launch.json

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Kotocord Debug (MSVC)",
            "type": "cppvsdbg",
            "request": "launch",
            "program": "${workspaceFolder}/bin/Debug/Kotocord.exe",
            "cwd": "${workspaceFolder}/bin/Debug",
            "environment": [
                {
                    "name": "PATH",
                    "value": "H:/Software/Qt/Qt6.9.3/6.9.3/msvc2022_64/bin;${env:PATH}"
                }
            ],
            "preLaunchTask": "CMake: build"
        }
    ]
}
```

**关键设计点：**

1. **`type: "cppvsdbg"`** — 使用 Visual Studio 调试引擎（不是 gdb）。MSVC 编译的程序只能用 cppvsdbg 调试。

2. **`cwd` 设为 exe 所在目录** — 这样 `AppPaths::getProjectRootDir()` 的相对路径计算才能正确工作。Qt 也会在此目录下寻找 `platforms/` 插件。

3. **PATH 注入 Qt bin** — 这是手动模式下最关键的一行。因为 `windeployqt` 不会在手动模式下运行，Qt 的 `platforms/qwindows.dll` 不在 exe 同级。将 Qt bin 加入 PATH 让 Qt 能找到全局插件目录。

   ```
   运行时插件搜索链:
   Qt 启动 → 找 platforms/qwindows.dll
     → exe 同级 platforms/ 目录 (没有 → 跳过)
     → PATH 中的 Qt bin 目录 (H:/.../msvc2022_64/bin)
       → 找到! H:/.../msvc2022_64/plugins/platforms/qwindows.dll
   ```

4. **`preLaunchTask: "CMake: build"`** — 按 F5 启动调试前自动触发增量编译。CMake Tools 插件内置此 task。

### tasks.json

CMake Tools 扩展会自动注册构建任务，通常不需要手动写 `tasks.json`。旧设备上的文件是扩展生成的默认模板。

---

## Visual Studio 2022 配置

### launch.vs.json

```json
{
  "version": "0.2.1",
  "configurations": [
    {
      "type": "default",
      "project": "CMakeLists.txt",
      "projectTarget": "Kotocord.exe",
      "name": "Kotocord.exe",
      "env": "PATH=H:\\Software\\Qt\\Qt6.9.3\\6.9.3\\msvc2022_64\\bin;${env.PATH}"
    }
  ]
}
```

> VS2022 的 CMake 集成通过 `launch.vs.json` 配置调试启动。和 VS Code 一样，PATH 中注入 Qt bin 是让调试时能找到 Qt 插件的关键。

### VS2022 的 CMake 工作流

1. `File → Open → CMake...` → 选择 `CMakeLists.txt`
2. VS 自动运行 CMake configure（使用 `CMakePresets.json` 中选中的预设）
3. 顶部下拉菜单选启动项 `Kotocord.exe`
4. F5 启动调试

VS2022 会在 `build/` 下生成 `.sln` 和 `.vcxproj`，可以直接用解决方案浏览器查看文件。

---

## 双模式 CMakeLists.txt 工作原理

核心控制流：

```
option(USE_VCPKG "..." ON)
       │
       ├── ON (默认) ──────────────────────────────────
       │   find_package(Qt6 ...)          ← vcpkg toolchain 提供路径
       │   find_package(whisper CONFIG)   ← vcpkg installed 提供
       │   链接 whisper target            ← IMPORTED target, 自带 include/libs
       │   部署: 从 vcpkg installed 拷贝 DLL + Qt 插件
       │
       └── OFF ───────────────────────────────────────
           find_package(Qt6 ...)          ← CMAKE_PREFIX_PATH 提供路径
           find_library(WHISPER_LIB ...)  ← third_party/whisper/lib/
           ggml.lib 搜索 → 找到则链接 / 找不到则假定编入 whisper.lib
           部署: 从 third_party/ 拷贝 whisper/ggml/Vosk DLL
```

**两种模式不重叠的部分只有：whisper.cpp 的发现方式和 DLL 部署策略。** 源码（`src/`）完全不知道模式的存在——它只通过 `WHISPER_LIBS` 变量链接，这个变量在两个模式下都被正确设置。

---

## 从旧代码迁移到双模式

旧设备上 `git pull` 拉下双模式代码后，需要在一处加上 `USE_VCPKG: OFF`。

**已有 `CMakeUserPresets.json` 的情况（旧设备实录）：**

```diff
  "cacheVariables": {
      "CMAKE_EXPORT_COMPILE_COMMANDS": true,
      "CMAKE_PREFIX_PATH": "H:/Software/Qt/Qt6.9.3/6.9.3/msvc2022_64",
+     "USE_VCPKG": "OFF"
  }
```

**没有 Preset 的情况：**

```powershell
cp CMakePresets.json.example CMakePresets.json
# 编辑: 保留 msvc-debug-manual, 删除 vcpkg 预设, 改 CMAKE_PREFIX_PATH
```

之后构建：

```powershell
cmake --preset kotocord-msvc-local    # 预设名不变 (如果用 CMakeUserPresets.json)
# 或
cmake --preset msvc-debug-manual      # 如果用 CMakePresets.json

cmake --build build --config Debug
```

**不需要改动的文件：**
- `.vscode/` — 在 `.gitignore` 中，pull 不覆盖
- `CMakeUserPresets.json` — 同上
- `third_party/` — 同上
- `build/` — 同上
- `bin/` — 同上
