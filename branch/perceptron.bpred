/*
 * Copyright (c) 2001 University of Texas at Austin
 *
 * Daniel A. Jimenez
 * Calvin Lin
 *
 * Permission is hereby granted, free of charge, to any person 
 * obtaining a copy of this software (the "Software"), to deal in
 * the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE UNIVERSITY OF TEXAS AT
 * AUSTIN BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file implements the simulated perceptron branch predictor from:
 *
 * Jimenez, D. A. & Lin, C., Dynamic branch prediction with perceptrons,
 * Proceedings of the Seventh International Symposium on High Performance
 * Computer Architecture (HPCA), Monterrey, NL, Mexico 2001
 *
 * The #define's here specify a perceptron predictor with a history
 * length of 24, 163 perceptrons, and  8-bit weights.  This represents
 * a hardware budget of (24+1)*8*163 = 32600 bits, or about 4K bytes,
 * which is comparable to the hardware budget of the Alpha 21264 hybrid
 * branch predictor.
 *
 * There are three important functions defined in this file:
 * 
 * 1. void initialize_perceptron_predictor (void);
 * Initialize the perceptron predictor
 *
 * 2. perceptron_state *perceptron_dir_lookup (unsigned int);
 * Get a branch prediction, given a branch address.  This function returns a
 * pointer to a 'perceptron_state' struct, which contains the prediction, the
 * perceptron output, and other information necessary for using and updating
 * the predictor.  The first member of a 'perceptron_state' struct is a char
 * that is assigned 3 if the branch is predicted taken, 0 otherwise; this way,
 * a pointer to 'perceptron_state' can be cast to (char *) and passed around
 * SimpleScalar as though it were a pointer to a pattern history table entry.
 *
 * 3. void perceptron_update (perceptron_state *, int);
 * Update the branch predictor using the 'perceptron_state' pointer 
 * returned by perceptron_dir_lookup() and an int that is 1 if the branch
 * was taken, 0 otherwise.
 */

#include "ooo_cpu.h"

/* history length for the global history shift register */

#define PERCEPTRON_HISTORY	24

/* number of perceptrons */

#define NUM_PERCEPTRONS		163

/* number of bits per weight */

#define PERCEPTRON_BITS		8

/* maximum and minimum weight values */

#define MAX_WEIGHT		((1<<(PERCEPTRON_BITS-1))-1)
#define MIN_WEIGHT		(-(MAX_WEIGHT+1))

/* threshold for training */

#define THETA			((int) (1.93 * PERCEPTRON_HISTORY + 14))

/* size of buffer for keeping 'perceptron_state' for update */

#define NUM_UPDATE_ENTRIES	100

/* perceptron data structure */

typedef struct {
	int	
		/* just a vector of integers */

		weights[PERCEPTRON_HISTORY+1];
} perceptron;

/* 'perceptron_state' - stores the branch prediction and keeps information
 * such as output and history needed for updating the perceptron predictor
 */
typedef struct {
	char	
		/* this char emulates a pattern history	table entry
		 * with a value of 0 for "predict not taken" or 3 for 
		 * "predict taken," so a perceptron_state pointer can 
		 * be passed around SimpleScalar's branch prediction 
		 * infrastructure without changing too much stuff.
		 */
		dummy_counter;

	int
		/* prediction: 1 for taken, 0 for not taken */

		prediction,

		/* perceptron output */

		output;

	unsigned long long int 
		/* value of the history register yielding this prediction */

		history;

	perceptron
		/* pointer to the perceptron yielding this prediction */

		*perc;
} perceptron_state;

perceptron 
	/* table of perceptrons */

	perceptrons[NUM_CPUS][NUM_PERCEPTRONS];

perceptron_state 
	/* state for updating perceptron predictor */

	perceptron_state_buf[NUM_CPUS][NUM_UPDATE_ENTRIES];

int 
	/* index of the next "free" perceptron_state */

	perceptron_state_buf_ctr[NUM_CPUS];

unsigned long long int

	/* speculative global history - updated by predictor */

	spec_global_history[NUM_CPUS],

	/* real global history - updated when the predictor is updated */

	global_history[NUM_CPUS];

perceptron_state *u[NUM_CPUS];

/* initialize a single perceptron */
void initialize_perceptron (perceptron *p) {
    int	i;

    for (i=0; i<=PERCEPTRON_HISTORY; i++) p->weights[i] = 0;
}

