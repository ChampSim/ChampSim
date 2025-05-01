#include "ooo_cpu.h"

#define Tag uint16_t
#define Index uint16_t
#define Path uint64_t
#define History uint64_t
#define TAGE_BIMODAL_TABLE_SIZE 16384
#define TAGE_MAX_INDEX_BITS 12
#define TAGE_NUM_COMPONENTS 12 // TODO
#define TAGE_BASE_COUNTER_BITS 2
#define TAGE_COUNTER_BITS 3
#define TAGE_USEFUL_BITS 2
#define TAGE_GLOBAL_HISTORY_BUFFER_LENGTH 1024
#define TAGE_PATH_HISTORY_BUFFER_LENGTH 32
#define TAGE_MIN_HISTORY_LENGTH 4
#define TAGE_HISTORY_ALPHA 1.6
#define TAGE_RESET_USEFUL_INTERVAL 512000

const uint8_t TAGE_INDEX_BITS[TAGE_NUM_COMPONENTS] = {10, 10, 11, 11, 11, 11, 10, 10, 10, 10, 9, 9};
const uint8_t TAGE_TAG_BITS[TAGE_NUM_COMPONENTS] = {7, 7, 8, 8, 9, 10, 11, 12, 12, 13, 14, 15};

struct tage_predictor_table_entry {
  uint8_t ctr;    // The counter on which prediction is based Range - 0-7
  Tag tag;        // Stores the tag
  uint8_t useful; // Variable to store the usefulness of the entry Range - 0-3
};

class Tage
{
private:
  /* data */
  int num_branches;                               // Stores the number of branch instructions since the last useful reset
  uint8_t bimodal_table[TAGE_BIMODAL_TABLE_SIZE]; // Array represent the counters of the bimodal table
  struct tage_predictor_table_entry predictor_table[TAGE_NUM_COMPONENTS][(1 << TAGE_MAX_INDEX_BITS)];
  uint8_t global_history[TAGE_GLOBAL_HISTORY_BUFFER_LENGTH]; // Stores the global branch history
  uint8_t path_history[TAGE_PATH_HISTORY_BUFFER_LENGTH];     // Stores the last bits of the last N branch PCs
  uint8_t use_alt_on_na;                                     // 4 bit counter to decide between alternate and provider component prediction
  int component_history_lengths[TAGE_NUM_COMPONENTS];        // History lengths used to compute hashes for different components
  uint8_t tage_pred, pred, alt_pred;                         // Final prediction , provider prediction, and alternate prediction
  int pred_comp, alt_comp;                                   // Provider and alternate component of last branch PC
  int STRONG;                                                // Strength of provider prediction counter of last branch PC

public:
  void init();                             // initialise the member variables
  uint8_t predict(uint64_t ip);            // return the prediction from tage
  void update(uint64_t ip, uint8_t taken); // updates the state of tage

  Index get_bimodal_index(uint64_t ip);                           // helper hash function to index into the bimodal table
  Index get_predictor_index(uint64_t ip, int component);          // helper hash function to index into the predictor table using histories
  Tag get_tag(uint64_t ip, int component);                        // helper hash function to get the tag of particular ip and component
  int get_match_below_n(uint64_t ip, int component);              // helper function to find the hit component strictly before the component argument
  void ctr_update(uint8_t& ctr, int cond, int low, int high);     // counter update helper function (including clipping)
  uint8_t get_prediction(uint64_t ip, int comp);                  // helper function for prediction
  Path get_path_history_hash(int component);                      // hepoer hash function to compress the path history
  History get_compressed_global_history(int inSize, int outSize); // Compress global history of last 'inSize' branches into 'outSize' by wrapping the history

  Tage();
  ~Tage();
};

void Tage::init()
{
  /*
  Initializes the member variables
  */
  use_alt_on_na = 8;
  tage_pred = 0;
  for (int i = 0; i < TAGE_BIMODAL_TABLE_SIZE; i++) {
    bimodal_table[i] = (1 << (TAGE_BASE_COUNTER_BITS - 1)); // weakly taken
  }
  for (int i = 0; i < TAGE_NUM_COMPONENTS; i++) {
    for (int j = 0; j < (1 << TAGE_INDEX_BITS[i]); j++) {
      predictor_table[i][j].ctr = (1 << (TAGE_COUNTER_BITS - 1)); // weakly taken
      predictor_table[i][j].useful = 0;                           // not useful
      predictor_table[i][j].tag = 0;
    }
  }

  double power = 1;
  for (int i = 0; i < TAGE_NUM_COMPONENTS; i++) {
    component_history_lengths[i] = int(TAGE_MIN_HISTORY_LENGTH * power + 0.5); // set component history lengths
    power *= TAGE_HISTORY_ALPHA;
  }

  num_branches = 0;
}

