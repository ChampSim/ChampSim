// WARNING!: single-core only.
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace std;

#include "ooo_cpu.h"

#define BORNTICK 1024
// To get the predictor storage budget on stderr  uncomment the next line
// #define PRINTSIZE
#include <vector>
long long IMLIcount; // use to monitor the iteration number

#define SC   // 8.2 % if TAGE alone
#define IMLI // 0.2 %
#define LOCALH

#ifdef LOCALH         // 2.7 %
#define LOOPPREDICTOR // loop predictor enable
#define LOCALS        // enable the 2nd local history
#define LOCALT        // enables the 3rd local history

#endif

// The statistical corrector components

#define PERCWIDTH 6 // Statistical corrector  counter width 5 -> 6 : 0.6 %
// The three BIAS tables in the SC component
// We play with the TAGE  confidence here, with the number of the hitting bank
#define LOGBIAS 8
int8_t Bias[(1 << LOGBIAS)];
#define INDBIAS (((((PC ^ (PC >> 2)) << 1) ^ (LowConf & (LongestMatchPred != alttaken))) << 1) + pred_inter) & ((1 << LOGBIAS) - 1)
int8_t BiasSK[(1 << LOGBIAS)];
#define INDBIASSK (((((PC ^ (PC >> (LOGBIAS - 2))) << 1) ^ (HighConf)) << 1) + pred_inter) & ((1 << LOGBIAS) - 1)

int8_t BiasBank[(1 << LOGBIAS)];

#define INDBIASBANK \
  (pred_inter + (((HitBank + 1) / 4) << 4) + (HighConf << 1) + (LowConf << 2) + ((AltBank != 0) << 3) + ((PC ^ (PC >> 2)) << 7)) & ((1 << LOGBIAS) - 1)

// In all th GEHL components, the two tables with the shortest history lengths
// have only half of the entries.

// IMLI-SIC -> Micro 2015  paper: a big disappointment on  CBP2016 traces
#ifdef IMLI
#define LOGINB 8 // 128-entry
#define INB 1
int Im[INB] = {8};
int8_t IGEHLA[INB][(1 << LOGINB)] = {{0}};

int8_t* IGEHL[INB];

#define LOGIMNB 9 // 2* 256 -entry
#define IMNB 2

int IMm[IMNB] = {10, 4};
int8_t IMGEHLA[IMNB][(1 << LOGIMNB)] = {{0}};

int8_t* IMGEHL[IMNB];
long long IMHIST[256];

#endif

// global branch GEHL
#define LOGGNB 10 // 1 1K + 2 * 512-entry tables
#define GNB 3
int Gm[GNB] = {40, 24, 10};
int8_t GGEHLA[GNB][(1 << LOGGNB)] = {{0}};

int8_t* GGEHL[GNB];

// variation on global branch history
#define PNB 3
#define LOGPNB 9 // 1 1K + 2 * 512-entry tables
int Pm[PNB] = {25, 16, 9};
int8_t PGEHLA[PNB][(1 << LOGPNB)] = {{0}};

int8_t* PGEHL[PNB];

// first local history
#define LOGLNB 10 // 1 1K + 2 * 512-entry tables
#define LNB 3
int Lm[LNB] = {11, 6, 3};
int8_t LGEHLA[LNB][(1 << LOGLNB)] = {{0}};

int8_t* LGEHL[LNB];
#define LOGLOCAL 8
#define NLOCAL (1 << LOGLOCAL)
#define INDLOCAL ((PC ^ (PC >> 2)) & (NLOCAL - 1))
long long L_shist[NLOCAL]; // local histories

// second local history
#define LOGSNB 9 // 1 1K + 2 * 512-entry tables
#define SNB 3
int Sm[SNB] = {16, 11, 6};
int8_t SGEHLA[SNB][(1 << LOGSNB)] = {{0}};

int8_t* SGEHL[SNB];
#define LOGSECLOCAL 4
#define NSECLOCAL (1 << LOGSECLOCAL) // Number of second local histories
#define INDSLOCAL (((PC ^ (PC >> 5))) & (NSECLOCAL - 1))
long long S_slhist[NSECLOCAL];

// third local history
#define LOGTNB 10 // 2 * 512-entry tables
#define TNB 2
int Tm[TNB] = {9, 4};
int8_t TGEHLA[TNB][(1 << LOGTNB)] = {{0}};

int8_t* TGEHL[TNB];
#define NTLOCAL 16
#define INDTLOCAL (((PC ^ (PC >> (LOGTNB)))) & (NTLOCAL - 1)) // different hash for the history
long long T_slhist[NTLOCAL];

// playing with putting more weights (x2)  on some of the SC components
// playing on using different update thresholds on SC
// update threshold for the statistical corrector
#define VARTHRES
#define WIDTHRES 12
#define WIDTHRESP 8
#ifdef VARTHRES
#define LOGSIZEUP 6 // not worth increasing
#else
#define LOGSIZEUP 0
#endif
#define LOGSIZEUPS (LOGSIZEUP / 2)
int updatethreshold;
int Pupdatethreshold[(1 << LOGSIZEUP)]; // size is fixed by LOGSIZEUP
#define INDUPD (PC ^ (PC >> 2)) & ((1 << LOGSIZEUP) - 1)
#define INDUPDS ((PC ^ (PC >> 2)) & ((1 << (LOGSIZEUPS)) - 1))
int8_t WG[(1 << LOGSIZEUPS)];
int8_t WL[(1 << LOGSIZEUPS)];
int8_t WS[(1 << LOGSIZEUPS)];
int8_t WT[(1 << LOGSIZEUPS)];
int8_t WP[(1 << LOGSIZEUPS)];
int8_t WI[(1 << LOGSIZEUPS)];
int8_t WIM[(1 << LOGSIZEUPS)];
int8_t WB[(1 << LOGSIZEUPS)];
#define EWIDTH 6
int LSUM;

// The two counters used to choose between TAGE and SC on Low Conf SC
int8_t FirstH, SecondH;
bool MedConf; // is the TAGE prediction medium confidence

#define CONFWIDTH 7 // for the counters in the choser
#define HISTBUFFERLENGTH \
  4096 // we use a 4K entries history buffer to store the branch history (this
       // allows us to explore using history length up to 4K)

// utility class for index computation
// this is the cyclic shift register for folding
// a long global history into a smaller number of bits; see P. Michaud's
// PPM-like predictor at CBP-1
class folded_history
{
public:
  unsigned comp;
  int CLENGTH;
  int OLENGTH;
  int OUTPOINT;

