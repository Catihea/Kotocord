cmake_minimum_required(VERSION 3.16)
project(Kotocord VERSION 0.1.0 LANGUAGES CXX)

# ==========================================
# 1. 编译选项与全局配置
# ==========================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/Release")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ==========================================
# 2. 第三方依赖 (vcpkg 管理)
# ==========================================
find_package(Qt6 COMPONENTS Widgets Multimedia REQUIRED)
find_package(whisper CONFIG REQUIRED)   # whisper-cpp (替代了 third_party/whisper)

# ==========================================
# 2.5 Vosk — 不在 vcpkg 中, 保留手动管理
# ==========================================
set(VOSK_DIR "${CMAKE_SOURCE_DIR}/third_party/vosk")

# ==========================================
# 3. 源码文件分类整理
# ==========================================
set(UTILS_SOURCES
    src/utils/AppPaths.h
)

set(UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.ui
)

set(CORE_SOURCES
    src/main.cpp
    src/core/AppController.cpp
    src/core/AppController.h
    src/core/DataTypes.h
)

set(MODULE_SOURCES
    # Input / ASR
    src/modules/input/IAudioTranscriber.h
    src/modules/input/VoskTranscriber.h
    src/modules/input/VoskTranscriber.cpp
    src/modules/input/WhisperTranscriber.h
    src/modules/input/WhisperTranscriber.cpp

    # Capture
    src/modules/capture/AudioCapture.h
    src/modules/capture/AudioCapture.cpp
    src/modules/capture/AudioFileSimulator.h
    src/modules/capture/AudioFileSimulator.cpp

    # LLM & Emotion
    src/modules/llm/ILanguageModel.h
    src/modules/llm/DeepSeekAPIWorker.h
    src/modules/llm/DeepSeekAPIWorker.cpp
    src/modules/llm/MockLLMWorker.h
    src/modules/llm/MockLLMWorker.cpp

    src/modules/llm/KaomojiManager.h
    src/modules/llm/KaomojiManager.cpp

    # Render
    src/modules/render/SubtitleRenderer.h
    src/modules/render/SubtitleRenderer.cpp

    # System
    src/modules/system/SystemResourceMonitor.h
    src/modules/system/SystemResourceMonitor.cpp
)

# ==========================================
# 4. 定义生成目标 (Executable)
# ==========================================
add_executable(${PROJECT_NAME}
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${MODULE_SOURCES}
    ${UTILS_SOURCES}
)

# ==========================================
# 5. 包含目录 & 链接
# ==========================================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${VOSK_DIR}/include
)

# Vosk 静态导入库 (手动管理)
find_library(VOSK_LIB
    NAMES vosk libvosk
    PATHS ${VOSK_DIR}/lib
    NO_DEFAULT_PATH
)
if(NOT VOSK_LIB)
    message(FATAL_ERROR "Vosk library not found in ${VOSK_DIR}/lib. "
                        "Please download prebuilt Vosk libs from https://alphacephei.com/vosk/")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    whisper                # vcpkg 管理的 whisper-cpp (ggml 作为传递依赖自动链接)
    ${VOSK_LIB}
)

