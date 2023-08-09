#ifndef BRANCH_HASHED_PERCEPTRON_H
#define BRANCH_HASHED_PERCEPTRON_H

#include <array>
#include <cstdint>
#include <tuple>

#include "modules.h"
#include "msl/bits.h"
#include "msl/fwcounter.h"

struct hashed_perceptron : champsim::modules::branch_predictor
{
  constexpr static std::size_t NTABLES = 16; // this many tables
  constexpr static int MAXHIST = 232; // maximum history length
  constexpr static int MINHIST = 3; // minimum history length (for table 1; table 0 is biases)
  constexpr static std::size_t TABLE_SIZE = 1 << 12; // 12-bit indices for the tables
  constexpr static std::size_t NGHIST_WORDS = MAXHIST / champsim::msl::lg2(TABLE_SIZE) + 1; // this many 12-bit words will be kept in the global history
  constexpr static int THRESHOLD = 1;

  constexpr static std::array<unsigned long, NTABLES> history_lengths = {0, 3, 4, 6, 8, 10, 14, 19, 26, 36, 49, 67, 91, 125, 170, MAXHIST}; // geometric global history lengths

  std::array<std::array<champsim::msl::sfwcounter<8>, TABLE_SIZE>, NTABLES> tables = {}; // tables of 8-bit weights

  std::array<unsigned int, NGHIST_WORDS> ghist_words = {}; // words that store the global history

  int theta = 10;
  int tc = 0; // counter for threshold setting algorithm

  struct perceptron_result
  {
    std::array<uint64_t, std::tuple_size_v<decltype(tables)>> indices = {}; // remember the indices into the tables from prediction to update
    int yout = 0; // perceptron sum
  };

  perceptron_result last_result{};

  using branch_predictor::branch_predictor;
  bool predict_branch(uint64_t pc);
  void last_branch_result(uint64_t pc, uint64_t branch_target, uint8_t taken, uint8_t branch_type);
  void adjust_threshold(bool correct);
};

#endif
