
#ifndef STAT_METHODS_H
#define STAT_METHODS_H

#include <cstdint>
#include "msl/fwcounter.h"
#include <cassert>
#include "extent.h"
#include "msl/bits.h"
#include "util/detect.h"
#include "util/span.h"
#include "util/type_traits.h"

template <typename T>
struct category_projector {
  auto operator()(const T& t) const { return t; }
};

namespace champsim::msl {

    template <typename T, typename CatProj = category_projector<T>>
    class categorizer {
        private:
            std::size_t sample_rate;
            CatProj cat_projection;
        public:
            std::size_t get_sample_rate() const { return sample_rate; }
            std::size_t get_sample_category(const T& candidate) const {
                auto sp = cat_projection(candidate);
                champsim::data::bits shift{champsim::lg2(sample_rate)};
                auto mask = champsim::bitmask(shift);

                auto low_slice = sp & mask;
                auto high_slice = (sp >> champsim::lg2(sample_rate)) & mask;
                return (sample_rate + low_slice - high_slice) & mask;
            }
            categorizer(std::size_t sample_rate_, CatProj cat_projection_) : sample_rate(sample_rate_), cat_projection(cat_projection_){}
            categorizer(std::size_t sample_rate_) : categorizer(sample_rate_, CatProj{}) {}
    };

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
        
        void update(const T& candidate) {
            auto category = cat_sampler.get_sample_category(candidate);
            if (category == 0) {
                counter += 1;
            } else if(category == 1) {
                counter -= 1;
            }
        }
        dscounter(std::size_t sample_rate_, CatProj cat_projection_) : cat_sampler(sample_rate_, cat_projection_), counter(0) {}
        dscounter(std::size_t sample_rate_) : dscounter(sample_rate_, CatProj{}) {}
    };

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
    static inline std::size_t get_num_samples(long num) {
        assert(num % get_sample_rate(num) == 0);
        return num / get_sample_rate(num);
    }
}

#endif