#ifndef BRANCH_HASHED_PERCEPTRON_H
#define BRANCH_HASHED_PERCEPTRON_H

#include <array>
#include <cstdint>
#include <tuple>
#include <vector>

#include "modules.h"
#include "msl/bits.h"
#include "msl/fwcounter.h"

class hashed_perceptron : champsim::modules::branch_predictor
{
  constexpr static std::size_t NTABLES = 16;         // this many tables
  constexpr static int MAXHIST = 232;                // maximum history length
  constexpr static int MINHIST = 3;                  // minimum history length (for table 1; table 0 is biases)
  constexpr static std::size_t TABLE_SIZE = 1 << 12; // 12-bit indices for the tables
  constexpr static champsim::data::bits TABLE_INDEX_BITS{champsim::msl::lg2(TABLE_SIZE)};
  constexpr static std::size_t NGHIST_WORDS = MAXHIST / champsim::msl::lg2(TABLE_SIZE) + 1; // this many 12-bit words will be kept in the global history
  constexpr static int THRESHOLD = 1;

  constexpr static std::array<unsigned long, NTABLES> history_lengths = {0,  3,  4,  6,  8,  10,  14,  19,
                                                                         26, 36, 49, 67, 91, 125, 170, MAXHIST}; // geometric global history lengths

  // tables of 8-bit weights
  std::vector<std::array<champsim::msl::sfwcounter<8>, TABLE_SIZE>> tables = [] {
    decltype(tables) retval;
    std::generate_n(std::back_inserter(retval), std::size(history_lengths), [] { return typename decltype(retval)::value_type{}; });
    return retval;
  }(); // immediately invoked

  using ghist_type = std::array<unsigned long long, NGHIST_WORDS>;
  ghist_type ghist_words = {}; // words that store the global history

  int theta = 10;
  int tc = 0; // counter for threshold setting algorithm

  class indexer
  {
    ghist_type hist_masks;

  public:
    explicit indexer(unsigned long hist_len);
    std::size_t get_index(champsim::address pc, ghist_type ghist_words) const;
  };

  std::vector<indexer> indexers = [] {
    decltype(indexers) retval;
    std::transform(std::begin(history_lengths), std::end(history_lengths), std::back_inserter(retval), [](unsigned long len) { return indexer{len}; });
    return retval;
  }(); // immediately invoked

  struct perceptron_result {
    std::array<uint64_t, std::tuple_size_v<decltype(history_lengths)>> indices = {}; // remember the indices into the tables from prediction to update
    int yout = 0;                                                                    // perceptron sum
  };

  perceptron_result last_result{};

public:
  using branch_predictor::branch_predictor;
  bool predict_branch(champsim::address pc);
  void last_branch_result(champsim::address pc, champsim::address branch_target, bool taken, uint8_t branch_type);
  void adjust_threshold(bool correct);
};

#endif
