#include "LogMacros.h"
#include <thread>
#include <vector>

// ģ�⹤������1
void worker_function1(int id) {
	LOG_DEBUG("Worker %d started in function1", id);
	for (int i = 0; i < 5; ++i) {
		LOG_INFO("Worker %d processing task %d in function1", id, i);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	LOG_WARNING("Worker %d completed in function1", id);
}

// ģ�⹤������2
void worker_function2(const std::string& name) {
	LOG_DEBUG("Worker %s started in function2", name.c_str());
	for (int i = 0; i < 3; ++i) {
		LOG_INFO("Worker %s processing task %d in function2", name.c_str(), i);
		std::this_thread::sleep_for(std::chrono::milliseconds(150));
	}
	LOG_ERROR("Worker %s encountered simulated error in function2", name.c_str());
}

// ģ�����ݴ�����
void data_processor() {
	LOG_DEBUG("Data processor started");
	try {
		for (int i = 0; i < 4; ++i) {
			LOG_INFO("Processing data batch %d", i);
			if (i == 2) {
				throw std::runtime_error("Simulated data corruption");
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
	}
	catch (const std::exception& e) {
		LOG_FATAL("Data processor failed: %s", e.what());
	}
}

int main() {
	// ��ʼ����־ϵͳ
	if (!AsyncLogger::instance().init("./logs", "test", AsyncLogger::Level::Debug)) {
		std::cerr << "Failed to initialize logger" << std::endl;
		return 1;
	}

	LOG_INFO("Main thread started");

	// ��¼��ͬ�������־
	LOG_DEBUG("This is a debug message");
	LOG_INFO("This is an info message, number: %d", 42);
	LOG_WARNING("This is a warning message");
	LOG_ERROR("This is an error message");
	LOG_FATAL("This is a fatal message");

	// ������������߳�
	std::vector<std::thread> threads;

	// �߳���1 - ʹ��worker_function1
	for (int i = 1; i <= 3; ++i) {
		threads.emplace_back(worker_function1, i);
	}

	// �߳���2 - ʹ��worker_function2
	threads.emplace_back(worker_function2, "Alpha");
	threads.emplace_back(worker_function2, "Beta");

	// �߳���3 - ���ݴ����߳�
	threads.emplace_back(data_processor);

	// ���߳�Ҳ��¼һЩ��־
	for (int i = 0; i < 10; ++i) {
		LOG_INFO("Main thread working on item %d", i);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	// �ȴ������߳����
	for (auto& t : threads) {
		t.join();
	}

	LOG_INFO("All worker threads completed");

	// ֹͣ��־ϵͳ
	AsyncLogger::instance().stop();

	return 0;
}