/**
 * \file
 * \brief
 *
 * \author Max Resch
 * \date 10.07.2013
 */

#include "Taxon.hpp"

#include <stdexcept>
#include <string>
#include <cstddef>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <memory>

using namespace std;

void Taxon::remove(const size_t pos)
{
	if (pos >= size)
		throw logic_error("Position out of bounds");
	if (pos < size - 1)
		move(internal + pos + 1, internal + size , internal + pos);
	size = size - 1;
}

void Taxon::resize()
{
  internal = (bool*) realloc(internal, size);
  //internal.resize(size);
}

const size_t Taxon::length () const noexcept
{
	return size;
}

const size_t Taxon::hash () const noexcept
{
	size_t hash = 0;
	for (int i = 0; i < size; i++)
		hash ^= (internal[i] << i);

	return hash;
}

const size_t Taxon::difference (const Taxon& other) const
{
	if (other.size != size)
		throw logic_error("Taxas do not have the same length");

	for (size_t i = 0; i < size; i++)
		if (internal[i] != other.internal[i])
			return i;
	return -1;
}

const size_t Taxon::distance (const Taxon& other) const
{
	if (other.size != size)
		throw logic_error("Taxas do not have the same length");

	size_t distance = 0;

	for (size_t i = 0; i < size; i++)
		if (internal[i] != other.internal[i])
			distance++;
	return distance;

}

bool Taxon::operator< (const Taxon& other) const noexcept
{
	if (other.size != size)
		return false;

	for (size_t i = 0; i < size; i++)
		if (internal[i] != other.internal[i])
			return internal[i];

	return false;
}

void Taxon::flip(const size_t pos)
{
	internal[pos] = !internal[pos];
}

bool Taxon::operator== (const Taxon& other) const noexcept
{
	if (other.size != size)
		return false;
	for (size_t i = 0; i < size; i++)
	{
		if (internal[i] != other.internal[i])
			return false;
	}

	return true;
}

bool& Taxon::operator[] (size_t pos) noexcept
{
	return internal[pos];
}

bool Taxon::operator[] (size_t pos) const noexcept
{
	return internal[pos];
}

bool& Taxon::at (size_t pos) noexcept
{
	return internal[pos];
}

bool Taxon::at (size_t pos) const noexcept
{
	return internal[pos];
}

ostream& operator<< (ostream& os, const Taxon& taxon)
{
	for (size_t i = 0; i < taxon.length(); i++)
		os << (taxon[i] ? '1' : '0');

	return os;
}

Taxon::Taxon (const Taxon& other) :
			Terminal(false),
			Index(0),
			size(other.size)
{
  internal = (bool*) malloc(size);

	for (size_t i = 0; i < size; i++)
	{
		internal[i] = other.internal[i];
	}

}

Taxon::Taxon (const size_t n) :
			Terminal(false),
			Index(0),
			size(n)
{
	internal = (bool*) malloc(size);

	for (size_t i = 0; i < size; i++)
		internal[i] = false;
}

Taxon::Taxon (const char * __restrict bitstring, const size_t __len) :
			Taxon(__len)
{
	for (size_t j = 0; j < size; j++)
	{
		if (bitstring[j] == '1')
			internal[j] = true;
		else if (bitstring[j] == '0')
			internal[j] = false;
		else
			throw out_of_range("Taxon bitstring is neither 0 or 1");
	}

	Terminal = true;
}

Taxon::~Taxon ()
{
	if (internal != nullptr)
	{
		free(internal);
		internal = nullptr;
	}
}
