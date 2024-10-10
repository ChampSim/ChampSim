#include <map>
#include <bitset>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"

// Local branch history uses the recent history of taken or not taken paths to decided whether or not to take a path
// Unlike most other prediction methods, this scheme does not use the instruction pointer

namespace
{
  // 32*14k bytes 
constexpr std::size_t BIMODAL_TABLE_SIZE = 16384; // How many avaliable spots there are in the bimodal table 
constexpr std::size_t COUNTER_BITS = 2; // How many counter bits the bimodal table will use 
constexpr std::size_t HISTORY_LENGTH = 14; // How long the history register is 

std::bitset<HISTORY_LENGTH> Global_History {"00000000000000"};


std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, BIMODAL_TABLE_SIZE>> bimodal_table;
// std::map<O3_CPU*, std::array<std::bitset<HISTORIES_TABLE_DEPTH>,BIMODAL_TABLE_SIZE>> history_table;

} // namespace

void O3_CPU::initialize_branch_predictor() {}

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
    // Index the bimodal table with the history register 
    auto value = ::bimodal_table[this][Global_History.to_ullong()]; // 
    // printf("%d", value.to_ullong());
    return value.value()>= 2;
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  // This code updates the bimodal table based on the previous result
  ::bimodal_table[this][Global_History.to_ullong()] += taken ? 1 : -1; // If the value for that has was taken, then we increment our value in the table by 1, else subtract 1 
 
  // Update the history bitset with the most recent data 
  Global_History >>= 1; // Shift the history register to the left to remove the least recent data 
  Global_History[HISTORY_LENGTH-1] = taken; // insert the most recent data into the history register 
  // printf("History:%b \n" , History);
}
