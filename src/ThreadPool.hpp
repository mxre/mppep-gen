/*
 * Copyright (c) 2012 Jakob Progsch
 * Copyright (c) 2013 Max Resch
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *  1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 *  2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 *  3. This notice may not be removed or altered from any source
 * distribution.
 *
 */

#ifndef THREAD_POOL_HPP_
#define THREAD_POOL_HPP_

#include "def.hpp"
#include <vector>
#include <queue>
#include <memory>
#include <mutex>
#include <future>
#include <thread>
#include <stdexcept>
#include <condition_variable>

class ThreadPool
{
public:
	ThreadPool (size_t);

	template <class T, class F>
	std::future<T> enqueue (const F f);

	size_t queued();
	void shutdown ();
	virtual ~ThreadPool ();

	typedef std::thread thread;

private:
	friend class Worker;

	// our worker thread objects
	class Worker
	{
	public:
		Worker (ThreadPool &s);
		void operator() ();
	private:
		ThreadPool &pool;
	};

	// need to keep track of threads so we can join them
	std::vector<thread> workers;
	// the task queue
	std::queue<std::function<void ()>> tasks;

	// Synchronisation
	std::mutex queue_mutex;
	std::condition_variable condition;
	bool stop;
};

template <class T, class F>
std::future<T> ThreadPool::enqueue (const F f)
{
	// don't allow enqueueing after stopping the pool
	if (stop)
	{
		throw std::runtime_error("enqueue on stopped ThreadPool");
	}

	auto task = std::make_shared<std::packaged_task<T ()> >(f);
	std::future<T> res = task->get_future();
	{
		std::unique_lock<std::mutex> lock(queue_mutex);
		tasks.push([task]()
		{
			(*task)();
		});
	}
	condition.notify_one();
	return res;
}

#endif // THREAD_POOL_HPP_
