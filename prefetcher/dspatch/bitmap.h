#ifndef BITMAP_H
#define BITMAP_H

#include <bitset>
#include <string>
#include <cstdint>
#define BITMAP_MAX_SIZE 64

typedef std::bitset<BITMAP_MAX_SIZE> Bitmap;

class BitmapHelper
{
public:
	static uint64_t value(Bitmap bmp, uint32_t size = BITMAP_MAX_SIZE);
	static std::string to_string(Bitmap bmp, uint32_t size = BITMAP_MAX_SIZE);
	static uint32_t count_bits_set(Bitmap bmp, uint32_t size = BITMAP_MAX_SIZE);
	static uint32_t count_bits_same(Bitmap bmp1, Bitmap bmp2, uint32_t size = BITMAP_MAX_SIZE);
	static uint32_t count_bits_diff(Bitmap bmp1, Bitmap bmp2, uint32_t size = BITMAP_MAX_SIZE);
	static Bitmap rotate_left(Bitmap bmp, uint32_t amount, uint32_t size = BITMAP_MAX_SIZE);
	static Bitmap rotate_right(Bitmap bmp, uint32_t amount, uint32_t size = BITMAP_MAX_SIZE);
	static Bitmap compress(Bitmap bmp, uint32_t granularity, uint32_t size = BITMAP_MAX_SIZE);
	static Bitmap decompress(Bitmap bmp, uint32_t granularity, uint32_t size = BITMAP_MAX_SIZE);
	static Bitmap bitwise_or(Bitmap bmp1, Bitmap bmp2, uint32_t size = BITMAP_MAX_SIZE);
	static Bitmap bitwise_and(Bitmap bmp1, Bitmap bmp2, uint32_t size = BITMAP_MAX_SIZE);
};

#endif /* BITMAP_H */