  folded_history() {}

  void init(int original_length, int compressed_length)
  {
    comp = 0;
    OLENGTH = original_length;
    CLENGTH = compressed_length;
    OUTPOINT = OLENGTH % CLENGTH;
  }

  void update(uint8_t* h, int PT)
  {
    comp = (comp << 1) ^ h[PT & (HISTBUFFERLENGTH - 1)];
    comp ^= h[(PT + OLENGTH) & (HISTBUFFERLENGTH - 1)] << OUTPOINT;
    comp ^= (comp >> CLENGTH);
    comp = (comp) & ((1 << CLENGTH) - 1);
  }
};

class bentry // TAGE bimodal table entry
{
public:
  int8_t hyst;
  int8_t pred;

  bentry()
  {
    pred = 0;

    hyst = 1;
  }
};
class gentry // TAGE global table entry
{
public:
  int8_t ctr;
  uint32_t tag;
  int8_t u;

  gentry()
  {
    ctr = 0;
    u = 0;
    tag = 0;
  }
};

#define POWER
// use geometric history length

#define NHIST 36 // twice the number of different histories

#define NBANKLOW \
  10                 // number of banks in the shared bank-interleaved for the low history
                     // lengths
#define NBANKHIGH 20 // number of banks in the shared bank-interleaved for the  history lengths

int SizeTable[NHIST + 1];

#define BORN \
  13 // below BORN in the table for low history lengths, >= BORN in the table
     // for high history lengths,

// we use 2-way associativity for the medium history lengths
#define BORNINFASSOC 9 // 2 -way assoc for those banks 0.4 %
#define BORNSUPASSOC 23

/*in practice 2 bits or 3 bits par branch: around 1200 cond. branchs*/

#define MINHIST 6 // not optimized so far
#define MAXHIST 3000

#define LOGG 10 /* logsize of the  banks in the  tagged TAGE tables */
#define TBITS \
  8 // minimum width of the tags  (low history lengths), +4 for high history
    // lengths

bool NOSKIP[NHIST + 1]; // to manage the associativity for different history lengths
bool LowConf;
bool HighConf;

#define NNN 1       // number of extra entries allocated on a TAGE misprediction (1+NNN)
#define HYSTSHIFT 2 // bimodal hysteresis shared by 4 entries
#define LOGB 13     // log of number of entries in bimodal predictor

#define PHISTWIDTH 27 // width of the path history used in TAGE
#define UWIDTH \
  1              // u counter width on TAGE (2 bits not worth the effort for a 512 Kbits
                 // predictor 0.2 %)
#define CWIDTH 3 // predictor counter width on the TAGE tagged tables

// the counter(s) to chose between longest match and alternate prediction on
// TAGE when weak counters
#define LOGSIZEUSEALT 4
bool AltConf; // Confidence on the alternate prediction
#define ALTWIDTH 5
#define SIZEUSEALT (1 << (LOGSIZEUSEALT))
#define INDUSEALT (((((HitBank - 1) / 8) << 1) + AltConf) % (SIZEUSEALT - 1))
int8_t use_alt_on_na[SIZEUSEALT];
// very marginal benefit
long long GHIST;
int8_t BIM;

int TICK; // for the reset of the u counter
uint8_t ghist[HISTBUFFERLENGTH];
int ptghist;
long long phist;                   // path history
folded_history ch_i[NHIST + 1];    // utility for computing TAGE indices
folded_history ch_t[2][NHIST + 1]; // utility for computing TAGE tags

// For the TAGE predictor
bentry* btable;            // bimodal TAGE table
gentry* gtable[NHIST + 1]; // tagged TAGE tables
int m[NHIST + 1];
int TB[NHIST + 1];
int logg[NHIST + 1];

int GI[NHIST + 1];    // indexes to the different tables are computed only once
uint32_t GTAG[NHIST + 1]; // tags for the different tables are computed only once
int BI;               // index of the bimodal table
bool pred_taken;      // prediction
bool alttaken;        // alternate  TAGEprediction
bool tage_pred;       // TAGE prediction
bool LongestMatchPred;
int HitBank; // longest matching bank
int AltBank; // alternate matching bank
int Seed;    // for the pseudo-random number generator
bool pred_inter;

#ifdef LOOPPREDICTOR
// parameters of the loop predictor
#define LOGL 5
#define WIDTHNBITERLOOP 10 // we predict only loops with less than 1K iterations
#define LOOPTAG 10         // tag width in the loop predictor

class lentry // loop predictor entry
{
public:
  uint16_t NbIter;      // 10 bits
  uint8_t confid;       // 4bits
  uint16_t CurrentIter; // 10 bits

  uint16_t TAG; // 10 bits
  uint8_t age;  // 4 bits
  bool dir;     // 1 bit

  // 39 bits per entry
  lentry()
  {
    confid = 0;
    CurrentIter = 0;
    NbIter = 0;
    TAG = 0;
    age = 0;
    dir = false;
  }
};

lentry* ltable; // loop predictor table
// variables for the loop predictor
bool predloop; // loop predictor prediction
int LIB;
int LI;
int LHIT;        // hitting way in the loop predictor
int LTAG;        // tag on the loop predictor
bool LVALID;     // validity of the loop predictor prediction
int8_t WITHLOOP; // counter to monitor whether or not loop prediction is
                 // beneficial

#endif

