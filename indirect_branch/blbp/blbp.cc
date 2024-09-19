#include "msl/fwcounter.h"
#include "ooo_cpu.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>


// blbp.h
#define __BLBP_H

#include <list>
#include <math.h>


static int default_xlat[] = {2, 4, 6, 8, 11, 14, 18, 24,};
static double default_coeffs[] = { 1.3, 1.6, 1.6, 1.5, 1.4, 1.3, 1.1, 1.0 };
static int default_lgsizes[] = { 10, 9, 10, 10, 10, 10, 10, 10, 0, 4, 5, 7, };
static int default_intervals[] = {
	0, 0, 
	0, 13, 
	1, 33, 
	23, 49, 
	44, 85, 
	77, 149, 
	159, 270, 
	252, 630, };

#define LOG2_BTB_SETS	12			// log2 of maximum number of BTB sets
#define BTB_SETS	(1<<LOG2_BTB_SETS)	// number of BTB sets
#define BTB_ASSOC	64			// maximum BTB associativity
#define LOG2_REGIONS	7			// log2 of maximum number of BTB regions
#define REGIONS		(1<<LOG2_REGIONS)	// number of BTB regions
#define REGION_INIT	0x1a4			// region numbers are initialized to this

static unsigned int hash32bit (unsigned int key) {
    key = ~key + (key << 15);
    key = key ^ (key >> 12);
    key = key + (key << 2);
    key = key ^ (key >> 4);
    key = key * 34129;
    key = key ^ (key >> 16);
    return key;
}


// perceptron weights are stored in sign-magnitude format. 
// this class implements an efficient sign-magnitude weight 
// in an unsigned char by keeping the high bit as the sign 
// and the lower 7 bits as the magnitude. this is needlessly 
// efficient.

class sign_magnitude {
	unsigned char bits;
public:
	// initially 0
	sign_magnitude (void) { bits = 0; }

	// return the high bit as the sign
	bool sign (void) { return !!(bits & 0x80); }

	// return the lower 7 bits as the magnitude
	int magnitude (void) { return bits & 0x7f; }

	// increment or decrement the value
	void incdec (bool inc, int max) {
		if (sign() == inc) {
			if (magnitude() == 0)
				bits = !inc * 0x80; // positive or negative 0
			else
				bits--;
		} else {
			if (magnitude() < max) bits++;
		}
	}

};

// branch target buffer entry class

struct btb_entry {

	// partial tag from PC for identifying branch. 
	// we can vary the bit width to match hardware budget; 
	// a false positive match is OK because it only affects accuracy

	unsigned int partial_tag;

	// re-reference interval prediction value for RRIP replacement; 2 bits

	int rrpv;

	// targets are represented in a compressed format. there are a number of 
	// "regions" kept in the region_entries array. target are represented as 
	// one of these regions plus an offset. the region array is indexed by 
	// region_index. the number of regions is small and the number of bits 
	// in offset is less than a full target, so we can represent a 64-bit 
	// target in ~27 or so bits. this trick is adapted from Seznec.

	unsigned int region_index, offset;

	btb_entry (void) { }
};

class perceptron__predictor {
    //#define GHIST_WORD_TYPE	unsigned long long int
    #define GHIST_WORD_TYPE	__uint128_t
    #define GHIST_WORD_BITS	(sizeof(GHIST_WORD_TYPE)*8)
    #define MAX_TABLES	9				// maximum number of weights tables
    #define MAX_HISTORY	1536		// maximum history length
    #define MAX_N		12				// maximum number of address bits to predict
    #define GHIST_WORDS	(MAX_HISTORY/sizeof(GHIST_WORD_TYPE))			// number of words we need to represent ghist
    #define LG2_PERC_TAB_SIZE	11			// log2 of maximum weights table size
    #define PERC_TAB_SIZE		(1<<LG2_PERC_TAB_SIZE)	// maxumum weights table size

    #define ASSOC	32

    unsigned int recency_stack[ASSOC];
    int assoc, rpos_idx;
    unsigned int ghist_words;

    // the global history represented as an array of big words

    GHIST_WORD_TYPE ghist[GHIST_WORDS], backghist[GHIST_WORDS];

    // adaptively trained thresholds and training counters, per address bit predicted
    int theta[MAX_N], tc[MAX_N];

    // training speed for adaptive threshold algorithm
    int speed;

    // the weights
    sign_magnitude weights[MAX_TABLES][MAX_N][PERC_TAB_SIZE];

    // indices into the tables for the current prediction
    unsigned int indices[MAX_TABLES];

    // coefficients to mutiply value read out from each table
    double *coeffs;

