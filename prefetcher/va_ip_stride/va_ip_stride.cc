#include "cache.h"

#define L2C_IP_STRIDE_TRACKER_COUNT 128
#define L2C_IP_STRIDE_PREFETCH_DEGREE 4

struct l2c_ip_stride_tracker_t
{
  uint64_t ip;
  uint64_t last_vcl;
  int64_t last_stride;
  uint64_t lru;
};

l2c_ip_stride_tracker_t l2c_ip_trackers[L2C_IP_STRIDE_TRACKER_COUNT];
int l2c_ip_tracker_lru_counter;

int l2c_prefetch_va_or_pa(CACHE *cache, uint64_t ip, uint64_t v_base_addr, uint64_t v_pf_addr, uint64_t p_base_addr, int pf_fill_level, int pf_metadata)
{
  if((v_base_addr>>LOG2_BLOCK_SIZE) == (v_pf_addr>>LOG2_BLOCK_SIZE))
    {
      // attempting to prefetch the same cache line!
      return -1;
    }

  if((v_base_addr>>LOG2_PAGE_SIZE) != (v_pf_addr>>LOG2_PAGE_SIZE))
    {
      // prefetching to different pages, so use virtual address prefetching
      if(cache->va_prefetch_line(ip, v_pf_addr, pf_fill_level, pf_metadata))
	{
	  return 1;
	}
      else
	{
	  return 0;
	}
    }

   if((v_base_addr>>LOG2_PAGE_SIZE) == (v_pf_addr>>LOG2_PAGE_SIZE))
     {
       // prefetching in the same page, so use physical address prefetching
       int64_t pf_offset = 0;
       if(v_pf_addr > v_base_addr)
	 {
	   pf_offset = v_pf_addr - v_base_addr;
	 }
       else
	 {
	   pf_offset = v_base_addr - v_pf_addr;
	   pf_offset *= -1;
	 }
       if(cache->prefetch_line(ip, p_base_addr, p_base_addr+pf_offset, pf_fill_level, pf_metadata))
	 {
	   return 2;
	 }
       else
	 {
	   return 0;
	 }
     }
  
  return 0;
}

void CACHE::l2c_prefetcher_initialize() 
{
  cout << "CPU " << cpu << " Virtual Address Space Instruction Pointer Stride L2C Prefetcher" << endl;
  
  for(int i=0; i<L2C_IP_STRIDE_TRACKER_COUNT; i++)
    {
      l2c_ip_trackers[i].ip = 0;
      l2c_ip_trackers[i].last_vcl = 0;
      l2c_ip_trackers[i].last_stride = 0;
      l2c_ip_trackers[i].lru = 0;
    }
  l2c_ip_tracker_lru_counter = 0;
}

uint32_t CACHE::l2c_prefetcher_operate(uint64_t v_addr, uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
  if(type == PREFETCH)
    {
      return metadata_in;
    }

  if(v_addr == 0)
    {
      return metadata_in;
    }

  uint64_t v_cl = v_addr>>LOG2_BLOCK_SIZE;
  
  // see if we're tracking this
  int ip_index = -1;
  for(int i=0; i<L2C_IP_STRIDE_TRACKER_COUNT; i++)
    {
      if(l2c_ip_trackers[i].ip == ip)
	{
	  ip_index = i;
	  break;
	}
    }

  if(ip_index == -1)
    {
      // find the LRU tracker entry
      int lru_index = 0;
      uint64_t lru_value = l2c_ip_trackers[lru_index].lru;
      for(int i=0; i<L2C_IP_STRIDE_TRACKER_COUNT; i++)
	{
	  if(l2c_ip_trackers[i].lru < lru_value)
	    {
	      lru_index = i;
	      lru_value = l2c_ip_trackers[lru_index].lru;
	    }
	}

      // and replace it
      l2c_ip_trackers[lru_index].ip = ip;
      l2c_ip_trackers[lru_index].last_vcl = v_cl;
      l2c_ip_trackers[lru_index].last_stride = 0;
      l2c_ip_trackers[lru_index].lru = l2c_ip_tracker_lru_counter;
      l2c_ip_tracker_lru_counter++;

      return metadata_in;
    }

  int64_t current_stride = 0;
  if(v_addr >= l2c_ip_trackers[ip_index].last_vcl)
    {
      current_stride = v_cl - l2c_ip_trackers[ip_index].last_vcl;
    }
  else
    {
      current_stride = l2c_ip_trackers[ip_index].last_vcl - v_cl;
      current_stride *= -1;
    }

  if(current_stride == 0)
    {
      return metadata_in;
    }

  if(current_stride == l2c_ip_trackers[ip_index].last_stride)
    {
      for(int i=0; i<L2C_IP_STRIDE_PREFETCH_DEGREE; i++)
	{
	  if (get_occupancy(0,0) < (get_size(0,0)>>1))
	    {
	      l2c_prefetch_va_or_pa(this, ip, v_addr, (v_cl+((1+i)*current_stride))<<LOG2_BLOCK_SIZE, addr, FILL_L2, 0);
	    }
	  else
	    {
	      l2c_prefetch_va_or_pa(this, ip, v_addr, (v_cl+((1+i)*current_stride))<<LOG2_BLOCK_SIZE, addr, FILL_LLC, 0);	      
	    }
	}
    }

  l2c_ip_trackers[ip_index].last_vcl = v_cl;
  l2c_ip_trackers[ip_index].last_stride = current_stride;

  l2c_ip_trackers[ip_index].lru = l2c_ip_tracker_lru_counter;
  l2c_ip_tracker_lru_counter++;

  return metadata_in;
}

uint32_t CACHE::l2c_prefetcher_cache_fill(uint64_t v_addr, uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::l2c_prefetcher_final_stats()
{

}
