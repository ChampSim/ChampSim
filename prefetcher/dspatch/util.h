#ifndef UTIL_H
#define UTIL_H

#include <cstdint>
#include <string>
#include <sstream>
#include <vector>

uint32_t folded_xor(uint64_t value, uint32_t num_folds);
uint32_t jenkins(uint32_t key);

template <class T> std::string array_to_string(std::vector<T> array, bool hex = false, uint32_t size = 0)
{
    std::stringstream ss;
    if (size == 0) size = array.size();
    for(uint32_t index = 0; index < size; ++index)
    {
    	if(hex)
    	{
    		ss << std::hex << array[index] << std::dec;
    	}
    	else
    	{
    		ss << array[index];
    	}
        ss << ",";
    }
    return ss.str();
}

#endif /* UTIL_H */