int predictorsize()
{
  int STORAGESIZE = 0;
  int inter = 0;

  STORAGESIZE += NBANKHIGH * (1 << (logg[BORN])) * (CWIDTH + UWIDTH + TB[BORN]);
  STORAGESIZE += NBANKLOW * (1 << (logg[1])) * (CWIDTH + UWIDTH + TB[1]);

  STORAGESIZE += (SIZEUSEALT)*ALTWIDTH;
  STORAGESIZE += (1 << LOGB) + (1 << (LOGB - HYSTSHIFT));
  STORAGESIZE += m[NHIST];
  STORAGESIZE += PHISTWIDTH;
  STORAGESIZE += 10; // the TICK counter

  fprintf(stderr, " (TAGE %d) ", STORAGESIZE);
#ifdef SC
#ifdef LOOPPREDICTOR

  inter = (1 << LOGL) * (2 * WIDTHNBITERLOOP + LOOPTAG + 4 + 4 + 1);
  fprintf(stderr, " (LOOP %d) ", inter);
  STORAGESIZE += inter;

#endif

  inter += WIDTHRES;
  inter = WIDTHRESP * ((1 << LOGSIZEUP));  // the update threshold counters
  inter += 3 * EWIDTH * (1 << LOGSIZEUPS); // the extra weight of the partial sums
  inter += (PERCWIDTH)*3 * (1 << (LOGBIAS));

  inter += (GNB - 2) * (1 << (LOGGNB)) * (PERCWIDTH) + (1 << (LOGGNB - 1)) * (2 * PERCWIDTH);
  inter += Gm[0]; // global histories for SC
  inter += (PNB - 2) * (1 << (LOGPNB)) * (PERCWIDTH) + (1 << (LOGPNB - 1)) * (2 * PERCWIDTH);
  // we use phist already counted for these tables

#ifdef LOCALH
  inter += (LNB - 2) * (1 << (LOGLNB)) * (PERCWIDTH) + (1 << (LOGLNB - 1)) * (2 * PERCWIDTH);
  inter += NLOCAL * Lm[0];
  inter += EWIDTH * (1 << LOGSIZEUPS);
#ifdef LOCALS
  inter += (SNB - 2) * (1 << (LOGSNB)) * (PERCWIDTH) + (1 << (LOGSNB - 1)) * (2 * PERCWIDTH);
  inter += NSECLOCAL * (Sm[0]);
  inter += EWIDTH * (1 << LOGSIZEUPS);

#endif
#ifdef LOCALT
  inter += (TNB - 2) * (1 << (LOGTNB)) * (PERCWIDTH) + (1 << (LOGTNB - 1)) * (2 * PERCWIDTH);
  inter += NTLOCAL * Tm[0];
  inter += EWIDTH * (1 << LOGSIZEUPS);
#endif

#endif

#ifdef IMLI

  inter += (1 << (LOGINB - 1)) * PERCWIDTH;
  inter += Im[0];

  inter += IMNB * (1 << (LOGIMNB - 1)) * PERCWIDTH;
  inter += 2 * EWIDTH * (1 << LOGSIZEUPS); // the extra weight of the partial sums
  inter += 256 * IMm[0];
#endif
  inter += 2 * CONFWIDTH; // the 2 counters in the choser
  STORAGESIZE += inter;

  fprintf(stderr, " (SC %d) ", inter);
#endif
#ifdef PRINTSIZE
  fprintf(stderr, " (TOTAL %d bits %d Kbits) ", STORAGESIZE, STORAGESIZE / 1024);
  fprintf(stdout, " (TOTAL %d bits %d Kbits) ", STORAGESIZE, STORAGESIZE / 1024);
#endif

  return (STORAGESIZE);
}

class PREDICTOR
{
public:
  int THRES;

  PREDICTOR(void)
  {
    reinit();
#ifdef PRINTSIZE
    predictorsize();
#endif
  }

  void reinit()
  {
    m[1] = MINHIST;
    m[NHIST / 2] = MAXHIST;
    for (int i = 2; i <= NHIST / 2; i++) {
      m[i] = (int)(((double)MINHIST * pow((double)(MAXHIST) / (double)MINHIST, (double)(i - 1) / (double)(((NHIST / 2) - 1)))) + 0.5);
      //      fprintf(stderr, "(%d %d)", m[i],i);
    }
    for (int i = 1; i <= NHIST; i++) {
      NOSKIP[i] = ((i - 1) & 1) || ((i >= BORNINFASSOC) & (i < BORNSUPASSOC));
    }

    NOSKIP[4] = 0;
    NOSKIP[NHIST - 2] = 0;
    NOSKIP[8] = 0;
    NOSKIP[NHIST - 6] = 0;
    // just eliminate some extra tables (very very marginal)

    for (int i = NHIST; i > 1; i--) {
      m[i] = m[(i + 1) / 2];
    }
    for (int i = 1; i <= NHIST; i++) {
      TB[i] = TBITS + 4 * (i >= BORN);
      logg[i] = LOGG;
    }

#ifdef LOOPPREDICTOR
    ltable = new lentry[1 << (LOGL)];
#endif

    gtable[1] = new gentry[NBANKLOW * (1 << LOGG)];
    SizeTable[1] = NBANKLOW * (1 << LOGG);

    gtable[BORN] = new gentry[NBANKHIGH * (1 << LOGG)];
    SizeTable[BORN] = NBANKHIGH * (1 << LOGG);

    for (int i = BORN + 1; i <= NHIST; i++)
      gtable[i] = gtable[BORN];
    for (int i = 2; i <= BORN - 1; i++)
      gtable[i] = gtable[1];
    btable = new bentry[1 << LOGB];

    for (int i = 1; i <= NHIST; i++) {
      ch_i[i].init(m[i], (logg[i]));
      ch_t[0][i].init(ch_i[i].OLENGTH, TB[i]);
      ch_t[1][i].init(ch_i[i].OLENGTH, TB[i] - 1);
    }
#ifdef LOOPPREDICTOR
    LVALID = false;
    WITHLOOP = -1;
#endif
    Seed = 0;

    TICK = 0;
    phist = 0;
    Seed = 0;

    for (int i = 0; i < HISTBUFFERLENGTH; i++)
      ghist[0] = 0;
    ptghist = 0;
    updatethreshold = 35 << 3;

    for (int i = 0; i < (1 << LOGSIZEUP); i++)
      Pupdatethreshold[i] = 0;
    for (int i = 0; i < GNB; i++)
      GGEHL[i] = &GGEHLA[i][0];
    for (int i = 0; i < LNB; i++)
      LGEHL[i] = &LGEHLA[i][0];

    for (int i = 0; i < GNB; i++)
      for (int j = 0; j < ((1 << LOGGNB) - 1); j++) {
        if (!(j & 1)) {
          GGEHL[i][j] = -1;
        }
      }
    for (int i = 0; i < LNB; i++)
      for (int j = 0; j < ((1 << LOGLNB) - 1); j++) {
        if (!(j & 1)) {
          LGEHL[i][j] = -1;
        }
      }

    for (int i = 0; i < SNB; i++)
      SGEHL[i] = &SGEHLA[i][0];
    for (int i = 0; i < TNB; i++)
      TGEHL[i] = &TGEHLA[i][0];
    for (int i = 0; i < PNB; i++)
      PGEHL[i] = &PGEHLA[i][0];
#ifdef IMLI
#ifdef IMLIOH
    for (int i = 0; i < FNB; i++)
      FGEHL[i] = &FGEHLA[i][0];

    for (int i = 0; i < FNB; i++)
      for (int j = 0; j < ((1 << LOGFNB) - 1); j++) {
        if (!(j & 1)) {
          FGEHL[i][j] = -1;
        }
      }
#endif
    for (int i = 0; i < INB; i++)
      IGEHL[i] = &IGEHLA[i][0];
    for (int i = 0; i < INB; i++)
      for (int j = 0; j < ((1 << LOGINB) - 1); j++) {
        if (!(j & 1)) {
          IGEHL[i][j] = -1;
        }
      }
    for (int i = 0; i < IMNB; i++)
      IMGEHL[i] = &IMGEHLA[i][0];
    for (int i = 0; i < IMNB; i++)
      for (int j = 0; j < ((1 << LOGIMNB) - 1); j++) {
        if (!(j & 1)) {
          IMGEHL[i][j] = -1;
        }
      }

#endif
    for (int i = 0; i < SNB; i++)
      for (int j = 0; j < ((1 << LOGSNB) - 1); j++) {
        if (!(j & 1)) {
          SGEHL[i][j] = -1;
        }
      }
    for (int i = 0; i < TNB; i++)
      for (int j = 0; j < ((1 << LOGTNB) - 1); j++) {
        if (!(j & 1)) {
          TGEHL[i][j] = -1;
        }
      }
    for (int i = 0; i < PNB; i++)
      for (int j = 0; j < ((1 << LOGPNB) - 1); j++) {
        if (!(j & 1)) {
          PGEHL[i][j] = -1;
        }
      }

    for (int i = 0; i < (1 << LOGB); i++) {
      btable[i].pred = 0;
      btable[i].hyst = 1;
    }

    for (int j = 0; j < (1 << LOGBIAS); j++) {
      switch (j & 3) {
      case 0:
        BiasSK[j] = -8;
        break;
      case 1:
        BiasSK[j] = 7;
        break;
      case 2:
        BiasSK[j] = -32;

        break;
      case 3:
        BiasSK[j] = 31;
        break;
      }
    }
    for (int j = 0; j < (1 << LOGBIAS); j++) {
      switch (j & 3) {
      case 0:
        Bias[j] = -32;

        break;
      case 1:
        Bias[j] = 31;
        break;
      case 2:
        Bias[j] = -1;
        break;
      case 3:
        Bias[j] = 0;
        break;
      }
    }
    for (int j = 0; j < (1 << LOGBIAS); j++) {
      switch (j & 3) {
      case 0:
        BiasBank[j] = -32;

        break;
      case 1:
        BiasBank[j] = 31;
        break;
      case 2:
        BiasBank[j] = -1;
        break;
      case 3:
        BiasBank[j] = 0;
        break;
      }
    }
    for (int i = 0; i < SIZEUSEALT; i++) {
      use_alt_on_na[i] = 0;
    }
    for (int i = 0; i < (1 << LOGSIZEUPS); i++) {
      WG[i] = 7;
      WL[i] = 7;
      WS[i] = 7;
      WT[i] = 7;
      WP[i] = 7;
      WI[i] = 7;
      WB[i] = 4;
    }
    TICK = 0;
    for (int i = 0; i < NLOCAL; i++) {
      L_shist[i] = 0;
    }
    for (int i = 0; i < NSECLOCAL; i++) {
      S_slhist[i] = 0;
    }
    GHIST = 0;
    ptghist = 0;
    phist = 0;
  }

