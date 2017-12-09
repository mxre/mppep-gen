/**
 * \file
 * \brief
 *
 * \author Max Resch
 * \date 10.07.2013
 */

#ifndef TAXON_HPP_
#define TAXON_HPP_

#include <string>
#include <cstddef>
#include <ostream>
#include <memory>

class Taxon
{
public:
	Taxon (const Taxon&);
	Taxon (const std::size_t);
	Taxon (const std::string&);
	virtual ~Taxon ();

	bool& operator[] (std::size_t pos) noexcept;
	bool operator[] (std::size_t pos) const noexcept;

	bool& at (std::size_t pos) noexcept;
	bool at (std::size_t pos) const noexcept;

	void flip(const std::size_t pos);

	bool operator== (const Taxon&) const noexcept;
	bool operator< (const Taxon&) const noexcept;
	Taxon& operator= (const Taxon& other) = default;

	/// get hamming distance
	const int distance (const Taxon& other) const;

	/// get first occurance of diverging bit
	const int difference (const Taxon& other) const;
	const std::size_t length () const noexcept;
	const std::size_t hash() const noexcept;
	void remove(const std::size_t);
	void resize();

	bool Terminal;

	int Index;

private:
	bool* internal;
	std::size_t size;
};

std::ostream& operator<< (std::ostream&, const Taxon&);

#endif /* TAXON_HPP_ */
