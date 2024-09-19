#include "msl/fwcounter.h"
#include "ooo_cpu.h"
#include <iostream>

#include <inttypes.h>
#include <math.h>
#define NHIST 15 /*number of tagged tables*/
#define SHARINGTABLES /* share some interleaved tables*/
#define INITHISTLENGTH /* use tuned history lengths*/
/*list of the history length*/
int m[NHIST+1]={0, 0, 10, 16, 27, 44, 60, 96, 109, 219, 449, 487, 714, 1313, 2146, 3881};

     
#define IUM
//if a matching  entry corresponds to an inflight branch  use the real target of this inflight branch 

#define LOGG 12

#ifndef SHARINGTABLES
#define TBITS 7
#endif
#ifndef INITHISTLENGTH
#define MINHIST 8		// shortest history length
#define MAXHIST 2000
#endif

int TICK;			//control counter for the resetting of useful bits
#define LOGTICK  6		//for management of the reset of useful counters
#define HISTBUFFERLENGTH 4096
//size of the history circular  buffer

// utility class for index computation
class folded_history
{
// this is the cyclic shift register for folding 
// a long global history into a smaller number of bits; see P. Michaud's PPM-like predictor at CBP-1
public:
  unsigned comp;
  int CLENGTH;
  int OLENGTH;
  int OUTPOINT;

    folded_history ()
  {
  }

  void init (int original_length, int compressed_length)
  {
    comp = 0;
    OLENGTH = original_length;
    CLENGTH = compressed_length;
    OUTPOINT = OLENGTH % CLENGTH;
  }

  void update (uint8_t * h, int PT)
  {
    comp = (comp << 1) | h[PT & (HISTBUFFERLENGTH - 1)];
    comp ^= h[(PT + OLENGTH) & (HISTBUFFERLENGTH - 1)] << OUTPOINT;
    comp ^= (comp >> CLENGTH);
    comp &= (1 << CLENGTH) - 1;
  }
};


//class for storing speculative predictions: i.e. provided by a table entry that has already provided a still speculative prediction
class specentry
{
public:
  int32_t tag;
  uint32_t pred;
  specentry (){//nothing
}
};


class gentry			// ITTAGE global table entry
{
public:
  int8_t ctr;			// 2 bits 
  uint16_t tag;			//width dependent on the table
  uint32_t target;		//25 bits (18 offset + 7-bit region pointer)
  int8_t u;			//1 bit
    gentry ()
  {
    ctr = 0;
    tag = 0;
    u = 0;
    target = 0;

  }
};
class regionentry		// ITTAGE target region  table entry
{
public:
     uint16_t region; // 14 bits

  int8_t u; //1 bit
    regionentry ()
  {
    region = 0;
    u = 0;


  }
};
int8_t USE_ALT_ON_NA;		// "Use alternate prediction on weak predictions":  a 4-bit counter  to determine whether the newly allocated entries should be considered as  valid or not for delivering  the prediction


//for managing global history  and path

uint8_t ghist[HISTBUFFERLENGTH];
//for management at fetch time
int Fetch_ptghist;
folded_history Fetch_ch_i[NHIST + 1];	//utility for computing TAGE indices
folded_history Fetch_ch_t[2][NHIST + 1];	//utility for computing TAGE tags

// for management at retire time
int Retire_ptghist;
folded_history Retire_ch_i[NHIST + 1];	//utility for computing TAGE indices
folded_history Retire_ch_t[2][NHIST + 1];	//utility for computing TAGE tags



//predictor tables
gentry *gtable[NHIST + 1];	// ITTAGE tables (T0 has no tags other tables are tagged) 
regionentry *rtable; // Target region tables 


int TB[NHIST + 1];		// tag width for the different tagged tables
int logg[NHIST + 1];		// log of number entries of the different tagged tables
int GI[NHIST + 1] ;		// indexes to the different tables are computed only once  
int GTAG[NHIST + 1];		// tags for the different tables are computed only once  