  // index function for the bimodal table

  int bindex(uint64_t PC) { return ((PC ^ (PC >> LOGB)) & ((1 << (LOGB)) - 1)); }

  // the index functions for the tagged tables uses path history as in the OGEHL
  // predictor
  // F serves to mix path history: not very important impact

  int F(long long A, int size, int bank)
  {
    int A1, A2;
    A = A & ((1 << size) - 1);
    A1 = (A & ((1 << logg[bank]) - 1));
    A2 = (A >> logg[bank]);

    if (bank < logg[bank])
      A2 = ((A2 << bank) & ((1 << logg[bank]) - 1)) + (A2 >> (logg[bank] - bank));
    A = A1 ^ A2;
    if (bank < logg[bank])
      A = ((A << bank) & ((1 << logg[bank]) - 1)) + (A >> (logg[bank] - bank));
    return (A);
  }

  // gindex computes a full hash of PC, ghist and phist
  int gindex(unsigned int PC, int bank, long long hist, folded_history* ch_i_aux)
  {
    int index;
    int M = (m[bank] > PHISTWIDTH) ? PHISTWIDTH : m[bank];
    index = PC ^ (PC >> (abs(logg[bank] - bank) + 1)) ^ ch_i_aux[bank].comp ^ F(hist, M, bank);

    return (index & ((1 << (logg[bank])) - 1));
  }

  //  tag computation
  uint16_t gtag(unsigned int PC, int bank, folded_history* ch0, folded_history* ch1)
  {
    int tag = (PC) ^ ch0[bank].comp ^ (ch1[bank].comp << 1);
    return (tag & ((1 << (TB[bank])) - 1));
  }

  // up-down saturating counter
  void ctrupdate(int8_t& ctr, bool taken, int nbits)
  {
    if (taken) {
      if (ctr < ((1 << (nbits - 1)) - 1))
        ctr++;
    } else {
      if (ctr > -(1 << (nbits - 1)))
        ctr--;
    }
  }

  bool getbim()
  {
    BIM = (btable[BI].pred << 1) + (btable[BI >> HYSTSHIFT].hyst);
    HighConf = (BIM == 0) || (BIM == 3);
    LowConf = !HighConf;
    AltConf = HighConf;
    MedConf = false;
    return (btable[BI].pred > 0);
  }

  void baseupdate(bool Taken)
  {
    int inter = BIM;
    if (Taken) {
      if (inter < 3)
        inter += 1;
    } else if (inter > 0)
      inter--;
    btable[BI].pred = inter >> 1;
    btable[BI >> HYSTSHIFT].hyst = (inter & 1);
  };

  // just a simple pseudo random number generator: use available information
  //  to allocate entries  in the loop predictor
  int MYRANDOM()
  {
    Seed++;
    Seed ^= phist;
    Seed = (Seed >> 21) + (Seed << 11);
    Seed ^= ptghist;
    Seed = (Seed >> 10) + (Seed << 22);
    return (Seed);
  };

