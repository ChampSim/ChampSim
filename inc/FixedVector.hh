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

public:
  // Default (needed for nested FixedVectors or delayed init)
  FixedVector() : vec(0) {}

  // Fixed size constructor
  explicit FixedVector(size_t size) : vec(size) {};
  explicit FixedVector(size_t size, T default_value = T{}) : vec(size, default_value) {};

  T& operator[](std::size_t idx)
  {
    if (idx >= vec.size())
      throw std::out_of_range("Index out of bounds. Vec size: " + std::to_string(vec.size()));
    return vec[idx];
  }

  const T& operator[](size_t idx) const
  {
    if (idx >= vec.size())
      throw std::out_of_range("Index out of bounds");
    return vec[idx];
  }

  void push(FixedVector<float> new_val)
  {
    /*
      Places new value at the front, moves all other elements
      one position to the right.
    */
    if (vec.size() == 0)
      throw std::runtime_error("Cannot push on an empty FixedVector");
    vec.erase(vec.begin());
    vec.push_back(new_val);
  }

  size_t size(){
    return vec.size();
  }

  // Disable operations that change size
  void push_back(const int&) = delete;
  void emplace_back(int) = delete;
  void resize(size_t) = delete;
};

#endif