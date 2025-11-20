#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <atomic>

// Platform-specific CPU affinity and priority settings
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <sched.h>
#include <pthread.h>
#endif

class ThreadPool
{
public:
	// Thread priority enumeration
	enum class Priority
	{
		LOW,
		NORMAL,
		HIGH,
		REALTIME
	};

	// Constructor with optional parameters: CPU affinity and priority
	ThreadPool(size_t threads,
		const std::vector<int>& cpu_affinity = {},
		Priority priority = Priority::NORMAL);

	// Constructor with numerical priority
	ThreadPool(size_t threads,
		const std::vector<int>& cpu_affinity,
		int custom_priority);

	template<class F, class... Args>
	auto enqueue(F&& f, Args&&... args)
		-> std::future<typename std::result_of<F(Args...)>::type>;

	~ThreadPool();

	// Wait for all tasks to complete (drain)
	void drain();

private:
	// Set thread CPU affinity function
	void set_thread_affinity(std::thread& thread, int cpu_core);

	// Set thread priority function (enumeration version)
	void set_thread_priority(std::thread& thread, Priority priority);

	// Set thread priority function (numerical version)
	void set_thread_priority(std::thread& thread, int custom_priority);

	// Thread collection (for joining)
	std::vector<std::thread> workers;
	// Task queue
	std::queue<std::function<void()>> tasks;

	// Synchronization primitives
	std::mutex queue_mutex;
	std::condition_variable condition;
	bool stop;

	// Stored configurations
	std::vector<int> cpu_affinity_;
	Priority priority_;

	//Task counter and completion condition variable
	std::atomic<size_t> task_count_{ 0 };  // Atomic counter for unfinished tasks
	std::condition_variable task_done_cond_;  // Notification for task completion
};

// Constructor implementation
inline ThreadPool::ThreadPool(size_t threads,
	const std::vector<int>& cpu_affinity,
	Priority priority)
	: stop(false), cpu_affinity_(cpu_affinity), priority_(priority)
{
	// Reserve space to avoid reallocation
	workers.reserve(threads);
	
	for (size_t i = 0; i < threads; ++i)
	{
		workers.emplace_back([this, i]
			{
				for (;;)
				{
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock(this->queue_mutex);
						this->condition.wait(lock,
							[this] { return this->stop || !this->tasks.empty(); });
						if (this->stop && this->tasks.empty())
							return;
						task = std::move(this->tasks.front());
						this->tasks.pop();
					}
					task();
				}
			});
	}
	
	// Set CPU affinity and priority after all threads are created
	for (size_t i = 0; i < workers.size(); ++i)
	{
		// Set CPU affinity (if configured)
		if (!cpu_affinity_.empty())
		{
			int core = cpu_affinity_[i % cpu_affinity_.size()];
			set_thread_affinity(workers[i], core);
		}
		
		// Set thread priority
		set_thread_priority(workers[i], priority_);
	}
}

// constructor (numerical priority)
inline ThreadPool::ThreadPool(size_t threads,
	const std::vector<int>& cpu_affinity,
	int custom_priority)
	: stop(false), cpu_affinity_(cpu_affinity)
{
	// Reserve space to avoid reallocation
	workers.reserve(threads);
	
	for (size_t i = 0; i < threads; ++i)
	{
		workers.emplace_back([this]
			{
				for (;;)
				{
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock(this->queue_mutex);
						this->condition.wait(lock,
							[this] { return this->stop || !this->tasks.empty(); });
						if (this->stop && this->tasks.empty())
							return;
						task = std::move(this->tasks.front());
						this->tasks.pop();
					}
					task();  // Execute task
				}
			});
	}
	
	// Set CPU affinity and priority after all threads are created
	for (size_t i = 0; i < workers.size(); ++i)
	{
		// Set CPU affinity (if configured)
		if (!cpu_affinity_.empty())
		{
			int core = cpu_affinity_[i % cpu_affinity_.size()];
			set_thread_affinity(workers[i], core);
		}
		
		// Set thread priority (numerical version)
		set_thread_priority(workers[i], custom_priority);
	}
}

// Wait for all tasks to complete (drain)
inline void ThreadPool::drain()
{
	std::unique_lock<std::mutex> lock(queue_mutex);
	// Wait for task counter to reach zero (efficient waiting via condition variable)
	task_done_cond_.wait(lock, [this] { return task_count_ == 0; });
}

// CPU affinity setting implementation (platform-specific)
inline void ThreadPool::set_thread_affinity(std::thread& thread, int cpu_core)
{
#if defined(_WIN32) // Windows implementation
	DWORD_PTR mask = static_cast<DWORD_PTR>(1) << cpu_core;
	SetThreadAffinityMask(reinterpret_cast<HANDLE>(thread.native_handle()), mask);
#elif defined(__linux__) // Linux implementation
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(cpu_core, &cpuset);
	pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpuset);
