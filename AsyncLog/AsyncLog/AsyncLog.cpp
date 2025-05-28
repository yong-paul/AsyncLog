#include "AsyncLog.h"
#include <fstream>
#include <thread>
#include <vector>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define stat _stat
#else
#include <unistd.h>
#include <dirent.h>
#endif

// 获取当前进程ID（跨平台）
static unsigned long get_process_id() {
#ifdef _WIN32
	return static_cast<unsigned long>(GetCurrentProcessId());
#else
	return static_cast<unsigned long>(getpid());
#endif
}

// 将thread::id转换为数值类型
static unsigned int thread_id_to_uint(const std::thread::id& id) {
	static_assert(sizeof(std::thread::id) == sizeof(unsigned int) ||
		sizeof(std::thread::id) == sizeof(unsigned long long),
		"Unsupported thread::id size");

	if (sizeof(std::thread::id) == sizeof(unsigned int)) {
		return *reinterpret_cast<const unsigned int*>(&id);
	}
	else {
		return static_cast<unsigned int>(
			*reinterpret_cast<const unsigned long long*>(&id));
	}
}

struct AsyncLogger::Impl {
	std::atomic<bool> running{ false };
	std::atomic<Level> level{ Level::Info };
	std::thread worker_thread;
	std::mutex mutex;
	std::condition_variable cond;
	std::queue<std::string> log_queue;

	std::string log_dir;
	std::string log_name;
	size_t max_file_size;
	int max_files;
	bool initialized = false;
};

AsyncLogger::AsyncLogger() : pimpl_(std::make_unique<Impl>()) {}

AsyncLogger::~AsyncLogger() {
	stop();
}

AsyncLogger& AsyncLogger::instance() {
	static AsyncLogger logger;
	return logger;
}

bool AsyncLogger::init(const std::string& log_dir, const std::string& log_name,
	Level level, size_t max_file_size, int max_files) {
	if (pimpl_->initialized) {
		return false;
	}

	pimpl_->log_dir = log_dir;
	pimpl_->log_name = log_name;
	pimpl_->level.store(level, std::memory_order_relaxed);
	pimpl_->max_file_size = max_file_size;
	pimpl_->max_files = max_files;

	// 创建日志目录
	if (mkdir(log_dir.c_str(), 0755) != 0 && errno != EEXIST) {
		std::cerr << "Failed to create log directory: " << log_dir << std::endl;
		return false;
	}

	// 启动后台线程
	pimpl_->running.store(true, std::memory_order_relaxed);
	pimpl_->worker_thread = std::thread(&AsyncLogger::run, this);

	pimpl_->initialized = true;
	return true;
}

void AsyncLogger::stop() {
	if (!pimpl_->initialized) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(pimpl_->mutex);
		pimpl_->running.store(false, std::memory_order_relaxed);
		pimpl_->cond.notify_one();
	}

	if (pimpl_->worker_thread.joinable()) {
		pimpl_->worker_thread.join();
	}

	pimpl_->initialized = false;
}

void AsyncLogger::set_level(Level level) {
	pimpl_->level.store(level, std::memory_order_relaxed);
}

void AsyncLogger::log(Level level, const char* func, const char* file, int line, const char* format, ...) {
	if (static_cast<int>(level) < static_cast<int>(pimpl_->level.load(std::memory_order_relaxed)) ||
		!pimpl_->initialized) {
		return;
	}

	// 使用局部缓冲区避免多次内存分配
	thread_local char buffer[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	const char* level_str = "";
	switch (level) {
	case Level::Debug:   level_str = "DEBUG"; break;
	case Level::Info:    level_str = "INFO"; break;
	case Level::Warning: level_str = "WARN"; break;
	case Level::Error:   level_str = "ERROR"; break;
	case Level::Fatal:   level_str = "FATAL"; break;
	}

	auto now = std::chrono::system_clock::now();
	auto now_time = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	struct tm tm_now;
#ifdef _WIN32
	localtime_s(&tm_now, &now_time);
#else
	localtime_r(&now_time, &tm_now);
#endif

	char time_str[64];
	strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_now);

	// 获取进程ID和线程ID
	unsigned long pid = get_process_id();
	unsigned int tid = thread_id_to_uint(std::this_thread::get_id());

	// 使用固定大小的缓冲区避免动态分配
	thread_local char log_entry[2048];
	snprintf(log_entry, sizeof(log_entry), "[%s.%03lld][%s][PID:%lu][TID:%u][%s][%s][%d] %s\n",
		time_str, static_cast<long long>(ms.count()), level_str,
		pid, tid, func, file, line, buffer);

	{
		std::lock_guard<std::mutex> lock(pimpl_->mutex);
		pimpl_->log_queue.push(log_entry);
		pimpl_->cond.notify_one();
	}
}