  //  TAGE PREDICTION: same code at fetch or retire time but the index and tags
  //  must recomputed
  void Tagepred(uint64_t PC)
  {
    HitBank = 0;
    AltBank = 0;
    for (int i = 1; i <= NHIST; i += 2) {
      GI[i] = gindex(PC, i, phist, ch_i);
      GTAG[i] = gtag(PC, i, ch_t[0], ch_t[1]);
      GTAG[i + 1] = GTAG[i];
      GI[i + 1] = GI[i] ^ (GTAG[i] & ((1 << LOGG) - 1));
    }
    int T = (PC ^ (phist & ((1 << m[BORN]) - 1))) % NBANKHIGH;
    // int T = (PC ^ phist) % NBANKHIGH;
    for (int i = BORN; i <= NHIST; i++)
      if (NOSKIP[i]) {
        GI[i] += (T << LOGG);
        T++;
        T = T % NBANKHIGH;
      }
    T = (PC ^ (phist & ((1 << m[1]) - 1))) % NBANKLOW;

    for (int i = 1; i <= BORN - 1; i++)
      if (NOSKIP[i]) {
        GI[i] += (T << LOGG);
        T++;
        T = T % NBANKLOW;
      }
    // just do not forget most address are aligned on 4 bytes
    BI = (PC ^ (PC >> 2)) & ((1 << LOGB) - 1);

    {
      alttaken = getbim();
      tage_pred = alttaken;
      LongestMatchPred = alttaken;
    }

    // Look for the bank with longest matching history
    for (int i = NHIST; i > 0; i--) {
      if (NOSKIP[i])
        if (gtable[i][GI[i]].tag == GTAG[i]) {
          HitBank = i;
          LongestMatchPred = (gtable[HitBank][GI[HitBank]].ctr >= 0);
          break;
        }
    }

    // Look for the alternate bank
    for (int i = HitBank - 1; i > 0; i--) {
      if (NOSKIP[i])
        if (gtable[i][GI[i]].tag == GTAG[i]) {
          AltBank = i;
          break;
        }
    }
    // computes the prediction and the alternate prediction

    if (HitBank > 0) {
      if (AltBank > 0) {
        alttaken = (gtable[AltBank][GI[AltBank]].ctr >= 0);
        AltConf = (abs(2 * gtable[AltBank][GI[AltBank]].ctr + 1) > 1);

      } else
        alttaken = getbim();

      // if the entry is recognized as a newly allocated entry and
      // USE_ALT_ON_NA is positive  use the alternate prediction

      bool Huse_alt_on_na = (use_alt_on_na[INDUSEALT] >= 0);
      if ((!Huse_alt_on_na) || (abs(2 * gtable[HitBank][GI[HitBank]].ctr + 1) > 1))
        tage_pred = LongestMatchPred;
      else
        tage_pred = alttaken;

      HighConf = (abs(2 * gtable[HitBank][GI[HitBank]].ctr + 1) >= (1 << CWIDTH) - 1);
      LowConf = (abs(2 * gtable[HitBank][GI[HitBank]].ctr + 1) == 1);
      MedConf = (abs(2 * gtable[HitBank][GI[HitBank]].ctr + 1) == 5);
    }
  }

  // compute the prediction
  bool GetPrediction(uint64_t PC)
  {
    // computes the TAGE table addresses and the partial tags

    Tagepred(PC);
    pred_taken = tage_pred;
#ifndef SC
    return (tage_pred);
#endif

#ifdef LOOPPREDICTOR
    predloop = getloop(PC); // loop prediction
    pred_taken = ((WITHLOOP >= 0) && (LVALID)) ? predloop : pred_taken;
#endif
    pred_inter = pred_taken;

    // Compute the SC prediction

    LSUM = 0;

    // integrate BIAS prediction
    int8_t ctr = Bias[INDBIAS];

    LSUM += (2 * ctr + 1);
    ctr = BiasSK[INDBIASSK];
    LSUM += (2 * ctr + 1);
    ctr = BiasBank[INDBIASBANK];
    LSUM += (2 * ctr + 1);
#ifdef VARTHRES
    LSUM = (1 + (WB[INDUPDS] >= 0)) * LSUM;
#endif
    // integrate the GEHL predictions
    LSUM += Gpredict((PC << 1) + pred_inter, GHIST, Gm, GGEHL, GNB, LOGGNB, WG);
    LSUM += Gpredict(PC, phist, Pm, PGEHL, PNB, LOGPNB, WP);
#ifdef LOCALH
    LSUM += Gpredict(PC, L_shist[INDLOCAL], Lm, LGEHL, LNB, LOGLNB, WL);
#ifdef LOCALS
    LSUM += Gpredict(PC, S_slhist[INDSLOCAL], Sm, SGEHL, SNB, LOGSNB, WS);
#endif
#ifdef LOCALT
    LSUM += Gpredict(PC, T_slhist[INDTLOCAL], Tm, TGEHL, TNB, LOGTNB, WT);
#endif
#endif

#ifdef IMLI
    LSUM += Gpredict(PC, IMHIST[(IMLIcount)], IMm, IMGEHL, IMNB, LOGIMNB, WIM);
    LSUM += Gpredict(PC, IMLIcount, Im, IGEHL, INB, LOGINB, WI);
#endif
    bool SCPRED = (LSUM >= 0);
    // just  an heuristic if the respective contribution of component groups can
    // be multiplied by 2 or not
    THRES = (updatethreshold >> 3) + Pupdatethreshold[INDUPD]
#ifdef VARTHRES
            + 12
                  * ((WB[INDUPDS] >= 0) + (WP[INDUPDS] >= 0)
#ifdef LOCALH
                     + (WS[INDUPDS] >= 0) + (WT[INDUPDS] >= 0) + (WL[INDUPDS] >= 0)
#endif
                     + (WG[INDUPDS] >= 0)
#ifdef IMLI
                     + (WI[INDUPDS] >= 0)
#endif
                  )
#endif
        ;

    // Minimal benefit in trying to avoid accuracy loss on low confidence SC
    // prediction and  high/medium confidence on TAGE
    //  but just uses 2 counters 0.3 % MPKI reduction
    if (pred_inter != SCPRED) {
      // Choser uses TAGE confidence and |LSUM|
      pred_taken = SCPRED;
      if (HighConf) {
        if ((abs(LSUM) < THRES / 4)) {
          pred_taken = pred_inter;
        }

        else if ((abs(LSUM) < THRES / 2))
          pred_taken = (SecondH < 0) ? SCPRED : pred_inter;
      }

      if (MedConf)
        if ((abs(LSUM) < THRES / 4)) {
          pred_taken = (FirstH < 0) ? SCPRED : pred_inter;
        }
    }

    return pred_taken;
  }

