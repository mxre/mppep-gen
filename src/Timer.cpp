/**
 * \file
 * \brief
 *
 * \author Max Resch
 * \date 2012-12-21
 */

#include "Timer.hpp"
#include <cassert>

#if defined ( __unix__)
#include <sys/resource.h>
#elif defined (_WIN32)
#include <windows.h>
#endif

using namespace std;

Timer Timer::total(true);

void Timer::start ()
{
	unique_lock<mutex>(atomic);
	if (!started) {
		t = getClocks();
		started = true;
	}
}

const CPUTime Timer::stop ()
{
	unique_lock<mutex>(atomic);
	if (started) {
		_elapsed = getClocks() - t;
		started = false;
	}
	return _elapsed;
}

const CPUTime Timer::elapsed() const
{
	unique_lock<mutex>(atomic);
	if (started)
		return getClocks() - t;
	else
		return _elapsed;
}

CPUTime Timer::getClocks()
{
	double sys = 0;
	double user = 0;
	double wall = 0;

#if defined(__unix__)
	struct rusage usage;
	int res = getrusage(RUSAGE_SELF, &usage);
	if (res < 0) assert(false);
	// process time
	user = usage.ru_utime.tv_sec;
	user += 1.0e-6*((double) usage.ru_utime.tv_usec);
	// OS time on behalf of process
	sys = usage.ru_stime.tv_sec;
	sys += 1.0e-6*((double) usage.ru_stime.tv_usec);
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	wall = ts.tv_sec + 1.0e-9*((double)ts.tv_nsec);
#elif defined(_WIN32)
	HANDLE hProcess = GetCurrentProcess();
	FILETIME ftCreation, ftExit, ftKernel, ftUser;
	GetProcessTimes(hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser);
	user = 1.0e-7*((double) (ftUser.dwLowDateTime | ((unsigned long long) ftUser.dwHighDateTime << 32)));
	sys = 1.0e-7*((double) (ftKernel.dwLowDateTime | ((unsigned long long) ftKernel.dwHighDateTime << 32)));
	LARGE_INTEGER cnt, freq;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&cnt);
	wall = (double) cnt.QuadPart / freq.QuadPart;
#endif
	return CPUTime(user, sys, wall);

}

Timer::Timer (bool autostart) : started (false)
{
	if (autostart)
		start();
}

Timer::~Timer ()
{
}
