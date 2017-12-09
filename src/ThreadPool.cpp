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

#if __linux__
#include <sys/prctl.h>
#endif
#include "ThreadPool.hpp"

using namespace std;

// the constructor just launches some amount of workers
ThreadPool::ThreadPool (size_t threads) :
			stop(false)
{
	if (threads == 0)
		threads = thread::hardware_concurrency();
	for (size_t i = 0; i < threads; ++i)
	{
		workers.push_back(ThreadPool::thread(Worker(*this)));
	}
}

size_t ThreadPool::queued()
{
	return tasks.size();
}

void ThreadPool::shutdown ()
{
	stop = true;
	condition.notify_all();
	for (size_t i = 0; i < workers.size(); ++i)
		workers[i].join();
}

ThreadPool::~ThreadPool ()
{
	if (!stop)
		shutdown();
}

ThreadPool::Worker::Worker (ThreadPool& s) :
			pool(s)
{
}

void ThreadPool::Worker::operator() ()
{
#if __linux__
	prctl(PR_SET_NAME, "TPworker");
#endif
	while (true)
	{
		unique_lock<mutex> lock(pool.queue_mutex);
		while (!pool.stop && pool.tasks.empty())
			pool.condition.wait(lock);
		if (pool.stop && pool.tasks.empty())
			return;
		function<void ()> task(std::move(pool.tasks.front()));
		pool.tasks.pop();
		lock.unlock();
		task();
	}
}