    // index the global history, getting ghist bits a..b
    // (pretend this is magic; note the recursion, and do not attempt to 
    // look behind the curtain)
    GHIST_WORD_TYPE idx (GHIST_WORD_TYPE *v, int a, int b) {
        unsigned int i = a / GHIST_WORD_BITS;
        if (i != (b-1)/GHIST_WORD_BITS) {
            int c0 = (a | (GHIST_WORD_BITS-1)) + 1;
            GHIST_WORD_TYPE w1 = idx (v, a, c0);
            GHIST_WORD_TYPE w2 = idx (v, c0, b);
            return (w2 << (c0-a)) | w1;
        } else {
            GHIST_WORD_TYPE w = v[i];
            int bits = b - a;
            int s = a & (GHIST_WORD_BITS-1);
            GHIST_WORD_TYPE mask = (1<<bits)-1;
            w = (w >> s) & mask;
            return w;
        }
    }

    // hash the global history from start to end yielding a certain 
    // number of bits. the hash is just the xor of successive bits-wide 
    // chunks of the ghist
    unsigned int fold_ghist (int start, int end, int bits) {
        assert (start >= 0);
        assert (end >= 0);
        int a = start;
        int b = end + 1;
        GHIST_WORD_TYPE x = 0;
        if (b-a < bits) return idx (ghist, a, b);
        int j;
        int j2 = b - bits;
        for (j=a; j<j2; j+=bits) x ^= idx (ghist, j, j+bits);
        if (j < b) x ^= idx (ghist, j, b);
        return x;
    }
    unsigned int fold_backghist (int start, int end, int bits) {
        return 0;
        int a = start;
        int b = end + 1;
        GHIST_WORD_TYPE x = 0;
        if (b-a < bits) return idx (backghist, a, b);
        int j;
        int j2 = b - bits;
        for (j=a; j<j2; j+=bits) x ^= idx (backghist, j, j+bits);
        if (j < b) x ^= idx (backghist, j, b);
            return x;
    }

        

    GHIST_WORD_TYPE ghist_accumulator;
    int ghist_count;

public:
	// send all the recently accumulated ghist bits to the ghist

	void flush_ghist (void) {
		if (!ghist_count) return;
#if 0
		for (int j=ghist_count-1; j>=0; j--) {
			bool taken = !!(ghist_accumulator & (1<<j));
			for (int i=ghist_words-1; i > 0; i--)
				ghist[i] = (ghist[i] << 1) | (ghist[i-1] >> 63);
			ghist[0] = (ghist[0] << 1);
			ghist[0] |= taken;
		}
#endif
		for (int i=ghist_words-1; i > 0; i--)
			ghist[i] = (ghist[i] << ghist_count) | (ghist[i-1] >> (GHIST_WORD_BITS-ghist_count));
		ghist[0] = (ghist[0] << ghist_count);
		ghist[0] |= ghist_accumulator & ((1<<ghist_count)-1);
		ghist_count = 0;
	}

	// update the global history with a branch outcome. shift all the 
	// bits over and insert the new bit.

	void update_ghist (bool taken) {
		ghist_accumulator <<= 1;
		ghist_accumulator |= taken;
		ghist_count++;
		//flush_ghist ();
		assert ((unsigned) ghist_count < GHIST_WORD_BITS);
#if 0
		for (int i=ghist_words-1; i > 0; i--)
			ghist[i] = (ghist[i] << 1) | (ghist[i-1] >> 63);
		ghist[0] = (ghist[0] << 1);
		ghist[0] |= taken;
#endif
	}

	void update_backghist (bool taken) {
		if (rpos_idx < 0) return;
		for (int i=GHIST_WORDS-1; i > 0; i--)
			backghist[i] = (backghist[i] << 1) | (backghist[i-1] >> (GHIST_WORD_BITS-1));
		backghist[0] = (backghist[0] << 1);
		backghist[0] |= taken;
	}

#define NLHIST	256

	// local histories
	unsigned int lhist[NLHIST];

	// number of local histories to keep
	int nh;

	// local history length
	int lhlength;

	// local history index (kept for updating)
	int lhx;

	// old value of pc (for updating)
	unsigned int opc;

	// the intervals for hashing ghist. the array contains pairs 
	// of integers giving the start and stop of a global history interval. 
	// so for table i, the interval is from position intervals[i*2] to 
	// position intervals[i*2+1]. the first table is indexed with local 
	// history so the first interval is ignored.
	int *intervals;

	// number of tables, i.e. number of intervals plus one for local history
	int ntables;

	// maximum weight value
	int maxweight;

	// number of address bits to predict
	int n;

	// perceptron outputs, per address bit predicted
	int youts[MAX_N];

	// transfer function indexed by weight magnitude
	int *xlat;

	// which bit of the target address to record in the local history
	int lbit;

	// array of log2 of table sizes
	int *lgsizes;

	double threshold, fudge;

