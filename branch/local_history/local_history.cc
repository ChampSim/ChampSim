#include <map>
#include <bitset>
#include <iostream>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"

// Local branch history uses the recent history of taken or not taken paths to decided whether or not to take a path
// Unlike most other prediction methods, this scheme does not use the instruction pointer

namespace
{
  // 516*18k bytes 
constexpr std::size_t BIMODAL_TABLE_SIZE = 2197152; // How many avaliable spots there are in the bimodal table 
constexpr std::size_t HISTORY_TABLE_SIZE = 262144;
constexpr std::size_t INDEX_SIZE = 20; 
constexpr std::size_t HISTORY_TABLE_DEPTH = 8;
constexpr std::size_t COUNTER_BITS = 3;          // How many counter bits the bimodal table will use 


std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, BIMODAL_TABLE_SIZE>> bimodal_table;
std::map<O3_CPU*, std::array<std::bitset<HISTORY_TABLE_DEPTH>,HISTORY_TABLE_SIZE>> history_table;

// std::size_t return_2_pow(std::size_t max)
// {
//   std::size_t count = 0;
//    std::size_t int2 = 2; 
//   while(int2 < max){
//     int2 = int2 << 1;
//     count++;
//   }
//   return count;
// }
} // namespace


void O3_CPU::initialize_branch_predictor() {}

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
std::bitset<64> ip_bitset(ip);
std::bitset<INDEX_SIZE> index;
auto hash = ip % ::HISTORY_TABLE_SIZE;
std::bitset<INDEX_SIZE> history;
for (std::size_t i = 0; i < HISTORY_TABLE_DEPTH; i++)
    history[i] = ::history_table[this][hash][i];
for (std::size_t i = 0; i < INDEX_SIZE - HISTORY_TABLE_DEPTH; i++)
    index[i] = ip_bitset[i];
index <<= HISTORY_TABLE_DEPTH;
index |= history;
auto value = ::bimodal_table[this][index.to_ullong()];

return value.value() >= (value.maximum / 2);
  
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{

std::bitset<64> ip_bitset(ip);
std::bitset<INDEX_SIZE> index;
auto hash = ip % ::HISTORY_TABLE_SIZE;

std::bitset<INDEX_SIZE> history;
for (std::size_t i = 0; i < HISTORY_TABLE_DEPTH; i++)
    history[i] = ::history_table[this][hash][i];
for (std::size_t i = 0; i < INDEX_SIZE - HISTORY_TABLE_DEPTH; i++)
    index[i] = ip_bitset[i];
index <<= HISTORY_TABLE_DEPTH;
index |= history;

::history_table[this][hash] <<= 1;
::history_table[this][hash][0] = taken;

::bimodal_table[this][index.to_ullong()] += taken ? 1 : -1;;




}
