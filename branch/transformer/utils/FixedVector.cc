#include <vector>
#include <stdexcept>

class FixedSizeVector {
private:
    std::vector<int> vec;

public:
    explicit FixedSizeVector(size_t size) : vec(size) {}

    int& operator[](size_t index) {
        if (index >= vec.size()) throw std::out_of_range("Index out of bounds");
        return vec[index];
    }

    const int& operator[](size_t index) const {
        if (index >= vec.size()) throw std::out_of_range("Index out of bounds");
        return vec[index];
    }

    size_t size() const { return vec.size(); }

    // Disable operations that change size
    void push_back(const int&) = delete;
    void emplace_back(int) = delete;
    void resize(size_t) = delete;
};