	// constructor for the perceptron predictor
	perceptron__predictor (
		double _fudge,
		int _n, 		// number of address bits to predict, i.e. number of separate predictors
		int *_intervals,	// intervals for hashing global history
		int _ntables,		// number of tables per predictor
		int _speed,		// learning speed for adaptive threshold training
		int _bpw,		// bits per weight (sort of deprecated; xlat assumes it's 4)
		int _lbit,		// which bit of the target PC should be used to record local history
		int _init_theta,	// initial value of theta
		int *_xlat,		// 8 elements; transfer function
		int _lhlength,		// local history length
		int _nh,		// number of local histories
		double *_coeffs,	// ntables elements; coefficients, one per table
		int *_lgsizes,	// ntables elements; log2 of the size of each table
		int _assoc,
		int _rpos_idx) {

		fudge = _fudge;
		ghist_accumulator = 0;
		ghist_count = 0;
		assoc = _assoc;
		rpos_idx = _rpos_idx;
		// initialize runtime constants
		coeffs = _coeffs;
		lgsizes = _lgsizes;
		lhlength = _lhlength;
		nh = _nh;
		xlat = _xlat;
		n = _n;
		lbit = _lbit;
		maxweight = (1<<(_bpw-1))-1;
		speed = _speed;
		ntables = _ntables;
		intervals = _intervals;
		memset (recency_stack, 0, sizeof (recency_stack));
		ghist_words = 0;
		for (int i=0; i<ntables; i++) 
			if (ghist_words < (unsigned) intervals[i*2+1]) ghist_words = intervals[i*2+1];
		ghist_words /= sizeof(GHIST_WORD_TYPE);
		ghist_words++;
		//printf ("ghist_words is %d!\n", ghist_words);
		// initialize thetas and adaptive training counters
		for (int k=0; k<n; k++) {
			theta[k] = _init_theta;
			tc[k] = 0;
		}

		// zero out local history
		memset (lhist, 0, sizeof (lhist));
	}

	unsigned int hash_recencypos (unsigned int pc, int l) {

        // search for the PC

        for (int i=0; i<l; i++) {
            if (recency_stack[i] == pc) {
                int r = i;
                return r * 5;
            }
        }
		return 12345;
    }

	void insert_recency (unsigned int pc) {
        int i = 0;
        for (i=0; i<assoc; i++) {
            if (recency_stack[i] == pc) break;
        }
        if (i == assoc) {
            i = assoc-1;
            recency_stack[i] = pc;
        }
        int j;
        unsigned int b = recency_stack[i];
        for (j=i; j>=1; j--) recency_stack[j] = recency_stack[j-1];
        recency_stack[0] = b;
    }

	// heart of the prediction code: given a PC, produce an array of youts that predict the address bits
	int *predict (unsigned int pc, int fix, int rotate, int cut) {

		// zero out youts
		memset (youts, 0, sizeof (int) * n);

		// remember the PC for training later
		opc = pc;

		// hash the PC to get an index into the local histories
		lhx = hash32bit (pc) % nh;

		// to spread out accesses to tables more uniformly, we rotate 
		// which table corresponds to which feature according to a 
		// hash of the PC
		unsigned int ii = hash32bit (pc) * rotate;

		// for each set of tables (i/ii), hash a feature to get an index (j) into 
		// the table, then add up the selected weights for each address bit (k)

		for (int i=0; i<ntables; i++) {
		//for (int i=0; i<cut; i++) {

			// j is the index into the i'th set of tables
			unsigned int j;

			// the first tables are for local history
			if (i == 0) {
				// hash the local history for this branch by xoring with the PC
				j = lhist[lhx] ^ pc;
			} else {
				// hash the global history of this branch by folding the i'th interval of
				// ghist and XORing with the right-shifted PC
				if (i == rpos_idx)
				j = (pc >> 1) ^ fold_backghist (0, assoc, lgsizes[i]); 
				else
				j = (pc >> 1) ^ fold_ghist (intervals[i*2], intervals[i*2+1], lgsizes[i]); 
			}

			// make sure j indexes in bounds of the table
			if (fix)
			j %= 1<<lgsizes[ii % ntables];
			else
			j %= 1<<lgsizes[i];

			// record this index for updating
			indices[i] = j;

			// for each predicted bit...
			for (int k=0; k<n; k++) {

				// get the weight from the current (rotated) table
				sign_magnitude w = weights[ii % ntables][k][j];

				// apply the transfer function to the magnitude
				int val = xlat[w.magnitude()];

				// negate if necessary
				if (w.sign()) val = -val;

				// scale by the i'th coefficient
				val *= coeffs[i];

				// add to the running total
				youts[k] += val;
			}

			// next table
			ii++;
		}

		// done; return the predictions
		return youts;
	}

	// adaptive threshold training
	void theta_setting (bool correct, int a, int k) {
		if (!correct) {
			tc[k]++;
			if (tc[k] >= speed) {
				theta[k]++;
				tc[k] = 0;
			}
		}
		if (correct && a < theta[k]) {
			tc[k]--;
			if (tc[k] <= -speed) {
				theta[k]--;
				if (theta[k] < 0) theta[k] = 0;
				tc[k] = 0;
			}
		}
	}

