
#ifndef STAT_METHODS_H
#define STAT_METHODS_H

#include <cstdint>
#include "msl/fwcounter.h"
#include <cassert>
#include "msl/bits.h"

template <typename T>
struct category_projector {
  auto operator()(const T& t) const { return t; }
};

namespace champsim::msl {

    //categorizer splits a given input space into categories based on the category projection and sample rate. It is used to determine which samples belong to which category for set dueling.
    template <typename T, typename CatProj = category_projector<T>>
    class categorizer {
        private:
            std::size_t sample_rate;
            CatProj cat_projection;
        public:
            std::size_t get_sample_rate() const { return sample_rate; }
            std::size_t get_sample_category(const T& candidate) const {
                auto sp = cat_projection(candidate);
                champsim::data::bits shift{lg2(sample_rate)};
                auto mask = bitmask(shift);

                auto low_slice = sp & mask;
                auto high_slice = (sp >> lg2(sample_rate)) & mask;
                return (sample_rate + low_slice - high_slice) & mask;
            }
            categorizer(std::size_t sample_rate_, CatProj cat_projection_) : sample_rate(sample_rate_), cat_projection(cat_projection_){}
            categorizer(std::size_t sample_rate_) : categorizer(sample_rate_, CatProj{}) {}
    };

    //dscounter is a dueling set counter that uses a categorizer to determine which category a given sample belongs to, and a fwcounter to keep track of the "score" of the categories. It provides methods to update the counter based on whether a sample is considered "good" or "bad" for the category it belongs to.
    template <typename T, std::size_t COUNTER_WIDTH, typename CatProj = category_projector<T>>
    class dscounter {
    private:
        categorizer<T, CatProj> cat_sampler;
        fwcounter<COUNTER_WIDTH> counter;
    public:
        std::size_t get_sample_rate() const { return cat_sampler.get_sample_rate(); }
    
        bool decide(const T& candidate) {
            auto category = cat_sampler.get_sample_category(candidate);
            if(category == 0)
                return true;
            else if(category == 1)
                return false;
            return counter >= (counter.maximum / 2);
        }
        
        void update_good(const T& candidate) {
            auto category = cat_sampler.get_sample_category(candidate);
            if (category == 0) {
                counter += 1;
            } else if(category == 1) {
                counter -= 1;
            }
        }
        void update_bad(const T& candidate) {
            auto category = cat_sampler.get_sample_category(candidate);
            if (category == 0) {
                counter -= 1;
            } else if(category == 1) {
                counter += 1;
            }
        }
        dscounter(std::size_t sample_rate_, CatProj cat_projection_) : cat_sampler(sample_rate_, cat_projection_), counter(0) {}
        dscounter(std::size_t sample_rate_) : dscounter(sample_rate_, CatProj{}) {}
    };

    // get_sample_rate determines the sampling rate for set dueling based on the number of sets in the cache. It returns a sample rate that is a power of 2, and is chosen to ensure that there are enough samples for each category in the categorizer.
    static inline std::size_t get_sample_rate(long num) {
        std::size_t set_sample_rate = 32; // 1 in 32
        if(num < 1024 && num >= 256) { // 1 in 16
            set_sample_rate = 16;
        } else if(num >= 64) { // 1 in 8
            set_sample_rate = 8;
        } else if(num >= 8) { // 1 in 4
            set_sample_rate = 4;
        } else {
            assert(false); // Not enough sets to sample for set dueling
        }
        return set_sample_rate;
    }
    // get_num_samples calculates the number of samples that will be taken for set dueling based on the total number of sets and the sample rate. It asserts that the number of sets is divisible by the sample rate, and returns the number of samples as the total number of sets divided by the sample rate.
    static inline std::size_t get_num_samples(long num) {
        assert(num % get_sample_rate(num) == 0);
        return num / get_sample_rate(num);
    }
}

#endif