void O3_CPU::initialize_branch_predictor()
{
    spec_global_history[cpu] = 0;
    global_history[cpu] = 0;
    perceptron_state_buf_ctr[cpu] = 0;
    for (int i=0; i<NUM_PERCEPTRONS; i++)
        initialize_perceptron (&perceptrons[cpu][i]);
}

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
    uint64_t address = ip;

    int	
        index,
        i,
        output,
        *w;
    unsigned long long int 
        mask;
    perceptron 
        *p;

    /* get a pointer to the next "free" perceptron_state,
     * bumping up the pointer (and possibly letting it wrap around) 
     */

    u[cpu] = &perceptron_state_buf[cpu][perceptron_state_buf_ctr[cpu]++];
    if (perceptron_state_buf_ctr[cpu] >= NUM_UPDATE_ENTRIES)
        perceptron_state_buf_ctr[cpu] = 0;

    /* hash the address to get an index into the table of perceptrons */

    index = address % NUM_PERCEPTRONS;

    /* get pointers to that perceptron and its weights */

    p = &perceptrons[cpu][index];
    w = &p->weights[0];

    /* initialize the output to the bias weight, and bump the pointer
     * to the weights
     */

    output = *w++;

    /* find the (rest of the) dot product of the history register
     * and the perceptron weights.  note that, instead of actually
     * doing the expensive multiplies, we simply add a weight when the
     * corresponding branch in the history register is taken, or
     * subtract a weight when the branch is not taken.  this also lets
     * us use binary instead of bipolar logic to represent the history
     * register
     */
    for (mask=1,i=0; i<PERCEPTRON_HISTORY; i++,mask<<=1,w++) {
        if (spec_global_history[cpu] & mask)
            output += *w;
        else
            output += -*w;
    }

    /* record the various values needed to update the predictor */

    u[cpu]->output = output;
    u[cpu]->perc = p;
    u[cpu]->history = spec_global_history[cpu];
    u[cpu]->prediction = output >= 0;
    u[cpu]->dummy_counter = u[cpu]->prediction ? 3 : 0;

    /* update the speculative global history register */

    spec_global_history[cpu] <<= 1;
    spec_global_history[cpu] |= u[cpu]->prediction;
    return u[cpu]->prediction;
}

void O3_CPU::last_branch_result(uint64_t ip, uint8_t taken)
{
    int	
        i,
        y, 
        *w;

    unsigned long long int
        mask, 
        history;

    /* update the real global history shift register */

    global_history[cpu] <<= 1;
    global_history[cpu] |= taken;

    /* if this branch was mispredicted, restore the speculative
     * history to the last known real history
     */

    if (u[cpu]->prediction != taken) spec_global_history[cpu] = global_history[cpu];

    /* if the output of the perceptron predictor is outside of
     * the range [-THETA,THETA] *and* the prediction was correct,
     * then we don't need to adjust the weights
     */

    if (u[cpu]->output > THETA)
        y = 1;
    else if (u[cpu]->output < -THETA)
        y = 0;
    else
        y = 2;
    if (y == 1 && taken) return;
    if (y == 0 && !taken) return;

    /* w is a pointer to the first weight (the bias weight) */

    w = &u[cpu]->perc->weights[0];

    /* if the branch was taken, increment the bias weight,
     * else decrement it, with saturating arithmetic
     */

    if (taken)
        (*w)++;
    else
        (*w)--;
    if (*w > MAX_WEIGHT) *w = MAX_WEIGHT;
    if (*w < MIN_WEIGHT) *w = MIN_WEIGHT;

    /* now w points to the next weight */

    w++;

    /* get the history that led to this prediction */

    history = u[cpu]->history;

    /* for each weight and corresponding bit in the history register... */

    for (mask=1,i=0; i<PERCEPTRON_HISTORY; i++,mask<<=1,w++) {

        /* if the i'th bit in the history positively correlates
         * with this branch outcome, increment the corresponding 
         * weight, else decrement it, with saturating arithmetic
         */

        if (!!(history & mask) == taken) { // a common trick to conver to boolean => !!x is 1 iff x is not zero, in this case history is positively correlated with branch outcome
            (*w)++;
            if (*w > MAX_WEIGHT) *w = MAX_WEIGHT;
        } else {
            (*w)--;
            if (*w < MIN_WEIGHT) *w = MIN_WEIGHT;
        }
    }
}