uint32_t pred_taken;		// prediction
uint32_t alttaken;		// alternate  TAGEprediction
uint32_t tage_pred;		// TAGE prediction
int HitBank;			// longest matching bank
int AltBank;			// alternate matching bank
uint32_t LongestMatchPred;

int Seed;			// for the pseudo-random number generator

//for the IUM
int PtIumRetire;
int PtIumFetch;
specentry *IUMPred;
#define LOGSPEC 6



namespace
{
// the index functions for the tagged tables uses path history as in the OGEHL predictor

// gindex computes a full hash of pc, ghist 
int gindex (unsigned int pc, int bank, folded_history * ch_i){
    int index;
    index =
      pc ^ (pc >> (abs (logg[bank] - bank) + 1)) ^
         ch_i[bank].comp;
    return (index & ((1 << (logg[bank])) - 1));
}

  //  tag computation
uint16_t gtag (unsigned int pc, int bank, folded_history * ch0, folded_history * ch1){
    int tag = pc ^ ch0[bank].comp ^ (ch1[bank].comp << 1);
    return (tag & ((1 << TB[bank]) - 1));
}


//just a simple pseudo random number generator: use available information
// to allocate entries  in the loop predictor
int MYRANDOM () {
    Seed++;
    Seed ^= Fetch_ptghist;  
    Seed = (Seed >> 11) + (Seed << 21);
    Seed += Retire_ptghist;
    return (Seed);
};


void Tagepred () {

    alttaken = 0;
    HitBank = -1; 
    AltBank = -1;
//Look for the bank with longest matching history
    for (int i = NHIST; i >= 0; i--) {
        if (gtable[i][GI[i]].tag == GTAG[i]) {
            HitBank = i;
            break;
        }
    }
//Look for the alternate bank
    for (int i = HitBank - 1; i >= 0; i--) {
        if (gtable[i][GI[i]].tag == GTAG[i]){
            AltBank = i;
            break;
        }
    }
//computes the prediction and the alternate prediction
    if (AltBank >= 0)
        alttaken = gtable[AltBank][GI[AltBank]].target;
    
    tage_pred = gtable[HitBank][GI[HitBank]].target;
//proceed with the indirection through the target region table
    alttaken =
      (alttaken & ((1 << 18) - 1)) +
      (rtable[(alttaken >> 18) & 127].region << 18);

    tage_pred =
      (tage_pred & ((1 << 18) - 1)) +
      (rtable[(tage_pred >> 18) & 127].region << 18);
    
    LongestMatchPred = tage_pred;



//if the entry is recognized as a newly allocated entry and 
//USE_ALT_ON_NA is positive  use the alternate prediction
    if (AltBank >= 0)
        if ((USE_ALT_ON_NA >= 0) & (gtable[HitBank][GI[HitBank]].ctr == 0))
	        tage_pred = alttaken;

}

uint32_t PredSpecIUM (uint32_t pred) {

#ifdef IUM
    int IumTag = (HitBank) + (GI[HitBank] << 4);

    int Min = (PtIumRetire > PtIumFetch + 8) ? PtIumFetch + 8 : PtIumRetire;


    for (int i = PtIumFetch; i < Min; i++){
	    if (IUMPred[i & ((1 << LOGSPEC) - 1)].tag == IumTag)
	        return IUMPred[i & ((1 << LOGSPEC) - 1)].pred;
	}

#endif
    return pred;

}

void UpdateSpecIUM (uint32_t Target){
#ifdef IUM
    int IumTag = (HitBank) + (GI[HitBank] << 4);

    PtIumFetch--;
    IUMPred[PtIumFetch & ((1 << LOGSPEC) - 1)].tag = IumTag;
    IUMPred[PtIumFetch & ((1 << LOGSPEC) - 1)].pred = Target;
#endif
}

void HistoryUpdate (uint32_t pc, uint16_t brtype, bool taken,
			uint32_t target,  int &Y, folded_history * H,
			folded_history * G, folded_history * J) {

    int maxt = (brtype == BRANCH_INDIRECT || brtype == BRANCH_INDIRECT_CALL) ? 10 : 0;
    if (!(brtype == BRANCH_INDIRECT || brtype == BRANCH_INDIRECT_CALL)) 
        if ( brtype == BRANCH_DIRECT_CALL)
	        maxt = 5;

    int PATH = ((target ^ (target >> 3) ^ pc));
    if (brtype == BRANCH_CONDITIONAL)
        PATH = taken;
      
    for (int t = 0; t < maxt; t++){
        bool P = (PATH  & 1);
        PATH >>= 1;
        
    //update  history
        Y--;
        ghist[Y & (HISTBUFFERLENGTH - 1)] = P;
            
    //prepare next index and tag computations for user branchs 
        for (int i = 1; i <= NHIST; i++){
            H[i].update (ghist, Y);
            G[i].update (ghist, Y);
            J[i].update (ghist, Y);
        }
	}

//END UPDATE FETCH HISTORIES
}




} // namespace

