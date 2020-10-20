#include "vmem.h"
#include "champsim.h"
#include "util.h"

VirtualMemory::VirtualMemory(uint32_t number_of_cpus, uint64_t capacity, uint64_t pg_size, uint32_t page_table_levels, uint64_t random_seed)
{
    num_cpus = number_of_cpus;
    pt_levels = page_table_levels;

    // calculate number of physical pages
    page_size = pg_size;
    if(capacity%page_size != 0)
    {
        std::cout << "VirtualMemory initialization error: memory capacity must be a multiple of page size!" << std::endl;
        exit(0);
    }
    num_ppages = capacity/page_size;
    std::cout << std::endl << "VirtualMemory physical capacity: " << capacity << " num_ppages: " << num_ppages << std::endl;

    if (pg_size != (1<<lg2(pg_size)) || pg_size < 1024)
    {
        std::cout<< "VirtualMemory initialization error: page size must be a power of 2, and at least 1024!" << std::endl;
        exit(0);
    }
    std::cout << "VirtualMemory page size: " << page_size << " log2_page_size: " << lg2(page_size) << std::endl;

    // initialize random number generator
    rand_state = random_seed+VMEM_RAND_FACTOR;
    if(rand_state == 0)
    {
        rand_state = VMEM_RAND_FACTOR<<1;
    }

    std::cout << "VirtualMemory initalizing ppage free list ... ";
    ppage_free_list.resize(num_ppages);
    // populate the free list
    for(uint64_t i=0; i<num_ppages; i++)
    {
        ppage_free_list[i] = i;
    }
    std::cout << "done" << std::endl << "VirtualMemory shuffling ppage free list ... ";
    // remove the reserve space from the free list
    uint64_t num_reserve_ppages = VMEM_RESERVE_CAPACITY < pg_size ? 1 : VMEM_RESERVE_CAPACITY/pg_size;

    for(int i=0; i<num_reserve_ppages; i++)
    {
        ppage_free_list.pop_front();
    }

    // then shuffle it
    uint64_t num_swap_ppages = num_ppages-num_reserve_ppages;
    for(uint64_t i=0; i<num_swap_ppages; i++)
    {
        // i is the swap source, swap target is random
        uint64_t swap_target = (vmem_rand()%(num_swap_ppages));

        uint64_t swap_temp = ppage_free_list[i];
        ppage_free_list[i] = ppage_free_list[swap_target];
        ppage_free_list[swap_target] = swap_temp;
    }
    std::cout << "done" << std::endl << std::endl;

    // initialize V to P page map tables
    vpage_to_ppage_map = new std::map<uint64_t, uint64_t>[num_cpus];

    // initialize per-process page tables
    page_table = new std::map<uint64_t, uint64_t>*[num_cpus];
    for(uint32_t i=0; i<num_cpus; i++)
    {
        page_table[i] = new std::map<uint64_t, uint64_t>[pt_levels];
    }
}

uint64_t VirtualMemory::get_next_free_ppage()
{
  if(ppage_free_list.empty())
    {
      // ran out of physical pages to allocate, so throw error and exit
      std::cout << "VirtualMemory error: ran out of physical pages to allocate!  Try a larger memory size." << std::endl;
      exit(0);
    }
  
  uint64_t free_ppage = ppage_free_list[0];
  ppage_free_list.pop_front();
  return free_ppage;
}

uint32_t VirtualMemory::get_paget_table_level_count()
{
  return pt_levels;
}

uint64_t VirtualMemory::va_to_pa(uint32_t cpu_num, uint64_t vaddr)
{
  uint64_t vpage = vaddr>>lg2(page_size);
  uint64_t voffset = vaddr&((1<<lg2(page_size))-1);

  if(vpage_to_ppage_map[cpu_num].find(vpage) == vpage_to_ppage_map[cpu_num].end())
    {
      // this vpage doesn't yet have a ppage mapping
      vpage_to_ppage_map[cpu_num][vpage] = get_next_free_ppage();
    }
  
  return (((vpage_to_ppage_map[cpu_num][vpage])<<lg2(page_size))+voffset);
}

uint64_t VirtualMemory::get_pte_pa(uint32_t cpu_num, uint64_t vaddr, uint32_t level)
{
  uint64_t vpage = vaddr>>lg2(page_size);
  uint64_t pte_offset = vpage&511;
  
  uint32_t shift_bits = 9 + (9*(pt_levels-1-level));
  uint64_t pt_lookup_tag = vpage>>shift_bits;

  if(page_table[cpu_num][level].find(pt_lookup_tag) == page_table[cpu_num][level].end())
    {
      // this PTE doesn't yet have a mapping
      page_table[cpu_num][level][pt_lookup_tag] = get_next_free_ppage();
    }
  
  return (((page_table[cpu_num][level][pt_lookup_tag])<<lg2(page_size))+(pte_offset*8));
}

uint64_t VirtualMemory::vmem_rand()
{
  rand_state += VMEM_RAND_FACTOR;

  rand_state ^= (rand_state<<13);
  rand_state ^= (rand_state>>7);
  rand_state ^= (rand_state<<11);
  rand_state ^= (rand_state>>1);
  
  return rand_state;
}
