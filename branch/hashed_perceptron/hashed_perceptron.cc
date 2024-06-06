/*

This code implements a hashed perceptron branch predictor using geometric
history lengths and dynamic threshold setting.  It was written by Daniel
A. Jiménez in March 2019.


The original perceptron branch predictor is from Jiménez and Lin, "Dynamic
Branch Prediction with Perceptrons," HPCA 2001.

The idea of using multiple independently indexed tables of perceptron weights
is from Jiménez, "Fast Path-Based Neural Branch Prediction," MICRO 2003 and
later expanded in "Piecewise Linear Branch Prediction" from ISCA 2005.

The idea of using hashes of branch history to reduce the number of independent
tables is documented in three contemporaneous papers:

1. Seznec, "Revisiting the Perceptron Predictor," IRISA technical report, 2004.

2. Tarjan and Skadron, "Revisiting the Perceptron Predictor Again," UVA
technical report, 2004, expanded and published in ACM TACO 2005 as "Merging
path and gshare indexing in perceptron branch prediction"; introduces the term
"hashed perceptron."

3. Loh and Jiménez, "Reducing the Power and Complexity of Path-Based Neural
Branch Prediction," WCED 2005.

The ideas of using "geometric history lengths" i.e. hashing into tables with
histories of exponentially increasing length, as well as dynamically adjusting
the theta parameter, are from Seznec, "The O-GEHL Branch Predictor," from CBP
2004, expanded later as "Analysis of the O-GEometric History Length Branch
Predictor" in ISCA 2005.

This code uses these ideas, but prefers simplicity over absolute accuracy (I
wrote it in about an hour and later spent more time on this comment block than
I did on the code). These papers and subsequent papers by Jiménez and other
authors significantly improve the accuracy of perceptron-based predictors but
involve tricks and analysis beyond the needs of a tool like ChampSim that
targets cache optimizations. If you want accuracy at any cost, see the winners
of the latest branch prediction contest, CBP 2016 as of this writing, but
prepare to have your face melted off by the complexity of the code you find
there. If you are a student being asked to code a good branch predictor for
your computer architecture class, don't copy this code; there are much better
sources for you to plagiarize.

*/

#include "hashed_perceptron.h"

#include <numeric>

hashed_perceptron::indexer::indexer(unsigned long hist_len)
{
  const auto lg2_table_size = champsim::msl::lg2(TABLE_SIZE);
  const auto most_words = hist_len / lg2_table_size;               // most of the words are 12 bits long
  const champsim::data::bits last_word{hist_len % lg2_table_size}; // the last word is fewer than 12 bits

  std::fill_n(std::begin(hist_masks), most_words, champsim::msl::bitmask(champsim::data::bits{lg2_table_size}));
  hist_masks.at(most_words) = champsim::msl::bitmask(last_word);
}

std::size_t hashed_perceptron::indexer::get_index(champsim::address pc, ghist_type ghist) const
{
  constexpr auto slice_width{champsim::msl::lg2(TABLE_SIZE)}; // NOTE: GCC 9 gives internal compiler error if this has type champsim::data::bits

  // Mask the words in this table's history
  std::transform(std::begin(ghist), std::end(ghist), std::begin(hist_masks), std::begin(ghist), std::bit_and<>{});

  // XOR up to the next-to-the-last word
  // seed in the PC to spread accesses around (like gshare) XOR in the last word
  auto x = std::accumulate(std::begin(ghist), std::end(ghist), pc.slice_lower<champsim::data::bits{slice_width}>().to<uint64_t>(), std::bit_xor<>{});

  return x & champsim::msl::bitmask(TABLE_INDEX_BITS); // stay within the table size
}

bool hashed_perceptron::predict_branch(champsim::address pc)
{
  auto get_index = [pc, ghist_words = ghist_words](const auto& idxer) {
    return idxer.get_index(pc, ghist_words);
  };
  perceptron_result result;
  std::transform(std::cbegin(indexers), std::cend(indexers), std::begin(result.indices), get_index);

  // add the selected weights to the perceptron sum
  result.yout = std::inner_product(std::begin(tables), std::end(tables), std::begin(result.indices), 0, std::plus<>{},
                                   [](const auto& table, const auto& index) { return table[index].value(); });
  last_result = result;
  return result.yout >= THRESHOLD;
}

void hashed_perceptron::last_branch_result(champsim::address pc, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  // insert this branch outcome into the global history
  auto shift_ghist_words = [](auto next_word, auto last_word) {
    bool b = (last_word > champsim::msl::bitmask(TABLE_INDEX_BITS)); // get the MSB from the last word
    return next_word | b;
  };

  // Remove MSBs that were shifted into the next word
  auto mask_ghist_words = [](auto x) {
    return x & champsim::msl::bitmask(TABLE_INDEX_BITS);
  };

  std::transform(std::cbegin(ghist_words), std::cend(ghist_words), std::begin(ghist_words), [](auto x) { return x << 1; });
  std::adjacent_difference(std::cbegin(ghist_words), std::cend(ghist_words), std::begin(ghist_words), shift_ghist_words);
  std::transform(std::cbegin(ghist_words), std::cend(ghist_words), std::begin(ghist_words), mask_ghist_words);
  ghist_words[0] |= taken;

  // perceptron learning rule: train if misprediction or weak correct prediction
  bool prediction_correct = (taken == (last_result.yout >= THRESHOLD));
  bool prediction_weak = (std::abs(last_result.yout) < theta);
  if (!prediction_correct || prediction_weak) {
    for (std::size_t i = 0; i < std::size(tables); i++)
      tables[i][last_result.indices[i]] += taken ? 1 : -1; // update weights
    adjust_threshold(prediction_correct);
  }
}

// dynamic threshold setting from Seznec's O-GEHL paper
void hashed_perceptron::adjust_threshold(bool correct)
{
  constexpr int SPEED = 18; // speed for dynamic threshold setting
  if (!correct) {
    // increase theta after enough mispredictions
    tc++;
    if (tc >= SPEED) {
      theta++;
      tc = 0;
    }
  } else {
    // decrease theta after enough weak but correct predictions
    tc--;
    if (tc <= -SPEED) {
      theta--;
      tc = 0;
    }
  }
}