void O3_CPU::initialize_indirect_branch_predictor() {
   USE_ALT_ON_NA = 0;
    Seed = 0;
    PtIumRetire = 0;
    PtIumFetch = 0;

    for (int i = 0; i < HISTBUFFERLENGTH; i++)
        ghist[0] = 0;
    Fetch_ptghist = 0;
    Retire_ptghist = 0;
#ifndef INITHISTLENGTH
    m[0] = 0;
    m[1] = 0;

    m[2] = MINHIST;
    m[NHIST] = MAXHIST;
    for (int i = 3; i <= NHIST; i++) {
	    m[i] = (int) (((double) MINHIST *
		       pow ((double) (MAXHIST) /
			    (double) MINHIST,
			    (double) (i -
				      2) / (double) ((NHIST - 2)))) + 0.5);
    }
    for (int i = 2; i <= NHIST; i++)
        if (m[i] <= m[i - 1] + 2)
	        m[i] = m[i - 1] + 2;
#endif
#ifndef SHARINGTABLES
    for (int i = 2; i <= NHIST; i++) {
        TB[i] = (TBITS + i);
        if (TB[i] >= 16)
            TB[i] = 16;
    }
    TB[0] = 0;			// table T0 is tagless
    TB[1] = 6;
    logg[0] = LOGG;
    logg[1]= LOGG-1;
    
    for (int i = 2; i <= 4; i++)
        logg[i] = LOGG -2;
    for (int i = 5; i <= 13; i++)
        logg[i] = LOGG - 3;
    for (int i = 14; i <= NHIST; i++)
        logg[i] = LOGG - 4;
#endif
#ifdef SHARINGTABLES
#define STEP1 3
#define STEP2 11
    logg[0] = LOGG;
    logg[1] = LOGG;
    logg[STEP1] = LOGG;
    logg[STEP2] = LOGG - 1;

    for (int i = 2; i <= STEP1-1; i++)
        logg[i] = logg[1] - 3;	/* grouped together with T1 4Kentries */

    for (int i = STEP1+1; i <= STEP2-1; i++)
        logg[i] = logg[STEP1] - 3;	/*grouped together with T4 4K entries */

    for (int i = STEP2+1; i <= 15; i++)
        logg[i] = logg[STEP2] - 3;	/*grouped together with T12 2K entries */


    gtable[0] = new gentry[1 << logg[0]];
    gtable[1] = new gentry[1 << logg[1]];
    gtable[STEP1] = new gentry[1 << logg[STEP1]];
    gtable[STEP2] = new gentry[1 << logg[STEP2]];
    TB[0] = 0; //T0 is tagless
    for (int i = 1; i <= STEP1-1; i++) {
        gtable[i] = gtable[1];
        TB[i] = 9;
    }

    for (int i = STEP1; i <= STEP2-1; i++) {
        gtable[i] = gtable[STEP1];
        TB[i] = 13;
    }

    for (int i = STEP2; i <= 15; i++) {
        gtable[i] = gtable[STEP2];
        TB[i] = 15;
    }

#endif

//initialisation of index and tag computation functions
    for (int i = 1; i <= NHIST; i++) {
        Fetch_ch_i[i].init (m[i], (logg[i]));
        Fetch_ch_t[0][i].init (Fetch_ch_i[i].OLENGTH, TB[i]);
        Fetch_ch_t[1][i].init (Fetch_ch_i[i].OLENGTH, TB[i] - 1);
    }

    for (int i = 1; i <= NHIST; i++) {
        Retire_ch_i[i].init (m[i], (logg[i]));
        Retire_ch_t[0][i].init (Retire_ch_i[i].OLENGTH, TB[i]);
        Retire_ch_t[1][i].init (Retire_ch_i[i].OLENGTH, TB[i] - 1);
    }
    rtable = new regionentry[128];
    IUMPred = new specentry[1 << LOGSPEC];
#ifndef SHARINGTABLES
    for (int i = 0; i <= NHIST; i++) {
	    gtable[i] = new gentry[1 << (logg[i])];
    }
#endif

#define PRINTCHAR
#ifdef PRINTCHAR
//for printing predictor characteristics
    int NBENTRY = 0;
    int STORAGESIZE = 0;
    fprintf(stdout,"history lengths: " );
    
    for (int i = 0; i <= NHIST; i++){
	    fprintf (stdout, "%d ", m[i]);  
    }
    fprintf (stdout, "\n");
#ifndef SHARINGTABLES
    for (int i = 0; i <= NHIST; i++){
        STORAGESIZE += (1 << logg[i]) * (25 + 2 + (i != 0) + TB[i]);
        NBENTRY += (1 << logg[i]);
    }
#else
//The ITTAGE physical  tables: four global tables shared (interleaved) among several logic ITTAGE tables
//Entry width: target 25 bits + 2 bits  confidence counter + TB[i] tag bits + 1 useful bit (except T0)
    STORAGESIZE += (1 << (logg[0])) * (25 + 2 + TB[0]);
    STORAGESIZE += (1 << (logg[1])) * (25 + 2 + 1 + TB[1]);
    STORAGESIZE += (1 << (logg[STEP1])) * (25 + 2 + 1 + TB[STEP1]);
    STORAGESIZE += (1 << (logg[STEP2])) * (25 + 2 + 1 + TB[STEP2]);
           

    NBENTRY +=
      (1 << (logg[0])) + (1 << (logg[1])) + (1 << (logg[STEP1])) +
      (1 << (logg[STEP2]));
#endif
 fprintf(stdout, "ITTAGE tables: %d bytes;",  STORAGESIZE/8);
 fprintf(stdout, "Region table: %d bytes;",  15*128/8);
    STORAGESIZE += 15 * 128;	//Region Table (14 bits Region + 1 bit for replacement)
#ifdef IUM
fprintf(stdout, "IUM: %d bytes;",  (1 << LOGSPEC) * 48/8);
    STORAGESIZE += (1 << LOGSPEC) * 48;	//entries in the speculative update table are 32 bits + 16 tag bits
#endif

fprintf (stdout,
	     "Total Storage= %d bytes;\n",
	      STORAGESIZE / 8);
#endif

}