# ==========================================
# 6. 运行时依赖部署 (开发阶段)
# ==========================================
if(WIN32)
    # ---------- 6.1 定位 vcpkg installed 目录 ----------
    #     CMAKE_PREFIX_PATH 由 vcpkg toolchain 自动注入
    foreach(_prefix ${CMAKE_PREFIX_PATH})
        if(EXISTS "${_prefix}/bin" AND EXISTS "${_prefix}/include")
            set(_VCPKG_INSTALLED "${_prefix}")
            break()
        endif()
    endforeach()

    # ---------- 6.2 获取构建类型 (Debug / Release) ----------
    #     使用 generator expression 在构建时根据实际配置解析路径
    #     Debug:   <vcpkg>/debug/bin/  +  <vcpkg>/debug/Qt6/plugins/
    #     Release: <vcpkg>/bin/        +  <vcpkg>/Qt6/plugins/
    set(_RUNTIME_BIN_DIR  "${_VCPKG_INSTALLED}/bin")
    set(_DEBUG_BIN_DIR    "${_VCPKG_INSTALLED}/debug/bin")
    set(_RUNTIME_PLUGINS  "${_VCPKG_INSTALLED}/Qt6/plugins")
    set(_DEBUG_PLUGINS    "${_VCPKG_INSTALLED}/debug/Qt6/plugins")

    # ---------- 6.3 whisper / ggml DLL ----------
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:$<CONFIG:Debug>,${_DEBUG_BIN_DIR},${_RUNTIME_BIN_DIR}>/whisper.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:$<CONFIG:Debug>,${_DEBUG_BIN_DIR},${_RUNTIME_BIN_DIR}>/ggml.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:$<CONFIG:Debug>,${_DEBUG_BIN_DIR},${_RUNTIME_BIN_DIR}>/ggml-cpu.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:$<CONFIG:Debug>,${_DEBUG_BIN_DIR},${_RUNTIME_BIN_DIR}>/ggml-base.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Deploying whisper/ggml DLLs..."
    )

    # ---------- 6.4 Vosk DLL (含 MinGW 运行时) ----------
    if(EXISTS "${VOSK_DIR}/lib")
        file(GLOB _VOSK_DLLS "${VOSK_DIR}/lib/*.dll")
        if(_VOSK_DLLS)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${_VOSK_DLLS}
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
                COMMENT "Deploying Vosk DLLs..."
            )
        endif()
    endif()

    # ---------- 6.5 Qt 平台插件 (关键！缺少会导致 "no Qt platform plugin" 错误) ----------
    #     windeployqt 会处理上层 DLL，但插件子目录有时需要显式拷贝
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        # platforms/ — qwindows(d).dll 是 Qt GUI 程序的运行时入口
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<IF:$<CONFIG:Debug>,${_DEBUG_PLUGINS},${_RUNTIME_PLUGINS}>/platforms"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms"
        # styles/ — qmodernwindowsstyle(d).dll 提供原生 Windows 风格
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<IF:$<CONFIG:Debug>,${_DEBUG_PLUGINS},${_RUNTIME_PLUGINS}>/styles"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/styles"
        # imageformats/ — qjpeg(d).dll, qgif(d).dll 等图像格式支持
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<IF:$<CONFIG:Debug>,${_DEBUG_PLUGINS},${_RUNTIME_PLUGINS}>/imageformats"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/imageformats"
        # tls/ — OpenSSL/Schannel 后端, QNetworkAccessManager (DeepSeek API) 需要
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<IF:$<CONFIG:Debug>,${_DEBUG_PLUGINS},${_RUNTIME_PLUGINS}>/tls"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/tls"
        # multimedia/ — 音频采集插件 (Qt Multimedia 需要)
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<IF:$<CONFIG:Debug>,${_DEBUG_PLUGINS},${_RUNTIME_PLUGINS}>/multimedia"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/multimedia"
        COMMENT "Deploying Qt plugin directories (platforms, styles, imageformats, tls, multimedia)..."
    )

elseif(APPLE)
    # macOS: 拷贝 dylib
    file(GLOB _VOSK_DYLIBS "${VOSK_DIR}/lib/*.dylib")
    if(_VOSK_DYLIBS)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${_VOSK_DYLIBS}
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        )
    endif()
else()
    # Linux: 拷贝 .so
    file(GLOB _VOSK_SOS "${VOSK_DIR}/lib/*.so")
    if(_VOSK_SOS)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${_VOSK_SOS}
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        )
    endif()
endif()

# ==========================================
# 7. 预编译头 (PCH)
# ==========================================
target_precompile_headers(${PROJECT_NAME} PRIVATE
    <QString>
    <QObject>
    <QWidget>
    <QDebug>
    <QMutex>
    <QThread>
    <QSettings>
    <QTimer>
    <QJsonDocument>
    <QNetworkAccessManager>
)
