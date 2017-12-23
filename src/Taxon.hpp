/**
 * \file
 * \brief
 *
 * \author Max Resch
 * \date 10.07.2013
 */

#ifndef TAXON_HPP_
#define TAXON_HPP_

#include "def.hpp"
#include <string>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>

class Taxon
{
public:
	Taxon (const Taxon&);
	Taxon (const std::size_t);
	Taxon (const char * __restrict, const std::size_t);
	virtual ~Taxon ();

	bool& operator[] (std::size_t pos) noexcept;
	bool operator[] (std::size_t pos) const noexcept;

	bool& at (std::size_t pos) noexcept;
	bool at (std::size_t pos) const noexcept;

	void flip(const std::size_t pos);

	void print(FILE* __restrict);

	bool operator== (const Taxon&) const noexcept;
	bool operator< (const Taxon&) const noexcept;
	Taxon& operator= (const Taxon& other) = default;

	/// get hamming distance
	const std::size_t distance (const Taxon& other) const;

	/// get first occurance of diverging bit
	const std::size_t difference (const Taxon& other) const;
	const std::size_t length () const noexcept;
	const std::size_t hash() const noexcept;
	void remove(const std::size_t);
	void resize();

	bool Terminal;

	uint64_t Index;

private:
	typedef bool __internal_t;
	__internal_t* internal;
	std::size_t size;
};

#endif /* TAXON_HPP_ */