	// train the predictors with the real bits from the address. 
	// some bits are supressed because they are judged to be useless
	// (e.g. if all target addresses have 0 for that bit so no need to predict)
	void update (bool *bits, bool *supress, int rotate, int cut) {

		// for each address bit to be predicted...
		for (int k=0; k<n; k++) {
			// if we don't need this bit then don't train, 
			// saving table resources for other branches
			if (supress[k]) continue;

			// get the magnitude of this yout and whether or not it was correct
			int a = abs (youts[k]) * fudge;
			bool correct = bits[k] == (youts[k] >= 0);

			// do adaptive threshold training
			theta_setting (correct, a, k);

			// if the bit was predicted incorrectly or the output 
			// magnitude failed to meet the threshold, train the weights

			if (!correct || a < theta[k]) {
				// these loops correspond to the ones for computing the prediction
				unsigned int ii = hash32bit (opc) * rotate;

				// for each table for this address bit...
				//for (int i=0; i<ntables; i++) {
				for (int i=0; i<cut; i++) {
					// what was the index we used to select this weight?
					int j = indices[i];

					// get a pointer to that weight
					sign_magnitude *w = &weights[ii % ntables][k][j];

					// increment (if the bit is one) or decrement (if the bit is 0)

					w->incdec (bits[k], maxweight);
					ii++;
				}

			}
		}

		insert_recency (opc);
		// update local history
		lhist[lhx] <<= 1;
		lhist[lhx] |= bits[lbit];
		lhist[lhx] &= (1<<lhlength)-1;
	}
};


    

    // we keep a "btb" that is a specialized branch target buffer 
	// just for indirect branch targets. see the btb_entry struct. 
	// the btb is tagged with partial tags to save space. the same 
	// set in the btb can have multiple partial tags with the same 
	// value representing different targets from the same branch. 
	// we "collect" all the targets that match a partial tag and, 
	// based on the perceptron predictions, choose the target that 
	// most closely matches. 

    int 
		shift1,
		shift2,
		shift3,
		lg2_btb_sets, 	// log2 of number of btb sets
		btb_sets, 	// number of btb sets
		btb_assoc,	// btb associativity
		btb_tag_bits,	// number of bits in a partial tag
		nregions;	// number of regions (see btb_entry description)

	// the indirect branch target buffer
	btb_entry 
		btb[BTB_SETS][BTB_ASSOC];

	// number of address bits to predict (same as "n" in perceptron_predictor)
	int nbits;

	// the perceptron predictor that does all the neural stuff
	perceptron__predictor *predictor;

	// mask giving which bits need to be predicted and which bits we can ignore

	unsigned int which;

	// youts from perceptron_predictor
	int prediction[MAX_N];
// a 32-bit hash function with a nice distribution. I got this from Thomas Wang's web page.

int bipolar;
int 
		pcibits,
		jbits,
		idbits, 	// number of bits of indirect target to record in ghist
		rtbits, 	// number of bits of return pc to record in ghist
		lg2_regions, 	// log2 of the number of regions for representing targets in the btb
		regions,	// number of regions
		maxtarget,	// maximum number of targets for a given prediction
		offset_bits, 	// number of offset bits for representing targets in btb
		region_bits;	// number of region bits for representing targets in btb

	// the set of regions the btb currently remembers
	unsigned int 
		region_entries[REGIONS];

	unsigned int
		rand_counter;	// counter used as a cheap pseudorandom number generator

	double rrpv_coeff;

	int fix, rotate, hashfunc;

	double fudge;
	int threehack, twohack;
	int cut1, cut2, ntables;
	bool do_which;

    unsigned int pcg;

namespace
{


// initialize the btb
	void init_btb (void) {

		for (int i=0; i<btb_sets; i++) for (int j=0; j<btb_assoc; j++) {
			btb[i][j].partial_tag = 0;
			btb[i][j].region_index = 0;
			btb[i][j].offset = 0;

			// 3 is equivalent to LRU for RRIP; will be selected first so we don't need a valid bit
			btb[i][j].rrpv = 3; 
		}
	}

	// get all the btb entries whose partial tags match this PC
	// return them in an STL list of pointers to btb entries
	std::list<btb_entry *> collect (unsigned int pc0) {
		// return this list
		std::list<btb_entry *> L;

		// the partial tag is a hashed pc
		unsigned int pc = hash32bit (pc0);

		// figure out the set and tag
		unsigned int set = pc & (btb_sets-1);
		unsigned int tag = pc >> lg2_btb_sets;
		tag &= ((1<<btb_tag_bits)-1);

		// count the number of targets we collect; we have a 
		// maximum number of targets as a parameter in case 
		// people want to limit the number of targets we can 
		// match against the prediction
		int nt = 0;

		// go through this set looking for matching partial tags
		for (int i=0; i<btb_assoc; i++) {
			if (btb[set][i].partial_tag == tag) {

				// found one; put it on the list
				L.push_back (&btb[set][i]);
				nt++;

				// too many targets? stop with what we have.
				if (nt >= maxtarget) break;
			}
		}

		// return the list of collected btb entries
		return L;
	}