uint8_t Tage::get_prediction(uint64_t ip, int comp)
{
  /*
  Get the prediction according to a specific component
  */
  if (comp == 0) // Check if component is the bimodal table
  {
    Index index = get_bimodal_index(ip); // Get bimodal index
    return bimodal_table[index] >= (1 << (TAGE_BASE_COUNTER_BITS - 1));
  } else {
    Index index = get_predictor_index(ip, comp); // Get component-specific index
    return predictor_table[comp - 1][index].ctr >= (1 << (TAGE_COUNTER_BITS - 1));
  }
}

uint8_t Tage::predict(uint64_t ip)
{
  pred_comp = get_match_below_n(ip, TAGE_NUM_COMPONENTS + 1); // Get the first predictor from the end which matches the PC
  alt_comp = get_match_below_n(ip, pred_comp);                // Get the first predictor below the provider which matches the PC

  // Store predictions for both components for use in the update step
  pred = get_prediction(ip, pred_comp);
  alt_pred = get_prediction(ip, alt_comp);

  if (pred_comp == 0)
    tage_pred = pred;
  else {
    Index index = get_predictor_index(ip, pred_comp);
    STRONG = abs(2 * predictor_table[pred_comp - 1][index].ctr + 1 - (1 << TAGE_COUNTER_BITS)) > 1;
    if (use_alt_on_na < 8 || STRONG) // Use provider component only if USE_ALT_ON_NA < 8 or the provider counter is strong
      tage_pred = pred;
    else
      tage_pred = alt_pred;
  }
  return tage_pred;
}

void Tage::ctr_update(uint8_t& ctr, int cond, int low, int high)
{
  /*
  Function to update bounded counters according to some condition
  */
  if (cond && ctr < high)
    ctr++;
  else if (!cond && ctr > low)
    ctr--;
}

void Tage::update(uint64_t ip, uint8_t taken)
{
  /*
  function to update the state (member variables) of the tage class
  */
  if (pred_comp > 0) // the predictor component is not the bimodal table
  {
    struct tage_predictor_table_entry* entry = &predictor_table[pred_comp - 1][get_predictor_index(ip, pred_comp)];
    uint8_t useful = entry->useful;

    if (!STRONG) {
      if (pred != alt_pred)
        ctr_update(use_alt_on_na, !(pred == taken), 0, 15);
    }

    if (alt_comp > 0) // alternate component is not the bimodal table
    {
      struct tage_predictor_table_entry* alt_entry = &predictor_table[alt_comp - 1][get_predictor_index(ip, alt_comp)];
      if (useful == 0)
        ctr_update(alt_entry->ctr, taken, 0, ((1 << TAGE_COUNTER_BITS) - 1)); // update ctr for alternate predictor if useful for predictor is 0
    } else {
      Index index = get_bimodal_index(ip);
      if (useful == 0)
        ctr_update(bimodal_table[index], taken, 0, ((1 << TAGE_BASE_COUNTER_BITS) - 1)); // update ctr for alternate predictor if useful for predictor is 0
    }

    // update u
    if (pred != alt_pred) {
      if (pred == taken) {
        if (entry->useful < ((1 << TAGE_USEFUL_BITS) - 1))
          entry->useful++; // if prediction from preditor component was correct
      } else {
        if (use_alt_on_na < 8) {
          if (entry->useful > 0)
            entry->useful--; // if prediction from altpred component was correct
        }
      }
    }

    ctr_update(entry->ctr, taken, 0, ((1 << TAGE_COUNTER_BITS) - 1)); // update ctr for predictor component
  } else {
    Index index = get_bimodal_index(ip);
    ctr_update(bimodal_table[index], taken, 0, ((1 << TAGE_BASE_COUNTER_BITS) - 1)); // update ctr for predictor if predictor is bimodal
  }

  // allocate tagged entries on misprediction
  if (tage_pred != taken) {
    long rand = random();
    rand = rand & ((1 << (TAGE_NUM_COMPONENTS - pred_comp - 1)) - 1);
    int start_component = pred_comp + 1;

    // compute the start-component for search
    if (rand & 1) // 0.5 probability
    {
      start_component++;
      if (rand & 2) // 0.25 probability
        start_component++;
    }

    // Allocate atleast one entry if no free entry
    int isFree = 0;
    for (int i = pred_comp + 1; i <= TAGE_NUM_COMPONENTS; i++) {
      struct tage_predictor_table_entry* entry_new = &predictor_table[i - 1][get_predictor_index(ip, i)];
      if (entry_new->useful == 0)
        isFree = 1;
    }
    if (!isFree && start_component <= TAGE_NUM_COMPONENTS)
      predictor_table[start_component - 1][get_predictor_index(ip, start_component)].useful = 0;

    // search for entry to steal from the start-component till end
    for (int i = start_component; i <= TAGE_NUM_COMPONENTS; i++) {
      struct tage_predictor_table_entry* entry_new = &predictor_table[i - 1][get_predictor_index(ip, i)];
      if (entry_new->useful == 0) {
        entry_new->tag = get_tag(ip, i);
        entry_new->ctr = (1 << (TAGE_COUNTER_BITS - 1));
        break;
      }
    }
  }

  // update global history
  for (int i = TAGE_GLOBAL_HISTORY_BUFFER_LENGTH - 1; i > 0; i--)
    global_history[i] = global_history[i - 1];
  global_history[0] = taken;

  // update path history
  for (int i = TAGE_PATH_HISTORY_BUFFER_LENGTH - 1; i > 0; i--)
    path_history[i] = path_history[i - 1];
  path_history[0] = ip & 1;

  // graceful resetting of useful counter
  num_branches++;
  if (num_branches % TAGE_RESET_USEFUL_INTERVAL == 0) {
    num_branches = 0;
    for (int i = 0; i < TAGE_NUM_COMPONENTS; i++) {
      for (int j = 0; j < (1 << TAGE_INDEX_BITS[i]); j++)
        predictor_table[i][j].useful >>= 1;
    }
  }
}

