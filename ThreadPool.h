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

// ƽ̨��ص�CPU�׺��Ժ����ȼ�����
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <sched.h>
#include <pthread.h>
#endif

class ThreadPool
{
public:
	// �߳����ȼ�ö��
	enum class Priority
	{
		LOW,
		NORMAL,
		HIGH,
		REALTIME
	};

	// ���캯�����ӿ�ѡ������CPU�׺��Ժ����ȼ�
	ThreadPool(size_t threads,
		const std::vector<int>& cpu_affinity = {},
		Priority priority = Priority::NORMAL);

	template<class F, class... Args>
	auto enqueue(F&& f, Args&&... args)
		-> std::future<typename std::result_of<F(Args...)>::type>;

	~ThreadPool();

private:
	// CPU�׺������ú���
	void set_thread_affinity(std::thread& thread, int cpu_core);

	// �߳����ȼ����ú���
	void set_thread_priority(std::thread& thread, Priority priority);
	// need to keep track of threads so we can join them
	std::vector< std::thread > workers;
	// the task queue
	std::queue< std::function<void()> > tasks;

	// synchronization
	std::mutex queue_mutex;
	std::condition_variable condition;
	bool stop;

	// �洢����
	std::vector<int> cpu_affinity_;
	Priority priority_;
};
// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads,
	const std::vector<int>& cpu_affinity, Priority priority)
	: stop(false), cpu_affinity_(cpu_affinity), priority_(priority)
{
	for (size_t i = 0; i < threads; ++i)
	{
		workers.emplace_back([this, i]
			{
				// ����CPU�׺��ԣ���������ˣ�
				if (!cpu_affinity_.empty())
				{
					int core = cpu_affinity_[i % cpu_affinity_.size()];
					set_thread_affinity(workers[i], core);
				}

				// �����߳����ȼ�
				set_thread_priority(workers[i], priority_);

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
}

// CPU�׺�������ʵ�֣�ƽ̨��أ�
inline void ThreadPool::set_thread_affinity(std::thread& thread, int cpu_core)
{
#if defined(_WIN32) // Windowsʵ��
	DWORD_PTR mask = static_cast<DWORD_PTR>(1) << cpu_core;
	SetThreadAffinityMask(thread.native_handle(), mask);
#elif defined(__linux__) // Linuxʵ��
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(cpu_core, &cpuset);
	pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpuset);
#endif
}

// �߳����ȼ�����ʵ�֣�ƽ̨��أ�
inline void ThreadPool::set_thread_priority(std::thread& thread, Priority priority)
{
#if defined(_WIN32) // Windowsʵ��
	int win_priority;
	switch (priority)
	{
	case Priority::LOW: win_priority = THREAD_PRIORITY_BELOW_NORMAL; break;
	case Priority::NORMAL: win_priority = THREAD_PRIORITY_NORMAL; break;
	case Priority::HIGH: win_priority = THREAD_PRIORITY_ABOVE_NORMAL; break;
	case Priority::REALTIME: win_priority = THREAD_PRIORITY_TIME_CRITICAL; break;
	default: win_priority = THREAD_PRIORITY_NORMAL;
	}
	SetThreadPriority(thread.native_handle(), win_priority);

#elif defined(__linux__) // Linuxʵ��
	int policy;
	struct sched_param param;

	// ��ȡ��ǰ���Ȳ���
	pthread_getschedparam(thread.native_handle(), &policy, &param);

	// �������ȼ����ò�ͬ�ĵ��Ȳ��Ժ����ȼ�
	switch (priority)
	{
	case Priority::LOW:
		param.sched_priority = sched_get_priority_min(SCHED_OTHER);
		fprintf(stderr, "LOW sched_priority:%d\n", sched_get_priority_min(SCHED_OTHER));
		break;
	case Priority::NORMAL:
		// ����Ĭ������
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

	// �����µ��Ȳ���
	pthread_setschedparam(thread.native_handle(), policy, &param);
#endif
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
-> std::future<typename std::result_of<F(Args...)>::type>
{
	using return_type = typename std::result_of<F(Args...)>::type;

	auto task = std::make_shared< std::packaged_task<return_type()> >(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...)
	);

	std::future<return_type> res = task->get_future();
	{
		std::unique_lock<std::mutex> lock(queue_mutex);

		// don't allow enqueueing after stopping the pool
		if (stop)
			throw std::runtime_error("enqueue on stopped ThreadPool");

		tasks.emplace([task]() { (*task)(); });
	}
	condition.notify_one();
	return res;
}

// the destructor joins all threads
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