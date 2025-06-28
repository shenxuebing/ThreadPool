#include <iostream>
#include <vector>
#include <chrono>

#include "ThreadPool.h"

int main()
{
    
    ThreadPool pool(4);
    std::vector< std::future<int> > results;

    for(int i = 0; i < 8; ++i) {
        results.emplace_back(
            pool.enqueue([i] {
                std::cout << "hello " << i << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::cout << "world " << i << std::endl;
                return i*i;
            })
        );
    }
	pool.drain();  // wait for all tasks to complete

    for(auto && result: results)
        std::cout << result.get() << ' ';
    std::cout << std::endl;
    
    std::vector< std::future<int> > results2;
	std::vector<int> cores = { 0, 1 };
	ThreadPool pool2(4, cores, ThreadPool::Priority::HIGH);
	for (int i = 0; i < 8; ++i)
	{
		results2.emplace_back(
			pool2.enqueue([i]
				{
					std::cout << "hello " << i << std::endl;
					std::this_thread::sleep_for(std::chrono::seconds(1));
					std::cout << "world " << i << std::endl;
					return i * i;
				})
		);
	}
	pool2.drain();  // wait for all tasks to complete

	for (auto&& result : results2)
		std::cout << result.get() << ' ';
	std::cout << std::endl;

	ThreadPool pool3(std::thread::hardware_concurrency(),
		{},
		ThreadPool::Priority::REALTIME);
	std::vector< std::future<int> > results3;
	
	for (int i = 0; i < 8; ++i)
	{
		results3.emplace_back(
			pool3.enqueue([i]
				{
					std::cout << "hello " << i << std::endl;
					std::this_thread::sleep_for(std::chrono::seconds(1));
					std::cout << "world " << i << std::endl;
					return i * i;
				})
		);
	}

	for (auto&& result : results3)
		std::cout << result.get() << ' ';
	std::cout << std::endl;
	
	pool3.drain();  // wait for all tasks to complete

#if defined(_WIN32)
	ThreadPool pool4(4, { 0,1 }, 10);  // Windows -2~15
#elif defined(__linux__)
	ThreadPool pool4(4, { 0,1 }, 50);  // Linux 1~99
#endif
    for (int i = 0; i < 8; ++i)
    {
        results.emplace_back(
            pool4.enqueue([i]
                {
                    std::cout << "hello " << i << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    std::cout << "world " << i << std::endl;
                    return i * i;
                })
        );
    }
	pool4.drain();  // wait for all tasks to complete
    return 0;
}