Index Tage::get_bimodal_index(uint64_t ip)
{
  /*
  Return index of the PC in the bimodal table using the last K bits
  */
  return ip & (TAGE_BIMODAL_TABLE_SIZE - 1);
}

Path Tage::get_path_history_hash(int component)
{
  /*
  Use a hash-function to compress the path history
  */
  Path A = 0;

  int size = component_history_lengths[component - 1] > 16 ? 16 : component_history_lengths[component - 1]; // Size of hash output
  for (int i = TAGE_PATH_HISTORY_BUFFER_LENGTH - 1; i >= 0; i--) {
    A = (A << 1) | path_history[i]; // Build the bit vector a using the path history array
  }

  A = A & ((1 << size) - 1);
  Path A1;
  Path A2;
  A1 = A & ((1 << TAGE_INDEX_BITS[component - 1]) - 1); // Get last M bits of A
  A2 = A >> TAGE_INDEX_BITS[component - 1];             // Get second last M bits of A

  // Use the hashing from the CBP-4 L-Tage submission
  A2 = ((A2 << component) & ((1 << TAGE_INDEX_BITS[component - 1]) - 1)) + (A2 >> abs(TAGE_INDEX_BITS[component - 1] - component));
  A = A1 ^ A2;
  A = ((A << component) & ((1 << TAGE_INDEX_BITS[component - 1]) - 1)) + (A >> abs(TAGE_INDEX_BITS[component - 1] - component));

  return (A);
}

History Tage::get_compressed_global_history(int inSize, int outSize)
{
  /*
  Compress global history of last 'inSize' branches into 'outSize' by wrapping the history
  */
  History compressed_history = 0; // Stores final compressed history
  History temporary_history = 0;  // Temorarily stores some bits of history
  int compressed_history_length = outSize;
  for (int i = 0; i < inSize; i++) {
    if (i % compressed_history_length == 0) {
      compressed_history ^= temporary_history; // XOR current segment into the compressed history
      temporary_history = 0;
    }
    temporary_history = (temporary_history << 1) | global_history[i]; // Build history bit vector
  }
  compressed_history ^= temporary_history;
  return compressed_history;
}

Index Tage::get_predictor_index(uint64_t ip, int component)
{
  /*
  Get index of PC in a particular predictor component
  */
  Path path_history_hash = get_path_history_hash(component); // Hash of path history

  // Hash of global history
  History global_history_hash = get_compressed_global_history(component_history_lengths[component - 1], TAGE_INDEX_BITS[component - 1]);

  return (global_history_hash ^ ip ^ (ip >> (abs(TAGE_INDEX_BITS[component - 1] - component) + 1)) ^ path_history_hash)
         & ((1 << TAGE_INDEX_BITS[component - 1]) - 1);
}

Tag Tage::get_tag(uint64_t ip, int component)
{
  /*
  Get tag of a PC for a particular predictor component
  */
  History global_history_hash = get_compressed_global_history(component_history_lengths[component - 1], TAGE_TAG_BITS[component - 1]);
  global_history_hash ^= get_compressed_global_history(component_history_lengths[component - 1], TAGE_TAG_BITS[component - 1] - 1);

  return (global_history_hash ^ ip) & ((1 << TAGE_TAG_BITS[component - 1]) - 1);
}

int Tage::get_match_below_n(uint64_t ip, int component)
{
  /*
  Get component number of first predictor which has an entry for the IP below a specfic component number
  */
  for (int i = component - 1; i >= 1; i--) {
    Index index = get_predictor_index(ip, i);
    Tag tag = get_tag(ip, i);

    if (predictor_table[i - 1][index].tag == tag) // Compare tags at a specific index
    {
      return i;
    }
  }

  return 0; // Default to bimodal in case no match found
}

Tage::Tage() {}

Tage::~Tage() {}