  void HistoryUpdate(uint64_t PC, uint8_t opType, bool taken, uint64_t target, long long& X, int& Y, folded_history* H, folded_history* G, folded_history* J)
  {
    int brtype = 0;

    switch (opType) {
    case BRANCH_INDIRECT:
    case BRANCH_INDIRECT_CALL:
    case BRANCH_RETURN:
    case BRANCH_OTHER:
      brtype = 2;
      break;
    case BRANCH_DIRECT_JUMP:
    case BRANCH_CONDITIONAL:
    case BRANCH_DIRECT_CALL:
      brtype = 0;
      break;
    default:
      exit(1);
    }
    switch (opType) {
    case BRANCH_CONDITIONAL:
    case BRANCH_OTHER:
      brtype += 1;
      break;
    }

    // special treatment for indirect  branchs;
    int maxt = 2;
    if (brtype & 1)
      maxt = 2;
    else if ((brtype & 2))
      maxt = 3;

#ifdef IMLI
    if (brtype & 1) {
#ifdef IMLI
      IMHIST[IMLIcount] = (IMHIST[IMLIcount] << 1) + taken;
#endif
      if (target < PC)

      {
        // This branch corresponds to a loop
        if (!taken) {
          // exit of the "loop"
          IMLIcount = 0;
        }
        if (taken) {
          if (IMLIcount < ((1 << Im[0]) - 1))
            IMLIcount++;
        }
      }
    }

#endif

    if (brtype & 1) {
      GHIST = (GHIST << 1) + (taken & (target < PC));
      L_shist[INDLOCAL] = (L_shist[INDLOCAL] << 1) + (taken);
      S_slhist[INDSLOCAL] = ((S_slhist[INDSLOCAL] << 1) + taken) ^ (PC & 15);
      T_slhist[INDTLOCAL] = (T_slhist[INDTLOCAL] << 1) + taken;
    }

    int T = ((PC ^ (PC >> 2))) ^ taken;
    int PATH = PC ^ (PC >> 2) ^ (PC >> 4);
    if ((brtype == 3) & taken) {
      T = (T ^ (target >> 2));
      PATH = PATH ^ (target >> 2) ^ (target >> 4);
    }

    for (int t = 0; t < maxt; t++) {
      bool DIR = (T & 1);
      T >>= 1;
      int PATHBIT = (PATH & 127);
      PATH >>= 1;
      // update  history
      Y--;
      ghist[Y & (HISTBUFFERLENGTH - 1)] = DIR;
      X = (X << 1) ^ PATHBIT;

      for (int i = 1; i <= NHIST; i++) {
        H[i].update(ghist, Y);
        G[i].update(ghist, Y);
        J[i].update(ghist, Y);
      }
    }

    X = (X & ((1 << PHISTWIDTH) - 1));

    // END UPDATE  HISTORIES
  }

  // PREDICTOR UPDATE

