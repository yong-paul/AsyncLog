#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <iostream>

class AsyncLogger {
public:
	enum class Level {
		Debug,
		Info,
		Warning,
		Error,
		Fatal
	};

	// 获取单例实例
	static AsyncLogger& instance();

	// 初始化日志系统
	bool init(const std::string& log_dir = "./logs",
		const std::string& log_name = "app",
		Level level = Level::Info,
		size_t max_file_size = 1048576, // 1MB
		int max_files = 10);

	// 停止日志系统
	void stop();

	// 设置日志级别
	void set_level(Level level);

	// 日志输出接口
	void log(Level level, const char* func, const char* file, int line, const char* format, ...);

	// 禁用拷贝和赋值
	AsyncLogger(const AsyncLogger&) = delete;
	AsyncLogger& operator=(const AsyncLogger&) = delete;

private:
	AsyncLogger();
	~AsyncLogger();

	// 后台线程主循环
	void run();

	// 获取文件大小
	size_t get_file_size(const std::string& filename) const;

	// 检查并限制日志文件数量
	void check_file_count();

	// 内部实现细节
	struct Impl;
	std::unique_ptr<Impl> pimpl_;
};