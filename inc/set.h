/*
 * This file defines a specalized bitset data structure that uses 64 bit
 * words to store bits in a set, but does something special for small
 * sets to make it faster.
 */

#ifndef __SET_H
#define __SET_H
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define TYPE	unsigned short int
//#define MAX_SIZE	ROB_SIZE
// sethpugsley - changed this from ROB_SIZE to allow for non-power-of-2 ROB sizes, like real CPUs have
// but MAX_SIZE here still requires a power-of-2 number
#define MAX_SIZE	512

// tuned empirically

#define SMALL_SIZE	13
#define SMALLER_SIZE	6

class fastset {
	union {
		// values for a small set
		TYPE 
			values[SMALL_SIZE];

		// the bits representing the set
		unsigned long long int 
			bits[MAX_SIZE/64];
	} data;

	int
		card;		// cardinality of small set

	// set a bit in the bits

	void setbit (TYPE x) {
		int word = x >> 6;
		int bit = x & 63;
		data.bits[word] |= 1ull << bit;
	}

	// get one of the bits

	bool getbit (TYPE x) {
		int word = x >> 6;
		int bit = x & 63;
		return (data.bits[word] >> bit) & 1;
	}

	// insert an item into a small set

	void insert_small (TYPE x) {
		int i;
		for (i=0; i<card; i++) {
			TYPE y = data.values[i];
			if (y == x) return;
			if (y > x) break;
		}
		// x belongs in i; move everything from v[i] through v[n-1]
		// to v[i+1] through v[n]
		for (int j=card-1; j>=i; j--) data.values[j+1] = data.values[j];
		// the loop seems a little faster than memmove
		//memmove (&data.values[i+1], &data.values[i], (sizeof (TYPE) * (card-i)));
		data.values[i] = x;
		card++;
	}


	// do a linear search in a small set

	bool search_small_linear (TYPE x) {
		for (int i=0; i<card; i++) {
			TYPE y = data.values[i];
			if (y > x) return false;
			if (y == x) return true;
		}
		return false;
	}


	// search a small set, specializing for the set size

	bool search_small (TYPE x) {

		// no elements? we're done.

		if (!card) return false;

		// below a certain size linear search is faster

		if (card < SMALLER_SIZE) return search_small_linear (x);

		// do a binary search for the item

		int begin = 0;
		int end = card-1;
		int middle = end/2;
		for (;;) {
			TYPE y = data.values[middle];
			if (x < y) {
				end = middle-1;
			} else if (x > y) {
				begin = middle+1;
			} else return true;
			if (end < begin) break;
			middle = (begin + end) / 2;
			// assert (middle < card && middle >= 0);
		}
		return false;
	}

	// convert a small set into a bitset

	void smalltobit (void) {

		// we have to use a temporary array to hold the small set contents
		// because the small set and bitset occupy the same memory 
	
		TYPE tmp[SMALL_SIZE];
		memcpy (tmp, data.values, sizeof (TYPE) * card);
		memset (data.bits, 0, sizeof (data.bits));
		for (int i=0; i<card; i++) setbit (tmp[i]);
	}

public:

	// constructor

	fastset (void) { card = 0; }

	// destructor

	~fastset (void) { }

	// insert a value into the set

	void insert (TYPE x) {
		//assert (x < MAX_SIZE);

		// if the set is empty...
		if (!card) {
			// now it has a single value

			data.values[card++] = x;

			// and we're done

			return;
		} 

		// if the set is small

		if (card < SMALL_SIZE) {
			insert_small (x);
			if (card == SMALL_SIZE) smalltobit ();
		} else

		// set the value
		setbit (x);
	}

	// search the set for a value

	bool search (TYPE x) {
		//assert (x < MAX_SIZE);

		// empty?
		if (!card) return false;

		// singleton?
		if (card == 1) return data.values[0] == x;

		// small?
		if (card < SMALL_SIZE) return search_small (x);

		// none of those; extract the bit

		return getbit (x);
	}

	// this set becomes the union of itself and the other set
	// (call it "join" because "union" is a C++ keyword)

	void join (fastset & other, int n) {

		// special rules for special sets

		if (!other.card) return;

		if (other.card < SMALL_SIZE) {
			// not too many values in other; just insert them one by one

			for (int i=0; i<other.card; i++) insert (other.data.values[i]);
			return;
		} else if (card < SMALL_SIZE) {
			// here, we know that other is not small, so we
			// know we're going to end up with this as a bit
			// set, so just make it a bit set now and fall 
			// through to the bitwise ANDing
			smalltobit ();
			card = SMALL_SIZE; // fake
			assert (other.card >= SMALL_SIZE);
		}

		// lim is the next multiple of 64

		int lim = ((n | 63) + 1) / 64;

		// bitwise OR the other bits into this set
		for (int i=0; i<lim; i++) data.bits[i] |= other.data.bits[i];
	}

	// expand the entire set into the array v, returning the cardinality

	int expand (TYPE v[], int n) {
		if (!card) return 0;

		// a small set can just be copied

		if (card < SMALL_SIZE) {
			for (int i=0; i<card; i++) v[i] = data.values[i];
			return card;
		}

		// go through the bit array looking for elements

		int k = 0;
		TYPE i;
		for (i=0; i<n; i+=64) {

			// if this 64 bit subset is not empty, copy it into v

			if (data.bits[i/64]) {
				for (TYPE j=0; j<64; j++) {
					TYPE l = i + j;
					if (l < n) {
						if (getbit (l)) v[k++] = l;
					} else break;
				}
			}
		}
		return k;
	}
};

// this little macro iterates over either the whole set or just the single member

#define ITERATE_SET(i,a,n) \
	TYPE expand_##i[n+1]; \
	int card_##i = (a).expand (expand_##i, n); \
	for (int count_##i=0, i=expand_##i[0]; count_##i<card_##i; i=expand_##i[++count_##i])

#endif