  void UpdatePredictor(uint64_t PC, uint8_t opType, bool resolveDir, bool predDir, uint64_t branchTarget)
  {
#ifdef SC
#ifdef LOOPPREDICTOR
    if (LVALID) {
      if (pred_taken != predloop)
        ctrupdate(WITHLOOP, (predloop == resolveDir), 7);
    }
    loopupdate(PC, resolveDir, (pred_taken != resolveDir));
#endif

    bool SCPRED = (LSUM >= 0);
    if (pred_inter != SCPRED) {
      if ((abs(LSUM) < THRES))
        if ((HighConf)) {
          if ((abs(LSUM) < THRES / 2))
            if ((abs(LSUM) >= THRES / 4))
              ctrupdate(SecondH, (pred_inter == resolveDir), CONFWIDTH);
        }
      if ((MedConf))
        if ((abs(LSUM) < THRES / 4)) {
          ctrupdate(FirstH, (pred_inter == resolveDir), CONFWIDTH);
        }
    }

    if ((SCPRED != resolveDir) || ((abs(LSUM) < THRES))) {
      {
        if (SCPRED != resolveDir) {
          Pupdatethreshold[INDUPD] += 1;
          updatethreshold += 1;
        }

        else {
          Pupdatethreshold[INDUPD] -= 1;
          updatethreshold -= 1;
        }

        if (Pupdatethreshold[INDUPD] >= (1 << (WIDTHRESP - 1)))
          Pupdatethreshold[INDUPD] = (1 << (WIDTHRESP - 1)) - 1;
        // Pupdatethreshold[INDUPD] could be negative
        if (Pupdatethreshold[INDUPD] < -(1 << (WIDTHRESP - 1)))
          Pupdatethreshold[INDUPD] = -(1 << (WIDTHRESP - 1));
        if (updatethreshold >= (1 << (WIDTHRES - 1)))
          updatethreshold = (1 << (WIDTHRES - 1)) - 1;
        // updatethreshold could be negative
        if (updatethreshold < -(1 << (WIDTHRES - 1)))
          updatethreshold = -(1 << (WIDTHRES - 1));
      }
#ifdef VARTHRES
      {
        int XSUM = LSUM - ((WB[INDUPDS] >= 0) * ((2 * Bias[INDBIAS] + 1) + (2 * BiasSK[INDBIASSK] + 1) + (2 * BiasBank[INDBIASBANK] + 1)));
        if ((XSUM + ((2 * Bias[INDBIAS] + 1) + (2 * BiasSK[INDBIASSK] + 1) + (2 * BiasBank[INDBIASBANK] + 1)) >= 0) != (XSUM >= 0))
          ctrupdate(WB[INDUPDS], (((2 * Bias[INDBIAS] + 1) + (2 * BiasSK[INDBIASSK] + 1) + (2 * BiasBank[INDBIASBANK] + 1) >= 0) == resolveDir), EWIDTH);
      }
#endif
      ctrupdate(Bias[INDBIAS], resolveDir, PERCWIDTH);
      ctrupdate(BiasSK[INDBIASSK], resolveDir, PERCWIDTH);
      ctrupdate(BiasBank[INDBIASBANK], resolveDir, PERCWIDTH);
      Gupdate((PC << 1) + pred_inter, resolveDir, GHIST, Gm, GGEHL, GNB, LOGGNB, WG);
      Gupdate(PC, resolveDir, phist, Pm, PGEHL, PNB, LOGPNB, WP);
#ifdef LOCALH
      Gupdate(PC, resolveDir, L_shist[INDLOCAL], Lm, LGEHL, LNB, LOGLNB, WL);
#ifdef LOCALS
      Gupdate(PC, resolveDir, S_slhist[INDSLOCAL], Sm, SGEHL, SNB, LOGSNB, WS);
#endif
#ifdef LOCALT

      Gupdate(PC, resolveDir, T_slhist[INDTLOCAL], Tm, TGEHL, TNB, LOGTNB, WT);
#endif
#endif

#ifdef IMLI
      Gupdate(PC, resolveDir, IMHIST[(IMLIcount)], IMm, IMGEHL, IMNB, LOGIMNB, WIM);
      Gupdate(PC, resolveDir, IMLIcount, Im, IGEHL, INB, LOGINB, WI);
#endif
    }
#endif

    // TAGE UPDATE
    bool ALLOC = ((tage_pred != resolveDir) & (HitBank < NHIST));

    // do not allocate too often if the overall prediction is correct

    if (HitBank > 0) {
      // Manage the selection between longest matching and alternate matching
      // for "pseudo"-newly allocated longest matching entry
      // this is extremely important for TAGE only, not that important when the
      // overall predictor is implemented
      bool PseudoNewAlloc = (abs(2 * gtable[HitBank][GI[HitBank]].ctr + 1) <= 1);
      // an entry is considered as newly allocated if its prediction counter is
      // weak
      if (PseudoNewAlloc) {
        if (LongestMatchPred == resolveDir)
          ALLOC = false;
        // if it was delivering the correct prediction, no need to allocate a
        // new entry
        // even if the overall prediction was false

        if (LongestMatchPred != alttaken) {
          ctrupdate(use_alt_on_na[INDUSEALT], (alttaken == resolveDir), ALTWIDTH);
        }
      }
    }

    if (pred_taken == resolveDir)
      if ((MYRANDOM() & 31) != 0)
        ALLOC = false;

    if (ALLOC) {
      int T = NNN;

      int A = 1;
      if ((MYRANDOM() & 127) < 32)
        A = 2;
      int Penalty = 0;
      int NA = 0;
      int DEP = ((((HitBank - 1 + 2 * A) & 0xffe)) ^ (MYRANDOM() & 1));
      // just a complex formula to chose between X and X+1, when X is odd: sorry

      for (int I = DEP; I < NHIST; I += 2) {
        int i = I + 1;
        bool Done = false;
        if (NOSKIP[i]) {
          if (gtable[i][GI[i]].u == 0)

          {
#define OPTREMP
// the replacement is optimized with a single u bit: 0.2 %
#ifdef OPTREMP
            if (abs(2 * gtable[i][GI[i]].ctr + 1) <= 3)
#endif
            {
              gtable[i][GI[i]].tag = GTAG[i];
              gtable[i][GI[i]].ctr = (resolveDir) ? 0 : -1;
              NA++;
              if (T <= 0) {
                break;
              }
              I += 2;
              Done = true;
              T -= 1;
            }
#ifdef OPTREMP
            else {
              if (gtable[i][GI[i]].ctr > 0)
                gtable[i][GI[i]].ctr--;
              else
                gtable[i][GI[i]].ctr++;
            }

#endif

          }

          else {
            Penalty++;
          }
        }

        if (!Done) {
          i = (I ^ 1) + 1;
          if (NOSKIP[i]) {
            if (gtable[i][GI[i]].u == 0) {
#ifdef OPTREMP
              if (abs(2 * gtable[i][GI[i]].ctr + 1) <= 3)
#endif

              {
                gtable[i][GI[i]].tag = GTAG[i];
                gtable[i][GI[i]].ctr = (resolveDir) ? 0 : -1;
                NA++;
                if (T <= 0) {
                  break;
                }
                I += 2;
                T -= 1;
              }
#ifdef OPTREMP
              else {
                if (gtable[i][GI[i]].ctr > 0)
                  gtable[i][GI[i]].ctr--;
                else
                  gtable[i][GI[i]].ctr++;
              }

#endif

            } else {
              Penalty++;
            }
          }
        }
      }
      TICK += (Penalty - 2 * NA);

      // just the best formula for the Championship:
      // In practice when one out of two entries are useful
      if (TICK < 0)
        TICK = 0;
      if (TICK >= BORNTICK) {
        for (int i = 1; i <= BORN; i += BORN - 1)
          for (int j = 0; j < SizeTable[i]; j++)
            gtable[i][j].u >>= 1;
        TICK = 0;
      }
    }

    // update predictions
    if (HitBank > 0) {
      if (abs(2 * gtable[HitBank][GI[HitBank]].ctr + 1) == 1)
        if (LongestMatchPred != resolveDir)

        { // acts as a protection
          if (AltBank > 0) {
            ctrupdate(gtable[AltBank][GI[AltBank]].ctr, resolveDir, CWIDTH);
          }
          if (AltBank == 0)
            baseupdate(resolveDir);
        }
      ctrupdate(gtable[HitBank][GI[HitBank]].ctr, resolveDir, CWIDTH);
      // sign changes: no way it can have been useful
      if (abs(2 * gtable[HitBank][GI[HitBank]].ctr + 1) == 1)
        gtable[HitBank][GI[HitBank]].u = 0;
      if (alttaken == resolveDir)
        if (AltBank > 0)
          if (abs(2 * gtable[AltBank][GI[AltBank]].ctr + 1) == 7)
            if (gtable[HitBank][GI[HitBank]].u == 1) {
              if (LongestMatchPred == resolveDir) {
                gtable[HitBank][GI[HitBank]].u = 0;
              }
            }
    }

    else
      baseupdate(resolveDir);

    if (LongestMatchPred != alttaken)
      if (LongestMatchPred == resolveDir) {
        if (gtable[HitBank][GI[HitBank]].u < (1 << UWIDTH) - 1)
          gtable[HitBank][GI[HitBank]].u++;
      }
    // END TAGE UPDATE

    HistoryUpdate(PC, opType, resolveDir, branchTarget, phist, ptghist, ch_i, ch_t[0], ch_t[1]);

    // END PREDICTOR UPDATE
  }
#define GINDEX                                                                                                                                           \
  (((long long)PC) ^ bhist ^ (bhist >> (8 - i)) ^ (bhist >> (16 - 2 * i)) ^ (bhist >> (24 - 3 * i)) ^ (bhist >> (32 - 3 * i)) ^ (bhist >> (40 - 4 * i))) \
      & ((1 << (logs - (i >= (NBR - 2)))) - 1)
  int Gpredict(uint64_t PC, long long BHIST, int* length, int8_t** tab, int NBR, int logs, int8_t* W)
  {
    int PERCSUM = 0;
    for (int i = 0; i < NBR; i++) {
      long long bhist = BHIST & ((long long)((1 << length[i]) - 1));
      long long index = GINDEX;

      int8_t ctr = tab[i][index];

      PERCSUM += (2 * ctr + 1);
    }
#ifdef VARTHRES
    PERCSUM = (1 + (W[INDUPDS] >= 0)) * PERCSUM;
#endif
    return ((PERCSUM));
  }
  void Gupdate(uint64_t PC, bool taken, long long BHIST, int* length, int8_t** tab, int NBR, int logs, int8_t* W)
  {
    int PERCSUM = 0;

    for (int i = 0; i < NBR; i++) {
      long long bhist = BHIST & ((long long)((1 << length[i]) - 1));
      long long index = GINDEX;

      PERCSUM += (2 * tab[i][index] + 1);
      ctrupdate(tab[i][index], taken, PERCWIDTH);
    }
#ifdef VARTHRES
    {
      int XSUM = LSUM - ((W[INDUPDS] >= 0)) * PERCSUM;
      if ((XSUM + PERCSUM >= 0) != (XSUM >= 0))
        ctrupdate(W[INDUPDS], ((PERCSUM >= 0) == taken), EWIDTH);
    }
#endif
  }

