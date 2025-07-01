ThreadPool
==========

A simple C++11 Thread Pool implementation.

Basic usage:



```c++
// Create thread pools and bind them to CPU cores 0 and 1, and set high priorities
std::vector<int> cores = {0, 1};
ThreadPool pool(4, cores, ThreadPool::Priority::HIGH);

// enqueue and store future
auto result = pool.enqueue([](int answer) { return answer; }, 42);

// get result from future
std::cout << result.get() << std::endl;
```



```c++
// Create a thread pool bound to all cores with real-time priority
ThreadPool pool(std::thread::hardware_concurrency(), 
                {}, 
                ThreadPool::Priority::REALTIME);

// enqueue and store future
auto result = pool.enqueue([](int answer) { return answer; }, 42);

// get result from future
std::cout << result.get() << std::endl;
```



```c++
// Creating a thread pool is not bound to the core and has a normal priority
#if defined(_WIN32)
	ThreadPool pool4(4, { 0,1 }, 10);  // Windows -2~15
#elif defined(__linux__)
	ThreadPool pool4(4, { 0,1 }, 50);  // Linux 1~99
#endif 

// enqueue and store future
auto result = pool.enqueue([](int answer) { return answer; }, 42);


// wait for all tasks to complete
pool4.drain(); 

// get result from future
std::cout << result.get() << std::endl;
```