uint64_t O3_CPU::predict_indirect_branch(uint64_t ip, uint8_t branch_type)
{
   // computes the table addresses and the partial tags
   if(branch_type == BRANCH_INDIRECT || branch_type== BRANCH_INDIRECT_CALL){
		for (int i = 0; i <= NHIST; i++){
			GI[i] = gindex (ip, i,  Fetch_ch_i);
			GTAG[i] = gtag (ip, i, Fetch_ch_t[0], Fetch_ch_t[1]);
		}
	#ifdef SHARINGTABLES
		for (int i = 2; i <= STEP1-1; i++)
			GI[i] = ((GI[1] & 7) ^ (i - 1)) + (GI[i] << 3);

		for (int i = STEP1+1; i <= STEP2-1; i++)
			GI[i] = ((GI[STEP1] & 7) ^ (i - STEP1)) + (GI[i] << 3);


		for (int i = STEP2+1; i <= 15; i++)
			GI[i] = ((GI[STEP2] & 7) ^ (i - STEP2)) + (GI[i] << 3);
	#endif	

		GTAG[0] = 0;
		GI[0] = ip & ((1 << logg[0]) - 1);


		Tagepred ();
		pred_taken = PredSpecIUM (tage_pred);

   }

	
   return pred_taken;
}


void O3_CPU::last_indirect_branch_result(uint64_t ip, uint64_t predicted_target, uint64_t actual_target, uint8_t taken, uint8_t branch_type)
{
    if(branch_type == BRANCH_INDIRECT || branch_type== BRANCH_INDIRECT_CALL){
        UpdateSpecIUM (actual_target);
    }

    HistoryUpdate (ip, branch_type, taken, actual_target, Fetch_ptghist,
		     Fetch_ch_i, Fetch_ch_t[0], Fetch_ch_t[1]);
    
    int NRAND = MYRANDOM ();

    if (branch_type == BRANCH_INDIRECT || branch_type== BRANCH_INDIRECT_CALL){
	    PtIumRetire--;

//Recompute  the prediction by the ITTAGE predictor: on an effective implementation one would try to avoid this recomputation by propagating the prediction with the branch instruction

	for (int i = 0; i <= NHIST; i++){
	    GI[i] = gindex (ip, i, Retire_ch_i);
	    GTAG[i] = gtag (ip, i, Retire_ch_t[0], Retire_ch_t[1]);
	}

#ifdef SHARINGTABLES
	for (int i = 2; i <= STEP1-1; i++)
	    GI[i] = ((GI[1] & 7) ^ (i - 1)) + (GI[i] << 3);

	for (int i = STEP1+1; i <= STEP2-1; i++)
	    GI[i] = ((GI[STEP1] & 7) ^ (i - STEP1)) + (GI[i] << 3);


	for (int i = STEP2+1; i <= 15; i++)
	    GI[i] = ((GI[STEP2] & 7) ^ (i - STEP2)) + (GI[i] << 3);
#endif

	GTAG[0] = 0;
	GI[0] = ip & ((1 << logg[0]) - 1);

	Tagepred ();


	bool ALLOC = (LongestMatchPred != actual_target);
    //allocation if the Longest Matching entry does not provide the correct entry

	if ((HitBank > 0) & (AltBank >= 0)) {
// Manage the selection between longest matching and alternate matching
// for "pseudo"-newly allocated longest matching entry

	    bool PseudoNewAlloc = (gtable[HitBank][GI[HitBank]].ctr == 0);
	    if (PseudoNewAlloc) {
		  if (alttaken)
		    if (LongestMatchPred != alttaken)
		      {
			if ((alttaken == actual_target)
			    || (LongestMatchPred == actual_target))
			  {
			    if (alttaken == actual_target)
			      {
				if (USE_ALT_ON_NA < 7)
				  USE_ALT_ON_NA++;
			      }
			    else
			      {
				if (USE_ALT_ON_NA >= -8)
				  USE_ALT_ON_NA--;
			      }
			  }

		      }
		}
	}


	if (ALLOC) {

//Need to compute the target field (Offset + Region pointer)
	    int Region = (actual_target >> 18);
	    int PtRegion = -1;
//Associative search on the region table
	    for (int i = 0; i < 128; i++)
            if (rtable[i].region == Region) {
                PtRegion = i;
                break;
            }

	    if (PtRegion == -1) {
//miss in the target region table, allocate a free entry
		    for (int i = 0; i < 128; i++)
		    if (rtable[i].u == 0) {
                PtRegion = i;
                rtable[i].region = Region;
                rtable[i].u = 1;
                break;
		    }

// a very simple  replacement policy (don't care for the competition, but some replacement is needed in a real processor)
		    if (PtRegion == -1) {
		        for (int i = 0; i < 128; i++)
			    rtable[i].u = 0;
		        PtRegion = 0;
		        rtable[0].region = Region;
		        rtable[0].u = 1;

		    }


		}
	    
        int IndTarget = (actual_target & ((1 << 18) - 1)) + (PtRegion << 18);



// we allocate an entry with a longer history
//to  avoid ping-pong, we do not choose systematically the next entry, but among the 2 next entries
	    int Y = NRAND;

	    int X = HitBank + 1;
	    if ((Y & 31) == 0) {
		    X++;
		}

	    int T = 2;
//allocating 3  entries on a misprediction just work a little bit better than a single allocation
	    for (int i = X; i <= NHIST; i += 1) {

		    if (gtable[i][GI[i]].u == 0) {
                gtable[i][GI[i]].tag = GTAG[i];
                gtable[i][GI[i]].target = IndTarget;
                gtable[i][GI[i]].ctr = 0;
                gtable[i][GI[i]].u = 0;
                if (TICK > 0)
                    TICK--;

		        if (T == 0)
			        break;
                T--;
                i += 1;

		    }
		    else
		        TICK++;
		}

	}

	if ((TICK >= (1 << LOGTICK))) {
	    TICK = 0;
// reset the useful  bit
	    for (int i = 0; i <= NHIST; i++)
		    for (int j = 0; j < (1 << logg[i]); j++)
		        gtable[i][j].u = 0;

	}

	if (LongestMatchPred == actual_target) {
	    if (gtable[HitBank][GI[HitBank]].ctr < 3)
		    gtable[HitBank][GI[HitBank]].ctr++;
	}
	else {
	    if (gtable[HitBank][GI[HitBank]].ctr > 0)
		    gtable[HitBank][GI[HitBank]].ctr--;
	    else {// replace the target field by the new target
// Need to compute the target field :-)
		  int Region = (actual_target >> 18);
		  int PtRegion = -1;

		    for (int i = 0; i < 128; i++)
		        if (rtable[i].region == Region) {
                    PtRegion = i;
                    break;
		        }

		    if (PtRegion == -1) {
                for (int i = 0; i < 128; i++)
                if (rtable[i].u == 0) {
                    PtRegion = i;
                    rtable[i].region = Region;
                    rtable[i].u = 1;
                    break;
                }

    //a very simple  replacement policy (don't care for the competition, but some replacement is needed in a real processor)
                if (PtRegion == -1) {
                    for (int i = 0; i < 128; i++)
                        rtable[i].u = 0;
                    PtRegion = 0;
                    rtable[0].region = Region;
                    rtable[0].u = 1;

                }


		    }

		    int IndTarget = (actual_target & ((1 << 18) - 1)) + (PtRegion << 18);
		    gtable[HitBank][GI[HitBank]].target = IndTarget;

		}
	}

// update the u bit
	if (HitBank != 0)
	    if (LongestMatchPred != alttaken) {
            if (LongestMatchPred == actual_target) {
                gtable[HitBank][GI[HitBank]].u = 1;

            }
	    }

//END PREDICTOR UPDATE
    }


//  UPDATE  RETIRE HISTORY PATH  
    HistoryUpdate (ip, branch_type, taken, actual_target,  Retire_ptghist,
		     Retire_ch_i, Retire_ch_t[0], Retire_ch_t[1]);

    
  
}