  void TrackOtherInst(uint64_t PC, uint8_t opType, bool taken, uint64_t branchTarget)
  {
    HistoryUpdate(PC, opType, taken, branchTarget, phist, ptghist, ch_i, ch_t[0], ch_t[1]);
  }

#ifdef LOOPPREDICTOR
  int lindex(uint64_t PC) { return (((PC ^ (PC >> 2)) & ((1 << (LOGL - 2)) - 1)) << 2); }

// loop prediction: only used if high confidence
// skewed associative 4-way
// At fetch time: speculative
#define CONFLOOP 15
  bool getloop(uint64_t PC)
  {
    LHIT = -1;

    LI = lindex(PC);
    LIB = ((PC >> (LOGL - 2)) & ((1 << (LOGL - 2)) - 1));
    LTAG = (PC >> (LOGL - 2)) & ((1 << 2 * LOOPTAG) - 1);
    LTAG ^= (LTAG >> LOOPTAG);
    LTAG = (LTAG & ((1 << LOOPTAG) - 1));

    for (int i = 0; i < 4; i++) {
      int index = (LI ^ ((LIB >> i) << 2)) + i;

      if (ltable[index].TAG == LTAG) {
        LHIT = i;
        LVALID = ((ltable[index].confid == CONFLOOP) || (ltable[index].confid * ltable[index].NbIter > 128));

        if (ltable[index].CurrentIter + 1 == ltable[index].NbIter)
          return (!(ltable[index].dir));
        return ((ltable[index].dir));
      }
    }

    LVALID = false;
    return (false);
  }

  void loopupdate(uint64_t PC, bool Taken, bool ALLOC)
  {
    if (LHIT >= 0) {
      int index = (LI ^ ((LIB >> LHIT) << 2)) + LHIT;
      // already a hit
      if (LVALID) {
        if (Taken != predloop) {
          // free the entry
          ltable[index].NbIter = 0;
          ltable[index].age = 0;
          ltable[index].confid = 0;
          ltable[index].CurrentIter = 0;
          return;

        } else if ((predloop != tage_pred) || ((MYRANDOM() & 7) == 0))
          if (ltable[index].age < CONFLOOP)
            ltable[index].age++;
      }

      ltable[index].CurrentIter++;
      ltable[index].CurrentIter &= ((1 << WIDTHNBITERLOOP) - 1);
      // loop with more than 2** WIDTHNBITERLOOP iterations are not treated
      // correctly; but who cares :-)
      if (ltable[index].CurrentIter > ltable[index].NbIter) {
        ltable[index].confid = 0;
        ltable[index].NbIter = 0;
        // treat like the 1st encounter of the loop
      }
      if (Taken != ltable[index].dir) {
        if (ltable[index].CurrentIter == ltable[index].NbIter) {
          if (ltable[index].confid < CONFLOOP)
            ltable[index].confid++;
          if (ltable[index].NbIter < 3)
          // just do not predict when the loop count is 1 or 2
          {
            // free the entry
            ltable[index].dir = Taken;
            ltable[index].NbIter = 0;
            ltable[index].age = 0;
            ltable[index].confid = 0;
          }
        } else {
          if (ltable[index].NbIter == 0) {
            // first complete nest;
            ltable[index].confid = 0;
            ltable[index].NbIter = ltable[index].CurrentIter;
          } else {
            // not the same number of iterations as last time: free the entry
            ltable[index].NbIter = 0;
            ltable[index].confid = 0;
          }
        }
        ltable[index].CurrentIter = 0;
      }

    } else if (ALLOC)

    {
      uint64_t X = MYRANDOM() & 3;

      if ((MYRANDOM() & 3) == 0)
        for (int i = 0; i < 4; i++) {
          int LHIT_aux = (X + i) & 3;
          int index = (LI ^ ((LIB >> LHIT_aux) << 2)) + LHIT_aux;
          if (ltable[index].age == 0) {
            ltable[index].dir = !Taken;
            // most of mispredictions are on last iterations
            ltable[index].TAG = LTAG;
            ltable[index].NbIter = 0;
            ltable[index].age = 7;
            ltable[index].confid = 0;
            ltable[index].CurrentIter = 0;
            break;

          } else
            ltable[index].age--;
          break;
        }
    }
  }
#endif
} predictor;

void O3_CPU::initialize_branch_predictor() {}

bool was_predicted_taken;

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
  // switch (branch_type) {
  //   case BRANCH_CONDITIONAL:
  //       was_conditional = true;
  //       break;
  //   case BRANCH_DIRECT_JUMP:
  //   case BRANCH_INDIRECT:
  //   case BRANCH_DIRECT_CALL:
  //   case BRANCH_INDIRECT_CALL:
  //   case BRANCH_RETURN:
  //       was_conditional = false;
  //       break;
  //   case BRANCH_OTHER:
  //       assert(false);
  // }
  // if (was_conditional) {
  //     return was_predicted_taken = predictor.GetPrediction(ip);
  // } else {
  //     return was_predicted_taken = true;
  // }
  return was_predicted_taken = predictor.GetPrediction(ip);
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  if (branch_type == BRANCH_CONDITIONAL) {
    predictor.UpdatePredictor(ip, branch_type, taken, was_predicted_taken, branch_target);
  } else {
    predictor.TrackOtherInst(ip, branch_type, taken, branch_target);
  }
}