void AsyncLogger::run() {
	std::ofstream log_file;
	std::string current_file;
	size_t current_size = 0;

	while (pimpl_->running.load(std::memory_order_relaxed) || !pimpl_->log_queue.empty()) {
		std::string log_entry;

		{
			std::unique_lock<std::mutex> lock(pimpl_->mutex);
			pimpl_->cond.wait(lock, [this] {
				return !pimpl_->running.load(std::memory_order_relaxed) || !pimpl_->log_queue.empty();
			});

			if (!pimpl_->log_queue.empty()) {
				log_entry = std::move(pimpl_->log_queue.front());
				pimpl_->log_queue.pop();
			}
		}

		if (!log_entry.empty()) {
			// 检查是否需要创建新日志文件
			auto now = std::chrono::system_clock::now();
			auto now_time = std::chrono::system_clock::to_time_t(now);

			struct tm tm_now;
#ifdef _WIN32
			localtime_s(&tm_now, &now_time);
#else
			localtime_r(&now_time, &tm_now);
#endif

			char date_str[64];
			strftime(date_str, sizeof(date_str), "%Y%m%d", &tm_now);

			std::string new_file = pimpl_->log_dir + "/" + pimpl_->log_name + "_" + date_str + ".log";

			if (new_file != current_file || current_size >= pimpl_->max_file_size) {
				if (log_file.is_open()) {
					log_file.close();
				}

				// 检查文件数量，如果超过限制则删除最旧的文件
				check_file_count();

				// 打开新文件
				log_file.open(new_file, std::ios::app);
				if (!log_file) {
					std::cerr << "Failed to open log file: " << new_file << std::endl;
					continue;
				}

				current_file = new_file;
				current_size = get_file_size(new_file);
			}

			// 批量写入日志
			log_file << log_entry;
			current_size += log_entry.size();

			// 定期flush而不是每次写入都flush
			static int flush_counter = 0;
			if (++flush_counter >= 100) {
				log_file.flush();
				flush_counter = 0;
			}
		}
	}

	// 最后确保所有日志都被写入
	if (log_file.is_open()) {
		log_file.flush();
		log_file.close();
	}
}

size_t AsyncLogger::get_file_size(const std::string& filename) const {
	struct stat st;
	if (stat(filename.c_str(), &st) == 0) {
		return st.st_size;
	}
	return 0;
}

void AsyncLogger::check_file_count() {
#ifdef _WIN32
	WIN32_FIND_DATAA findData;
	HANDLE hFind = INVALID_HANDLE_VALUE;

	std::string pattern = pimpl_->log_dir + "\\" + pimpl_->log_name + "_*.log";
	hFind = FindFirstFileA(pattern.c_str(), &findData);

	if (hFind == INVALID_HANDLE_VALUE) {
		return;
	}

	std::vector<std::string> log_files;
	do {
		if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			log_files.push_back(findData.cFileName);
		}
	} while (FindNextFileA(hFind, &findData) != 0);

	FindClose(hFind);
#else
	std::unique_ptr<DIR, decltype(&closedir)> dir(opendir(pimpl_->log_dir.c_str()), &closedir);
	if (!dir) {
		return;
	}

	std::vector<std::string> log_files;
	struct dirent* entry;

	std::string prefix = pimpl_->log_name + "_";
	while ((entry = readdir(dir.get())) != nullptr) {
		std::string filename = entry->d_name;
		if (filename.find(prefix) == 0 && filename.rfind(".log") == filename.length() - 4) {
			log_files.push_back(filename);
		}
	}
#endif

	if (log_files.size() <= static_cast<size_t>(pimpl_->max_files)) {
		return;
	}

	// 按文件名排序(按日期)
	std::sort(log_files.begin(), log_files.end());

	// 删除最旧的文件
	for (size_t i = 0; i < log_files.size() - pimpl_->max_files; ++i) {
		std::string filepath = pimpl_->log_dir + "/" + log_files[i];
#ifdef _WIN32
		DeleteFileA(filepath.c_str());
#else
		unlink(filepath.c_str());
#endif
	}
}