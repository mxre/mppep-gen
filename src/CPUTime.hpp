/**
 * \file
 * \brief
 *
 * \author Max Resch
 * \date 21.04.2013
 */

#ifndef TIME_HPP_
#define TIME_HPP_

#include <chrono>
#include <ostream>

typedef std::chrono::duration<double, std::ratio<1>> seconds;

struct CPUTime
{
	typedef std::chrono::nanoseconds _time;

	CPUTime ();
	CPUTime (const double);
	CPUTime (const double _user, const double _sys, const double _wall);

	_time wall;
	_time user;
	_time sys;

	CPUTime& operator+= (const CPUTime&);
	CPUTime operator+ (const CPUTime&) const;
	CPUTime operator- (const CPUTime&) const;

	double getSeconds() const;
};

std::ostream& operator<< (std::ostream& os, const CPUTime t);

#endif /* TIME_HPP_ */
