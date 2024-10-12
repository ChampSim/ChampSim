#include <map>
#include <bitset>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"



// The two level local branch predictor is similar to the local history branch predictor, except that it correlates branches with 
// Histories in the history table, the histories in the history table (which are bitsets of length 14) are then used to index the 
// Bimodal table 
namespace
{

// 4*16384
// 14*
constexpr std::size_t BIMODAL_TABLE_SIZE = 16384;
constexpr std::size_t COUNTER_BITS = 4;
constexpr std::size_t HISTORIES_TABLE_DEPTH = 14;

// std::bitset<HISTORIES_TABLE_DEPTH> History {"0000000000000"}; 


std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, BIMODAL_TABLE_SIZE>> bimodal_table; // Bimodal table that the history table will be indexing 
std::map<O3_CPU*, std::array<std::bitset<HISTORIES_TABLE_DEPTH>,BIMODAL_TABLE_SIZE>> history_table; // Table that stores the various histories 

} // namespace

void O3_CPU::initialize_branch_predictor() {}

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
    auto hash = ip % ::BIMODAL_TABLE_SIZE; // Hash the ip
    auto bimodal_index = ::history_table[this][hash]; // Use the IP to index the history_table
    auto value = ::bimodal_table[this][bimodal_index.to_ullong()]; // Use the history from the history table to index the bimodal table
    // printf("%d", value.to_ullong());
    return value.value()>= 2; // bimodal value comparison 
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  // This code updates the bimodal table based on the most recent results 
    auto hash = ip % ::BIMODAL_TABLE_SIZE; // Hash the IP
    auto bimodal_index = ::history_table[this][hash]; // Use the ip hash to get the relavant history bitset from the history table. 
    ::bimodal_table[this][bimodal_index.to_ullong()] += taken ? 1 : -1; // If the value for that has was taken, then we increment our value in the table by 1, else subtract 1 

  // Update the history bitset (of the current ip) with the most recent data 
    ::history_table[this][hash] >>= 1; // remove the least recent history bit
    ::history_table[this][hash][HISTORIES_TABLE_DEPTH-1] = taken; // update the removed bit with whether or not we took the branch
  // printf("History:%b \n" , History);
}