	// we don't actually predict bits of the PC; 
	// we predict bits of this hash of the PC

	unsigned int do_hash (unsigned int x) {
		unsigned int y = 0;
		if (shift1 == 0 && shift2 == 0 && shift3 == 0) {
			y = (x >> 4) ^ x;
		} else {
			unsigned int xl = x;
			unsigned int xm = x;
			unsigned int xr = x;
			if (shift1 < 0) xl <<= -shift1; else xl >>= shift1;
			if (shift2 < 0) xr <<= -shift2; else xr >>= shift2;
			if (shift3 < 0) xm <<= -shift3; else xm >>= shift3;
			y = xl ^ xr ^ xm;
		}
		return y;
	}

	// get the target from a btb entry by appending the region with the offset bits
	unsigned int get_target (btb_entry *e) {
		return (region_entries[e->region_index] << offset_bits) | e->offset;
	}

	// set the btb entry's region/offset to some target
	void set_target (btb_entry *e, unsigned int target) {

		// get the memory region where this target lives
		unsigned int region_number = target >> offset_bits;

		// look for this region in the table of regions
		unsigned int i;
		for (i=0; i<(unsigned)nregions; i++) {
			// found it? we're done
			if (region_entries[i] == region_number) break;

			// found an uninitalized region entry? place this new one there.
			if (region_entries[i] == REGION_INIT) break;
		}

		// random replacement if we don't find our region or uninitialized entry
		if (i == (unsigned)nregions) i = (rand_counter++) % nregions;

		// (re)place the entry
		region_entries[i] = region_number;

		// tell the btb entry where to find the upper part of its target address
		e->region_index = i;

		// tell the btb entry the lower bits of its target address
		e->offset = target & ((1<<offset_bits)-1);
	}

	unsigned int rotate_bits (unsigned int x, int n) {
		for (int i=0; i<n; i++) {
			bool bit = x & 1;
			x >>= 1;
			if (bit) x |= 0x80000000;
		}
		return x;
	}

	// figure out which bits need to be predicted and which bits can be ignored
	// right now, we do that by only predicting bits in target addresses where
	// sometimes that bit is 0 and sometime it's 1 among the possible targets 
	// for this branch
	unsigned int which_bits (unsigned int pc) {
		if (!do_which) return 0xffffffff;

		// get all the targets corresponding to this PC
		std::list<btb_entry *> L = collect (pc);

		// if there's only zero or one target, we don't need to predict any bits
		if (L.size() <= 1) return 0;

		// assume no bits need to be predicted
		unsigned int w = 0;

		// if there are only two targets, find random bits where they differ and then we're done
		if (twohack && (L.size() == 2)) {
			unsigned int t0 = do_hash (get_target (L.front()));
			unsigned int t1 = do_hash (get_target (L.back()));
			int k = (pc >> 3) % nbits;
			int tot = 0;
			for (int i=0; i<nbits; i++) {
				if (((1<<k)&t0) != ((1<<k)&t1)) {
					w |= 1 << k;
					tot++;
					if (tot >= twohack) break;
				}
				k++;
				k %= nbits;
			}
		} else if (threehack && (L.size() == 3)) {
			int z = 0;
			unsigned int t0 = 0, t1 = 0, t2 = 0;
			// ungodly hack
			for (std::list<btb_entry*>::iterator p=L.begin(); p!=L.end(); p++) {
				unsigned int h = do_hash (get_target (*p));
				if (z == 0) t0 = h; else if (z == 1) t1 = h; else t2 = h;
				z++;
			}
			int k = (pc >> 3) % nbits;
			int tot = 0;
			for (int i=0; i<nbits; i++) {
				unsigned int b0 = (1<<k) & t0;
				unsigned int b1 = (1<<k) & t1;
				unsigned int b2 = (1<<k) & t2;
				if (b0 != b1 || b1 != b2 || b0 != b2) {
					w |= 1 << k;
					tot++;
					if (tot >= threehack) break;
				}
				k++;
				k %= nbits;
			}
		} else

		// go through all the bits of the target addresses
		for (int i=0; i<MAX_N; i++) {
			// we want to see if bit i is a 0 in one target 
			// and a 1 in another. if so, we need to predict bit i
			bool seenone = false, seenzero = false;
			for (std::list<btb_entry*>::iterator p=L.begin(); p!=L.end(); p++) {
				if (do_hash (get_target ((*p))) & (1<<i)) seenone = true; else seenzero = true;
			}

			// both 0 and 1? put this bit in the mask
			if (seenone && seenzero) w |= (1<<i);
		}
		return w;
	}

