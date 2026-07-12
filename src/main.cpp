#include <QApplication>
#include <QDir>
#include <QCoreApplication>

#include "ui/MainWindow.h"
#include "utils/AppPaths.h"
#include "core/AppController.h"
#include "modules/input/VoskTranscriber.h"
#include "modules/input/WhisperTranscriber.h"
#include "modules/capture/AudioCapture.h"
#include "modules/llm/MockLLMWorker.h"
#include "modules/llm/DeepSeekAPIWorker.h"
#include "modules/llm/KaomojiManager.h"
#include "modules/system/SystemResourceMonitor.h"
#include <QFile>
#include <QTextStream>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);// 初始化 Qt 应用程序实例

	// ==========================================
	// 第一步：实例化所有组件 (工厂制造)
	// ==========================================

	// 1. 核心控制器
	AppController controller;

	// 2. 底层干活的 Worker 们
	VoskTranscriber voskEngine;
	WhisperTranscriber whisperEngine;
	AudioCapture micCapture;
	MockLLMWorker mockLLM;
	DeepSeekAPIWorker deepSeekLLM;

	// 尝试从 exe 同级目录的 apikey.txt 加载 API Key (分发版本用)
	QString apiKeyPath = AppPaths::getApiKeyFilePath();
	if (QFile::exists(apiKeyPath)) {
		QFile keyFile(apiKeyPath);
		if (keyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
			QString fileKey = QString::fromUtf8(keyFile.readAll()).trimmed();
			if (!fileKey.isEmpty()) {
				deepSeekLLM.setApiConfig(fileKey);
				qDebug() << "[Main] API Key 已从文件加载:" << apiKeyPath;
			}
			keyFile.close();
		}
	}

	KaomojiManager kaomojiManager;
	SystemResourceMonitor sysMonitor;

	// 3. UI 界面 (将控制器注入)
	MainWindow window(&controller);

	// ==========================================
	// 第二步：依赖注入与初始配置 (初始化)
	// ==========================================

	kaomojiManager.loadFromFile(AppPaths::getKaomojiPath());
	controller.setKaomojiManager(&kaomojiManager);
	controller.setLanguageModel(&mockLLM); // 默认使用 Mock LLM

	// 用一个局部变量记录当前选中的 ASR 引擎
	IAudioTranscriber* currentASR = &voskEngine;
	bool isAsrEnabled = false; // 记录总开关状态

	// ==========================================
	// 第三步：连线打通整个系统 (建立神经网络)
	// ==========================================

	// --- 1. 控制器 -> UI (文字上屏) ---
	QObject::connect(&controller,&AppController::subtitleReadyForRender,
					 &window,&MainWindow::onSubtitleReady);

	// --- 2. 系统/LLM -> UI (刷新监控数据看板) ---
	QObject::connect(&sysMonitor,&SystemResourceMonitor::resourceUpdated,
					 &window,&MainWindow::updateCpuMem);

	QObject::connect(&mockLLM,&ILanguageModel::textProcessed,
					 &window,&MainWindow::updateEmotionLabel);
	QObject::connect(&deepSeekLLM,&ILanguageModel::textProcessed,
					 &window,&MainWindow::updateEmotionLabel);
	QObject::connect(&deepSeekLLM,&ILanguageModel::performanceMetricsReported,
					 &window,&MainWindow::updateLatencyAndTokens);

	// --- 3. 麦克风 -> ASR 引擎 (语音分发) ---
	QObject::connect(&micCapture,&AudioCapture::audioDataReady,
					 &voskEngine,&VoskTranscriber::onAudioDataReady);
	QObject::connect(&micCapture,&AudioCapture::audioDataReady,
					 &whisperEngine,&WhisperTranscriber::onAudioDataReady);

	// --- 4. ASR 引擎 -> 控制器 (文字汇聚) ---
	QObject::connect(&voskEngine,&IAudioTranscriber::textReady,
					 &controller,&AppController::onASRTextReady);
	QObject::connect(&whisperEngine,&IAudioTranscriber::textReady,
					 &controller,&AppController::onASRTextReady);

	// --- 5. UI -> 控制器/底层 (响应用户意图) ---
	// 切换 LLM
	QObject::connect(&window,&MainWindow::llmEngineSwitched,[&](bool isDeepSeek){
		controller.setLanguageModel(isDeepSeek ? static_cast<ILanguageModel*>(&deepSeekLLM) : static_cast<ILanguageModel*>(&mockLLM));
	});

	// 录入 API Key
	QObject::connect(&window,&MainWindow::apiKeyChanged,[&](const QString& key){
		deepSeekLLM.setApiConfig(key);
	});

	// 启停语音识别
	QObject::connect(&window,&MainWindow::asrToggleRequested,[&](bool enabled){
		isAsrEnabled = enabled;
		if(enabled) {
			if(currentASR->start()) micCapture.start();
		} else {
			micCapture.stop();
			currentASR->stop();
		}
	});

	// 切换 ASR 引擎
	QObject::connect(&window,&MainWindow::asrEngineSwitched,[&](bool isWhisper){
		currentASR->stop(); // 强行拔掉旧引擎的车钥匙
		currentASR = isWhisper ? static_cast<IAudioTranscriber*>(&whisperEngine) : static_cast<IAudioTranscriber*>(&voskEngine);
		if(isAsrEnabled) {
			currentASR->start(); // 如果麦克风开着，立刻启动新引擎
		}
	});

	// ==========================================
	// 第四步：启动
	// ==========================================
	sysMonitor.start(1000);
	window.show();

    return app.exec();//进入 Qt 的事件循环
}
