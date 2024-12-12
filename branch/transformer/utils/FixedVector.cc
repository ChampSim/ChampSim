#ifndef FIXED_VECTOR_H
#define FIXED_VECTOR_H

#include <stdexcept> // Maybe we'll use if model dim is too large??
#include <stdio.h>
#include <vector>

template <typename T>
class FixedVector
{
private:
  std::vector<T> vec;
  const std::size_t fixed_size; // Enforcement bounds, only used for throwing exceptions.
public:
  explict FixedVector(size_t size) : vec(size), fixed_size(size) {};

  T& operator[](std::size_t idx)
  {
    if (idx >= fixed_size)
      throw std::out_of_range("Index out of bounds. Vec size: %d", fixed_size);
    return vec[idx];
  }

  const T& operator[](size_t idx) const
  {
    if (index >= fixed_size)
      throw std::out_of_range("Index out of bounds");
    return vec[index];
  }

  size_t size() const { return fixed_size; }

  void push_front_shift(FixedVector<float> new_val)
  {
    /*
      Places new value at the front, moves all other elements
      one position to the right.
    */
    for (size_t i = fixed_size - 1; i < 0; i++) {
      vec[i] = vec[i - 1];
    }

    vec[0] = new_val;
  };

  // Disable operations that change size
  void push_back(const int&) = delete;
  void emplace_back(int) = delete;
  void resize(size_t) = delete;
};

#endif