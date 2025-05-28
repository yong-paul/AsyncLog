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

	// ��ȡ����ʵ��
	static AsyncLogger& instance();

	// ��ʼ����־ϵͳ
	bool init(const std::string& log_dir = "./logs",
		const std::string& log_name = "app",
		Level level = Level::Info,
		size_t max_file_size = 1048576, // 1MB
		int max_files = 10);

	// ֹͣ��־ϵͳ
	void stop();

	// ������־����
	void set_level(Level level);

	// ��־����ӿ�
	void log(Level level, const char* func, const char* file, int line, const char* format, ...);

	// ���ÿ����͸�ֵ
	AsyncLogger(const AsyncLogger&) = delete;
	AsyncLogger& operator=(const AsyncLogger&) = delete;

private:
	AsyncLogger();
	~AsyncLogger();

	// ��̨�߳���ѭ��
	void run();

	// ��ȡ�ļ���С
	size_t get_file_size(const std::string& filename) const;

	// ��鲢������־�ļ�����
	void check_file_count();

	// �ڲ�ʵ��ϸ��
	struct Impl;
	std::unique_ptr<Impl> pimpl_;
};