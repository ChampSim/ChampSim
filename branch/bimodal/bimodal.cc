#include "ooo_cpu.h"

#define BIMODAL_TABLE_SIZE 16384
#define BIMODAL_PRIME 16381
#define MAX_COUNTER 3
int bimodal_table[NUM_CPUS][BIMODAL_TABLE_SIZE];

void O3_CPU::initialize_branch_predictor()
{
    cout << "CPU " << cpu << " Bimodal branch predictor" << endl;

    for(int i = 0; i < BIMODAL_TABLE_SIZE; i++)
        bimodal_table[cpu][i] = 0;
}

uint64_t O3_CPU::predict_branch(uint64_t ip, uint8_t branch_type)
{
  uint8_t always_taken;
  uint64_t target = btb_prediction(ip, branch_type, always_taken);

  if(always_taken)
    {
      return target;
    }

  if(branch_type == BRANCH_CONDITIONAL)
    {
      uint32_t hash = ip % BIMODAL_PRIME;
      if((bimodal_table[cpu][hash] < ((MAX_COUNTER + 1)/2)))
	{
	  target = 0;
	}
    }

  return target;
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  update_btb(ip, branch_target, taken, branch_type);

  if(branch_type == BRANCH_CONDITIONAL)
    {
      uint32_t hash = ip % BIMODAL_PRIME;

      if (taken && (bimodal_table[cpu][hash] < MAX_COUNTER))
        bimodal_table[cpu][hash]++;
      else if ((taken == 0) && (bimodal_table[cpu][hash] > 0))
        bimodal_table[cpu][hash]--;
    }
}
