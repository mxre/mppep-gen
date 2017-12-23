/**
 * \file
 * \brief
 *
 * \author Max Resch
 * \date 2012-12-21
 */

#ifndef TIMER_H_
#define TIMER_H_

#include "def.hpp"
#include "CPUTime.hpp"

#include <mutex>
#include <tuple>

class Timer
{
public:

	Timer (bool autostart = false);

	virtual ~Timer ();

	void start ();

	const CPUTime elapsed () const;

	const CPUTime stop ();

	static Timer total;

	static CPUTime getClocks();

private:
	std::mutex atomic;

	CPUTime t, _elapsed;

	bool started;
};

#endif /* TIMER_H_ */
