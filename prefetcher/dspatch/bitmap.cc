#include "bitmap.h"
#include <assert.h>
#include <sstream>

std::string BitmapHelper::to_string(Bitmap bmp, uint32_t size)
{
	// return bmp.to_string<char,std::string::traits_type,std::string::allocator_type>();
	std::stringstream ss;
	for(int32_t bit = size-1; bit >= 0; --bit)
	{
		ss << bmp[bit];
	}
	return ss.str();
}

uint32_t BitmapHelper::count_bits_set(Bitmap bmp, uint32_t size)
{
	// return static_cast<uint32_t>(bmp.count());
	uint32_t count = 0;
	for(uint32_t index = 0; index < size; ++index)
	{
		if(bmp[index]) count++;
	}
	return count;
}

uint32_t BitmapHelper::count_bits_same(Bitmap bmp1, Bitmap bmp2, uint32_t size)
{
	uint32_t count_same = 0;
	for(uint32_t index = 0; index < size; ++index)
	{
		if(bmp1[index] && bmp1[index] == bmp2[index])
		{
			count_same++;
		}
	}
	return count_same;
}

uint32_t BitmapHelper::count_bits_diff(Bitmap bmp1, Bitmap bmp2, uint32_t size)
{
	uint32_t count_diff = 0;
	for(uint32_t index = 0; index < size; ++index)
	{
		if(bmp1[index] && !bmp2[index])
		{
			count_diff++;
		}
	}
	return count_diff;
}

uint64_t BitmapHelper::value(Bitmap bmp, uint32_t size)
{
	return bmp.to_ullong();
}

Bitmap BitmapHelper::rotate_left(Bitmap bmp, uint32_t amount, uint32_t size)
{
	Bitmap result;
	for(uint32_t index = 0; index < (size - amount); ++index)
	{
		result[index+amount] = bmp[index];
	}
	for(uint32_t index = 0; index < amount; ++index)
	{
		result[index] = bmp[index+size-amount];
	}
	return result;
}

Bitmap BitmapHelper::rotate_right(Bitmap bmp, uint32_t amount, uint32_t size)
{
	Bitmap result;
	for(uint32_t index = 0; index < size - amount; ++index)
	{
		result[index] = bmp[index+amount];
	}
	for(uint32_t index = 0; index < amount; ++index)
	{
		result[size-amount+index] = bmp[index];
	}
	return result;

}

Bitmap BitmapHelper::compress(Bitmap bmp, uint32_t granularity, uint32_t size)
{
	assert(size % granularity == 0);
	uint32_t index = 0;
	Bitmap result;
	uint32_t ptr = 0;

	while(index < size)
	{
		bool res = false;
		uint32_t gran = 0;
		for(gran = 0; gran < granularity; ++gran)
		{
			assert(index + gran < size);
			res = res | bmp[index+gran];
		}
		result[ptr] = res;
		ptr++;
		index = index + gran;
	}
	return result;
}

Bitmap BitmapHelper::decompress(Bitmap bmp, uint32_t granularity, uint32_t size)
{
	Bitmap result;
	result.reset();
	assert(size*granularity <= BITMAP_MAX_SIZE);
	for(uint32_t index = 0; index < size; ++index)
	{
		if(bmp[index])
		{
			uint32_t ptr = index * granularity;
			for(uint32_t count = 0; count < granularity; ++count)
			{
				result[ptr+count] = true;
			}
		}
	}
	return result;
}

Bitmap BitmapHelper::bitwise_or(Bitmap bmp1, Bitmap bmp2, uint32_t size)
{
	Bitmap result;
	for(uint32_t index = 0; index < size; ++index)
	{
		if(bmp1[index] || bmp2[index])
		{
			result[index] = true;
		}
	}
	return result;
}

Bitmap BitmapHelper::bitwise_and(Bitmap bmp1, Bitmap bmp2, uint32_t size)
{
	Bitmap result;
	for(uint32_t index = 0; index < size; ++index)
	{
		if(bmp1[index] && bmp2[index])
		{
			result[index] = true;
		}
	}
	return result;
}