	// insert a target into the btb
	void insert_btb (unsigned int pc0, unsigned int target) {

		// partial tags are hashed of the PC

		unsigned int pc = hash32bit (pc0);

		// figure out set and tag
		unsigned int set = pc & (btb_sets-1);
		unsigned int tag = pc >> lg2_btb_sets;
		tag &= ((1<<btb_tag_bits)-1);

		// search for this combination of pc and target

		int i;
		for (i=0; i<btb_assoc; i++) if (btb[set][i].partial_tag == tag && get_target (&btb[set][i]) == target) break;

		// if it is not there, insert it

		bool miss = false;
		if (i == btb_assoc) {
			miss = true;
			// look for an uninitialized btb entry, where the tag and target are 0
			for (i=0; i<btb_assoc; i++) if (btb[set][i].partial_tag == 0 && get_target (&btb[set][i]) == 0) break;

			// no? then invoke the replacement policy (RRIP)
			if (i == btb_assoc) {
				// keep looking for some entry that has an RRPV of 3. 
				// if there is none, increment all RRPVs and start over

			startover:
				// collect all the entries with RRPV of 3, in random order
				unsigned int j = rand_counter;
				rand_counter += 17;
				for (i=0; i<btb_assoc; i++,j++) if (btb[set][(j%btb_assoc)].rrpv == 3) {
					i = j % btb_assoc;
					break;
				}
				if (i == btb_assoc) {
					// didn't get any replacement candidate; increment all RRPVs and start over
					for (i=0; i<btb_assoc; i++) btb[set][i].rrpv++;
					goto startover;
				}
				// place new entry in RRPV position 2, near LRU
				btb[set][i].rrpv = 2;
			}
		}

		// fill entry with tag and target (i.e. region/offset pair)
		btb[set][i].partial_tag = tag;
		set_target (&btb[set][i], target);

		// promote this target if this was a hit

		if (!miss) btb[set][i].rrpv = 0;
	}

	
    int xfr (int x) {
		return x;
	}

	// two questions: 1. why doesn't it matter if i use 0,-1 or 1,0, and 2. why doesn't
	// it matter whether i zero out the ith bit in prediction[]? what is going on?
	// answer for 2: since all the target bits are the same, the youts add up to a constant offset

	// dot product of bit vector in x with int vector in y of size m
	int dot_product (unsigned int x, int *y, int m) {
		int sum = 0;
		int xi;
		for (int i=0; i<m; i++) {
			unsigned int bit = x & (1u<<i);
			switch (bipolar) {
			case 0: xi = bit ? 1 : -1; break;
			case 1:	xi = bit ? 0 : -1; break;
			case 2: xi = bit ? 1 : 0; break;
			default: assert (0);
			}
			sum += xi * xfr (y[i]);
		}
		return sum;
	}

    double mag (double X[], int n) {
        double sum = 0.0;
        for (int i=0; i<n; i++) sum += X[i] * X[i];
        return sqrt (sum);
    }


    double cossim (int X[], int Y[], int n) {
        double XX[n], YY[n];
        double max = 0.0;
        for (int i=0; i<n; i++) if (fabs(X[i]) > max) max = fabs (X[i]);
        for (int i=0; i<n; i++) XX[i] = X[i] / max;
        for (int i=0; i<n; i++) YY[i] = Y[i];
        // compute dot product divided by product of magnitudes
        double sum = 0.0;
        // this is cosine similarity
#if 0
        for (int i=0; i<n; i++) sum += XX[i] * YY[i];
        return sum / (mag (XX, n) * mag (YY, n));
#endif
        // this is Euclidean distance
        for (int i=0; i<n; i++) sum += XX[i] * YY[i];
        return sqrt (sum);
    }

    double compute_pearsons (int X[], int Y[], int n) {
            double  sumxy, sumx, sumy, sumx2, sumy2, N;
            int     i;

            N = (double) n;
            sumxy = 0.0;
            sumx = 0.0;
            sumy = 0.0;
            sumx2 = 0.0;
            sumy2 = 0.0;
            for (i=0; i<n; i++) {
                    sumx += X[i];
                    sumy += Y[i];
                    sumxy += X[i] * Y[i];
                    sumx2 += X[i] * X[i];
                    sumy2 += Y[i] * Y[i];
            }
            double num = sumxy - ((sumx * sumy) / N);
            double dterm1 = (sumx2 - ((sumx * sumx) / N));
            double dterm2 = (sumy2 - ((sumy * sumy) / N));
            return num / sqrt (dterm1 * dterm2);
    }

    // dot-product each vector of matrix with prediction, putting the result in the result vector
    void matrix_vector_product (unsigned int *matrix, int *prediction, int *mvp, int n, int nbits) {
            if (bipolar >= 3) {
                    // compute distance between prediction vector and row i of matrix
                    for (int i=0; i<n; i++) {
                            int vec[nbits];
                            for (int j=0; j<nbits; j++) {
                                    unsigned int bit = matrix[i] & (1u<<j);
                                    switch (bipolar){
                                    case 3: vec[j] = bit ? 1 : 0; break;
                                    case 4: vec[j] = bit ? 1 : -1; break;
                                    case 5: vec[j] = bit ? 0 : -1; break;
                                    default:assert (0);
                                    }
                            }
                            mvp[i] = (int) (cossim (vec, prediction, nbits) * 1000.0);
                    }
            } else for (int i=0; i<n; i++) {
                    mvp[i] = dot_product (matrix[i], prediction, nbits);
            }
    }


} // namespace