#endif
}

// Thread priority setting implementation (platform-specific)
inline void ThreadPool::set_thread_priority(std::thread& thread, Priority priority)
{
#if defined(_WIN32) // Windows implementation
	int win_priority;
	switch (priority)
	{
	case Priority::LOW: win_priority = THREAD_PRIORITY_BELOW_NORMAL; break;
	case Priority::NORMAL: win_priority = THREAD_PRIORITY_NORMAL; break;
	case Priority::HIGH: win_priority = THREAD_PRIORITY_ABOVE_NORMAL; break;
	case Priority::REALTIME: win_priority = THREAD_PRIORITY_TIME_CRITICAL; break;
	default: win_priority = THREAD_PRIORITY_NORMAL;
	}
	SetThreadPriority(reinterpret_cast<HANDLE>(thread.native_handle()), win_priority);

#elif defined(__linux__) // Linux implementation
	int policy;
	struct sched_param param;

	// Get current scheduling policy
	pthread_getschedparam(thread.native_handle(), &policy, &param);

	// Set different scheduling policies and priorities based on priority level
	switch (priority)
	{
	case Priority::LOW:
		param.sched_priority = sched_get_priority_min(SCHED_OTHER);
		fprintf(stderr, "LOW sched_priority:%d\n", sched_get_priority_min(SCHED_OTHER));
		break;
	case Priority::NORMAL:
		// Keep default settings
		break;
	case Priority::HIGH:
		policy = SCHED_RR;
		param.sched_priority = (sched_get_priority_min(SCHED_RR) +
			sched_get_priority_max(SCHED_RR)) / 2;
		fprintf(stderr, "HIGH sched_priority:%d\n", (sched_get_priority_min(SCHED_RR) +
			sched_get_priority_max(SCHED_RR)) / 2);
		break;
	case Priority::REALTIME:
		policy = SCHED_FIFO;
		param.sched_priority = sched_get_priority_max(SCHED_FIFO);
		fprintf(stderr, "REALTIME sched_priority:%d\n", sched_get_priority_max(SCHED_FIFO));
		break;
	}

	// Set scheduling policy
	pthread_setschedparam(thread.native_handle(), policy, &param);
#endif
}

// Task enqueue (modified: added task counting)
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
-> std::future<typename std::result_of<F(Args...)>::type>
{
	using return_type = typename std::result_of<F(Args...)>::type;

	auto task = std::make_shared<std::packaged_task<return_type()>>(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...)
	);

	std::future<return_type> res = task->get_future();
	{
		std::unique_lock<std::mutex> lock(queue_mutex);

		if (stop)
			throw std::runtime_error("enqueue on stopped ThreadPool");

		// Increase counter when enqueuing
		task_count_++;
		
		// Wrap task to decrease counter and notify after execution
		tasks.emplace([task, this]()
			{
				(*task)();
				// Decrease counter and notify in a thread-safe manner
				{
					std::unique_lock<std::mutex> lock(this->queue_mutex);
					task_count_--;
					if (task_count_ == 0)
					{
						task_done_cond_.notify_all();  // Notify all waiting drain() calls
					}
				}
			});
	}
	condition.notify_one();
	return res;
}

// Thread priority setting (numerical version)
inline void ThreadPool::set_thread_priority(std::thread& thread, int custom_priority)
{
#if defined(_WIN32)
	// Windows priority range: THREAD_PRIORITY_LOWEST(-2) to THREAD_PRIORITY_TIME_CRITICAL(15)
	if (custom_priority >= -2 && custom_priority <= 15)
	{
		SetThreadPriority(reinterpret_cast<HANDLE>(thread.native_handle()), custom_priority);
	}
	else
	{
		std::cerr << "Windows: Invalid priority value (range -2~15)" << std::endl;
	}
#elif defined(__linux__)
	// Linux uses SCHED_RR policy (priority 1~99)
	struct sched_param param;
	param.sched_priority = custom_priority;
	if (custom_priority >= 1 && custom_priority <= 99)
	{
		pthread_setschedparam(thread.native_handle(), SCHED_RR, &param);
	}
	else
	{
		std::cerr << "Linux: Invalid priority value (range 1~99)" << std::endl;
	}
#endif
}

// Destructor implementation
inline ThreadPool::~ThreadPool()
{
	{
		std::unique_lock<std::mutex> lock(queue_mutex);
		stop = true;
	}
	condition.notify_all();
	for (std::thread& worker : workers)
		worker.join();
}

#endif