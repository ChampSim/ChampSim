/*
 * This file defines a specalized bitset data structure that uses 64 bit
 * words to store bits in a set, but does something special for empty and
 * singleton sets to make it faster.
 */

#ifndef __SET_H
#define __SET_H
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define TYPE	unsigned short int
#define MAX_SIZE	ROB_SIZE

class danbitset {
	bool
		_empty, 	// this set is empty
		_single;	// this set is a singleton
	TYPE 
		_singlevalue;	// the singleton value, if any


	// the bits representing the set

	unsigned long long int 
		bits[MAX_SIZE/64];

	// set a bit in the bits

	void setbit (TYPE x) {
		int word = x >> 6;
		int bit = x & 63;
		bits[word] |= 1ull << bit;
	}

public:

	// constructor

	danbitset (void) {
		_empty = true;
		_single = false;
		_singlevalue = 0;
	}

	// destructor

	~danbitset (void) {
	}

	// read _single
	bool single (void) { return _single; }

	// read _singevalue
	TYPE singlevalue (void) { return _singlevalue; }

	// read _empty
	bool empty (void) { return _empty; }

	// insert a value into the set

	void insert (TYPE x) {
		//assert (x < MAX_SIZE);

		// if the set is empty...
		if (_empty) {
			// now it's not empty

			_empty = false;

			// it has a single value

			_singlevalue = x;
			_single = true;

			// and we're done

			return;
		} 

		// if the set is a singleton...

		if (_single) {

			// if we're not inserting the same value...

			if (x != _singlevalue) {
 				// then it's no longer a singleton
				_single = false;

				// clear out the bits (we don't do this
				// in the constructor to save time)

				memset (bits, 0, sizeof (bits));

				// set the value that we know

				setbit (_singlevalue);
			} 
				// if it was the same value, we're done
				else return;
		}

		// set the value
		setbit (x);
	}

	// search the set for a value

	bool search (TYPE x) {
		//assert (x < MAX_SIZE);

		// empty?
		if (_empty)
			return false;

		// singleton?
		else if (_single) 
			return x == _singlevalue;

		// none of those; extract the bit

		int word = x >> 6;
		int bit = x & 63;
		return (bits[word] >> bit) & 1;
	}

	// this set becomes the union of itself and the other set

	void join (danbitset & other, int n) {

		// special rules for special sets

		if (other._empty) return;
		if (other._single) {
			insert (other._singlevalue);
			return;
		}
		if (_single) {
			if (other._singlevalue != _singlevalue)
				insert (other._singlevalue);
			else
				return;
		}

		// lim is the next multiple of 64

		int lim = ((n | 63) + 1) / 64;

		for (int i=0; i<lim; i++) {
			// bitwise OR the other bits into this set

			bits[i] |= other.bits[i];

			// if our bits aren't false, then this set isn't empty

			_empty &= !bits[i];
		}
	}

};

typedef danbitset myset;

// this little macro iterates over either the whole set or just the single member
// it's not specialized for the empty set because this seems to not happen often
// enough in ChampSim

#define ITERATE_SET(i,a,n) \
	TYPE lo, hi; \
	if ((a).single()) { \
		lo = (a).singlevalue(); \
		hi = lo+1; \
		if (lo >= n) { \
			lo = 0; \
			hi = 0; \
		} \
	} else { \
		lo = 0; \
		hi = (n); \
	} \
	for (TYPE (i)=lo; i<hi; i++) if ((a).search(i))

#endif