void O3_CPU::initialize_indirect_branch_predictor() {
    bool _do_which = 1;
	int _cut1 = 0;
	int _cut2 = 0;
	int _twohack = 5;
	int _threehack = 8;
	double _fudge = 1.000000;
	int _btb_tag_bits = 8;		// number of bits for partial btb tags
	int _lg2_btb_sets = 6;		// log2 number of btb sets
	int _btb_assoc = 64;		// btb associativity
	int _nbits = 12;		// number of address bits to predict
	int *_intervals = default_intervals; // the intervals
	int _ntables = 8;		// number of prediction tables
	int _speed = 1;			// speed for adaptive threshold training
	int _bpw = 4;			// bits per weight (somewhat deprecated; should be 4)
	int _lbit = 3;			// which bit of target PC to record in local history
	int _init_theta = 1;		// initial value of theta
	int *_xlat = default_xlat;	// transfer function
	int _pcibits = 3;		// number of indirect target bits to record in ghist
	int _jbits = 0;	
	int _idbits = 9;		// number of indirect target bits to record in ghist
	int _rtbits = 6;		// number of return pc bits to record in ghist
	int _lhlength = 10;		// local history length
	int _nh = 256;			// number of local histories to keep
	int _lg2_regions = 7;		// log2 number of regions
	int _offset_bits = 20;		// number of offset bits
	double *_coeffs = default_coeffs; // coefficients to multiply weights by, indexed by table numbe
	int *_lgsizes = default_lgsizes; // log2 sizes of tables
	int _maxtarget = 64;		// maximum number of targets for a given prediction
	int _fix = 1;
	int _rotate = 0;
	int _hashfunc = 42;
	int _shift1 = -2;
	int _shift2 = 1;
	int _shift3 = 6;
	double _rrpv_coeff = 0.000000;
	int _bipolar = 0;
	int _assoc = 2;
	int _rpos_idx = -1;

    do_which = _do_which;
	cut1 = _cut1;
	cut2 = _cut2;
	fudge = _fudge;
	twohack = _twohack;
	threehack = _threehack;
	bipolar = _bipolar;
	rrpv_coeff = _rrpv_coeff;
	shift1 = _shift1;
	shift2 = _shift2;
	shift3 = _shift3;
	hashfunc = _hashfunc;
	fix = _fix;
	rotate = _rotate;
	// initialize from parameters
	maxtarget = _maxtarget;
	lg2_regions = _lg2_regions;
	nregions = 1<<lg2_regions;
	offset_bits = _offset_bits;
	region_bits = 32 - offset_bits;
	for (int i=0; i<1<<lg2_regions; i++) region_entries[i] = REGION_INIT;
	pcibits = _pcibits;
	jbits = _jbits;
	idbits = _idbits;
	rtbits = _rtbits;
	btb_tag_bits = _btb_tag_bits;
	lg2_btb_sets = _lg2_btb_sets;
	btb_sets = 1 << _lg2_btb_sets;
	btb_assoc = _btb_assoc;

	nbits = _nbits;

	// start off random number generator counter
	rand_counter = 0xdeadb10c;

	// intialize btb
	init_btb ();
	// if (_rpos_idx >= 0) printf ("what what what! %d %d %d", bipolar, _assoc, _rpos_idx);

	// get a perceptron predictor for targets
	ntables = _ntables;

    predictor = new perceptron__predictor (fudge, nbits, _intervals, _ntables, _speed, _bpw, _lbit, _init_theta, _xlat, _lhlength, _nh, _coeffs, _lgsizes, _assoc, _rpos_idx);



}

