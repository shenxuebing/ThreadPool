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

    return 0;
}
