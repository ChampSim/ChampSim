#ifndef DELAY_QUEUE_H
#define DELAY_QUEUE_H

#include <algorithm>
#include <iostream>
#include <iterator>
#include <type_traits>

#include "circular_buffer.h"

namespace champsim {

    /***
     * A fixed-size queue that releases its members only after a delay.
     *
     * This class forwards most of its functionality on to a champsim::circular_buffer<>, but
     * introduces some boilerplate code to wrap members in a type that counts down the cycles
     * until the member is ready to be released.
     *
     * The `end_ready()` member function (and related functions) are provided to permit iteration
     * over only ready members.
     *
     * No checking is done when members are popped. A recommended way to operate on and to pop multiple members is:
     *
     *     delay_queue<int> dq(10, 1);
     *     std::size_t bandwidth = 4;
     *     auto end = std::min(dq.end_ready(), std::next(dq.begin(), bandwidth));
     *     for (auto it = dq.begin(); it != end; it = dq.begin()) {
     *         perform_task(*it);
     *         dq.pop_front();
     *     }
     *
     * If no bandwidth constraints are needed, the terminal condition can simply be `it != dq.end_ready()`.
     ***/
    template <typename T>
    class delay_queue {
        private:
            template <typename U>
            using buffer_t = circular_buffer<U>;

        public:
            delay_queue(std::size_t size, unsigned latency) : sz(size), _buf(size),_delays(size), _latency(latency) {}

            /***
             * These types provided for compatibility with standard containers.
             ***/
            using value_type             = T;
            using size_type              = std::size_t;
            using difference_type        = std::ptrdiff_t;
            using reference              = value_type&;
            using const_reference        = const reference;
            using pointer                = value_type*;
            using const_pointer          = const pointer;
            using iterator               = typename buffer_t<value_type>::iterator;
            using const_iterator         = typename buffer_t<value_type>::const_iterator;
            using reverse_iterator       = std::reverse_iterator<iterator>;
            using const_reverse_iterator = std::reverse_iterator<const_iterator>;

            constexpr size_type size() const noexcept     { return sz; }
            size_type occupancy() const noexcept          { return _buf.occupancy(); };
            //size_type occupancy() const noexcept          { return _buf.size(); };
            bool empty() const noexcept                   { return occupancy() == 0; }
            bool full()  const noexcept                   { return _buf.full(); }
            bool has_ready() const noexcept               { return _delays.front() <= 0; }
            constexpr size_type max_size() const noexcept { return _buf.max_size(); }

            /***
             * Note: there is no guarantee that either the front or back element is ready.
             ***/
            reference front()             { return _buf.front(); }
            reference back()              { return _buf.back(); }
            const_reference front() const { return _buf.front(); }
            const_reference back() const  { return _buf.back(); }

            /***
             * Note: there is no guarantee that either begin() or end() points to a ready member.
             * end_ready() will always point to the first non-ready member.
             ***/
            iterator begin() noexcept                  { return _buf.begin(); }
            iterator end() noexcept                    { return _buf.end(); }
            const_iterator begin() const noexcept      { return _buf.begin(); }
            const_iterator end() const noexcept        { return _buf.end(); }
            const_iterator cbegin() const noexcept     { return _buf.cbegin(); }
            const_iterator cend() const noexcept       { return _buf.cend(); }
            iterator end_ready() noexcept;
            const_iterator end_ready() const noexcept;
            const_iterator cend_ready() const noexcept;

            reverse_iterator rbegin() noexcept                  { return _buf.rbegin(); }
            reverse_iterator rend() noexcept                    { return _buf.rend(); }
            reverse_iterator rend_ready() noexcept              { return reverse_iterator(end_ready()); }
            const_reverse_iterator rbegin() const noexcept      { return _buf.rbegin(); }
            const_reverse_iterator rend() const noexcept        { return _buf.rend(); }
            const_reverse_iterator rend_ready() const noexcept  { return reverse_iterator(end_ready()); }
            const_reverse_iterator crbegin() const noexcept     { return _buf.crbegin(); }
            const_reverse_iterator crend() const noexcept       { return _buf.crend(); }
            const_reverse_iterator crend_ready() const noexcept { return reverse_iterator(end_ready()); }

            void clear() { _buf.clear(); }
            void push_back(const T& item) { _buf.push_back(item); _delays.push_back(_latency); }
            void push_back(const T&& item) { _buf.push_back(item); _delays.push_back(_latency); }
            void pop_front() { if (!has_ready()) _end_ready++; _buf.pop_front(); _delays.pop_front(); }

            /***
             * This function must be called once every cycle.
             ***/
            void operate() {
                for (auto &x : _delays)
                    --x; // The delay may go negative, this is permitted.

                auto delay_it = std::partition_point(_delays.begin(), _delays.end(), [](long long int x){ return x <= 0; });
                _end_ready = std::next(_buf.begin(), std::distance(_delays.begin(), delay_it));
                //std::cout << _end_ready - _buf.begin() << " ";
                //std::cout << _buf.end() - _end_ready << " ";
                //std::cout << _buf.end() - _buf.begin() << " ";
                std::cout << std::distance(_buf.begin(), _end_ready) << " ";
                std::cout << std::distance(_end_ready, _buf.end()) << " ";
                std::cout << std::distance(_buf.begin(), _buf.end()) << std::endl;
            }

        private:
            const size_type sz;
            buffer_t<value_type> _buf = {};
            buffer_t<long long int> _delays = {};
            const long long int _latency;
            iterator _end_ready = _buf.end();
    };

} // namespace champsim

template <typename T>
auto champsim::delay_queue<T>::end_ready() noexcept -> iterator {
    return _end_ready;
}

template <typename T>
auto champsim::delay_queue<T>::end_ready() const noexcept -> const_iterator {
    return _end_ready;
}

template <typename T>
auto champsim::delay_queue<T>::cend_ready() const noexcept -> const_iterator {
    return _end_ready;
}

#endif