uint64_t O3_CPU::predict_indirect_branch(uint64_t ip, uint8_t branch_type)
{
    rand_counter++;
	
	pcg = ip >> 1;

	// this will point to the btb entry of our prediction, if any
	btb_entry *e = NULL;

    

	// get all the targets matching this PC
	std::list<btb_entry *> L = collect (pcg);

	// predict the bits of the target address
	int *youts = predictor->predict (ip, fix, rotate, L.size() >= (unsigned) cut1 ? ntables : cut2);

	// special cases when there are one or two targets
	if (L.size() == 0) {
		// i don't know any target!
		return 0;
	} else if (L.size() == 1) {
		// there's only one target; that's my prediction and I'm done
		e = L.front();
		return get_target (e);
	} else {

		// which bits should we be trying to predict?
		which = which_bits (pcg);

		// build up a prediction for those bits

		for (int i=0; i<nbits; i++) {
			if (which & (1<<i)) {
				prediction[i] = youts[i];
			} else {
				prediction[i] = 0;
			}
		}

		// ### IDEA: put the youts through a transfer function
		// put all the possible (hashed) targets into rows of a matrix
		unsigned int matrix[64];
		btb_entry *entries[64];
		int n = 0;
		for (std::list<btb_entry *>::iterator p=L.begin(); p!=L.end(); p++) {
			btb_entry *f = *p;
			entries[n] = f;
			matrix[n++] = do_hash (get_target (f));
		}

		// compute the matrix vector product. the vector in the matrix 
		// corresponding to the best match will give the maximum value 
		// in the result vector
		int mvp[n];
		// matrix is a n x nbits matrix, vector is a nbits vector, result is a n vector
		matrix_vector_product (matrix, prediction, mvp, n, nbits);

		// find the maximum entry in the vector, corresponding to 
		// the best matching (hashed) target address
		int maxi = 0;
		// ### here is where we should account for the RRPV
		if (bipolar < 3) for (int i=0; i<n; i++) {
			btb_entry *ee = entries[i];
			mvp[i] += ee->rrpv * rrpv_coeff;
		}
		for (int i=0; i<n; i++) {
			if (mvp[i] > mvp[maxi]) maxi = i;
		}

		// get the btb entry for that best matching target
		e = entries[maxi];

		// the only thing that makes this outcome different is the rrpv_coeff
		if (0) {
			int bp = bipolar;
			int maxi[3];
			memset (maxi, 0, sizeof (maxi));
			for (bipolar=0; bipolar<=1; bipolar++) {
				int mvp[n];
				matrix_vector_product (matrix, prediction, mvp, n, nbits);
				for (int i=0; i<n; i++) {
					btb_entry *ee = entries[i];
					mvp[i] += ee->rrpv * rrpv_coeff;
				}
				for (int i=0; i<n; i++) {
					if (mvp[i] > mvp[maxi[bipolar]]) maxi[bipolar] = i;
				}
			}
			bipolar = bp;
			if (maxi[0] != maxi[1]) {
				printf ("nope!\n");
			}
		}
	}

	// if we have a predicted target, return it, otherwise just guess 0

	if (e)
		return get_target (e);
	else
		return 0;
		
   

   
}


void O3_CPU::last_indirect_branch_result(uint64_t ip, uint64_t predicted_target, uint64_t actual_target, uint8_t taken, uint8_t branch_type)
{
    // we remember the PC from before
		

	// record return PC bits in the global history
	if (branch_type == BRANCH_RETURN) {
		for (int i=0; i<rtbits; i++) {
			predictor->update_ghist (!(ip & (1<<i)));
		}
	}

	// return conditional branch outcome hashed with bit 7 
	// of the PC in the global history
	if (branch_type == BRANCH_CONDITIONAL) {
		predictor->update_ghist (taken ^ !(ip & 7));
		if (actual_target < ip) predictor->update_backghist (taken ^ !(ip & 7));
	}

	// record some bits from an indirect target in the global history
	if (branch_type == BRANCH_INDIRECT || branch_type == BRANCH_INDIRECT_CALL) {
		for (int i=0; i<idbits; i++) {
			predictor->update_ghist (!(actual_target & (1<<i)));
		}
	}
	if (branch_type == BRANCH_DIRECT_JUMP) {
		// plain old unconditional direct jump
		for (int i=0; i<jbits; i++) {
			unsigned int mask = 2 << i;
			predictor->update_ghist (!(ip & mask));
		}
	}

	// for an indirect branch, update the predictor
	if (branch_type == BRANCH_INDIRECT || branch_type == BRANCH_INDIRECT_CALL) {

		// stick this target in the btb, possibly inserting or promoting it

		insert_btb (pcg, actual_target);

		// update path history (seems redundant with the idbits stuff above 
		// but it's not; there we're recording target bits but here we're recording 
		// pc bits
		//predictor->update_indirect_path_history (pc);
		for (int i=0; i<pcibits; i++) {
			unsigned int mask = 2 << i;
			predictor->update_ghist (!(ip & mask));
		}

		// get all the possible targets of this branch. this list is not empty 
		// because now we know at least one target: the current one
		std::list<btb_entry *> L = collect (pcg);

		if (L.size() == 1) {
			// if there is just one target, no training needed
		} else {
			// try again with if there are 2 targets? better idee?
			// recompute 'which' mask in case we got a new target
			which = which_bits (pcg);

			// form the vector of bits used to update the predictor
			bool bits[nbits];
			bool supress[nbits];
			unsigned int y = do_hash (actual_target);
			for (int i=0; i<nbits; i++) {
				bits[i] = !!(y & (1<<i));
				// if all the i'th bits are the same, supress training for this bit
				supress[i] = !(which & (1<<i));
			}

			// update the perceptron predictor with this bit vector
			predictor->update (bits, supress, rotate, L.size() >= (unsigned)cut1 ? ntables : cut2);
		}
	}
	predictor->flush_ghist ();
  
}