#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <cassert>
#include <iterator>
#include <type_traits>
#include <vector>

namespace champsim
{

template<typename T>
class circular_buffer_iterator
{
    protected:
        using cbuf_type = T;
        using self_type = circular_buffer_iterator<T>;

        cbuf_type*                    buf;
    public:
        typename cbuf_type::size_type pos;

        using difference_type   = typename cbuf_type::difference_type;
        using value_type        = typename cbuf_type::value_type;
        using pointer           = value_type*;
        using reference         = value_type&;
        using iterator_category = std::random_access_iterator_tag;

        friend class circular_buffer_iterator<typename std::remove_const<T>::type>;
        friend class circular_buffer_iterator<typename std::add_const<T>::type>;

    public:
        circular_buffer_iterator() : buf(NULL), pos(0) {}
        circular_buffer_iterator(cbuf_type *buf, typename cbuf_type::size_type pos) : buf(buf), pos(pos) {}

        circular_buffer_iterator(const circular_buffer_iterator<typename std::remove_const<T>::type> &other) : buf(other.buf), pos(other.pos) {}

        reference operator*()  { return (*buf)[pos]; }
        pointer   operator->() { return &(operator*()); }

        self_type& operator+=(difference_type n);
        self_type  operator+(difference_type n);
        self_type& operator-=(difference_type n);
        self_type  operator-(difference_type n);

        self_type& operator++()    { return operator+=(1); }
        self_type  operator++(int) { self_type r(*this); operator++(); return r; }
        self_type& operator--()    { return operator-=(1); }
        self_type  operator--(int) { self_type r(*this); operator--(); return r; }

        difference_type operator-(const self_type& other) const;
        reference operator[](difference_type n) { return *(*this + n); }

        bool operator<(const self_type& other) const;
        bool operator>(const self_type& other) const { return other.operator<(*this); }
        bool operator>=(const self_type& other) const { return !operator<(other); }
        bool operator<=(const self_type& other) const { return !operator>(other); }
        bool operator==(const self_type& other) const { return operator<=(other) && operator>=(other); }
        bool operator!=(const self_type& other) const { return !operator==(other); }
};

/***
 * This class implements a deque-like interface with fixed (maximum) size over contiguous memory.
 * Iterators to this structure are never invalidated, unless the element it refers to is popped.
 */
template<typename T>
class circular_buffer
{
    protected:
        // N+1 elements are used to avoid the aliasing of the full and the empty cases.
        using buffer_t = std::vector<T>;
        using self_type = circular_buffer<T>;

    public:
        using value_type             = typename buffer_t::value_type;
        using size_type              = typename buffer_t::size_type;
        using difference_type        = typename buffer_t::difference_type;
        using reference              = value_type&;
        using const_reference        = const value_type&;
        using pointer                = value_type*;
        using const_pointer          = const value_type*;
        using iterator               = circular_buffer_iterator<self_type>;
        using const_iterator         = circular_buffer_iterator<const self_type>;
        using reverse_iterator       = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    protected:
        friend iterator;
        friend const_iterator;
        friend reverse_iterator;
        friend const_reverse_iterator;

        buffer_t  entry_     = {};
        size_type head_      = 0;
        size_type tail_      = 0;

        reference operator[](size_type n)             { return entry_[n % entry_.size()]; }
        const_reference operator[](size_type n) const { return entry_[n % entry_.size()]; }

    public:
        explicit circular_buffer(std::size_t N) : entry_(N) {}

        constexpr size_type size() const noexcept     { return entry_.size(); }
        size_type occupancy() const noexcept          { return tail_ - head_; };
        bool empty() const noexcept                   { return occupancy() == 0; }
        bool full()  const noexcept                   { return occupancy() == size(); }
        constexpr size_type max_size() const noexcept { return entry_.max_size(); }

        reference front()             { return this->operator[](head_); }
        reference back()              { return this->operator[](tail_-1); }
        const_reference front() const { return this->operator[](head_); }
        const_reference back() const  { return this->operator[](tail_-1); }

        iterator begin() noexcept              { return iterator(this, head_); }
        iterator end() noexcept                { return iterator(this, tail_); }
        const_iterator begin() const noexcept  { return const_iterator(this, head_); }
        const_iterator end() const noexcept    { return const_iterator(this, tail_); }
        const_iterator cbegin() const noexcept { return const_iterator(this, head_); }
        const_iterator cend() const noexcept   { return const_iterator(this, tail_); }

        iterator rbegin() noexcept              { return reverse_iterator(this, head_); }
        iterator rend() noexcept                { return reverse_iterator(this, tail_); }
        const_iterator rbegin() const noexcept  { return const_reverse_iterator(this, head_); }
        const_iterator rend() const noexcept    { return const_reverse_iterator(this, tail_); }
        const_iterator crbegin() const noexcept { return const_reverse_iterator(this, head_); }
        const_iterator crend() const noexcept   { return const_reverse_iterator(this, tail_); }

        void clear() { head_ = tail_ = 0; }
        void push_back(const T& item);
        void push_back(const T&& item);
        void pop_front();
};

template<typename T>
auto circular_buffer_iterator<T>::operator+=(difference_type n) -> self_type&
{
    pos += n;
    //if (pos >= buf->entry_.size())
        //pos -= buf->entry_.size();
    return *this;
}

template<typename T>
auto circular_buffer_iterator<T>::operator+(difference_type n) -> self_type
{
    self_type r(*this);
    r += n;
    return r;
}

template<typename T>
auto circular_buffer_iterator<T>::operator-=(difference_type n) -> self_type&
{
    //if (pos < n)
        //pos += buf->entry_.size();
    pos -= n;
    return *this;
}

template<typename T>
auto circular_buffer_iterator<T>::operator-(difference_type n) -> self_type
{
    self_type r(*this);
    r -= n;
    return r;
}

template <typename T>
bool circular_buffer_iterator<T>::operator<(const self_type& other) const
{
    return buf == other.buf && (other - *this) > 0;
}

template<typename T>
auto circular_buffer_iterator<T>::operator-(const self_type& other) const -> difference_type
{
    difference_type d = pos - other.pos;
    //if (other.pos <= buf->tail_ && buf->head_ <= pos && buf->tail_ < buf->head_)
    //{
        //d += buf->entry_.size();
    //}

    return d;
}

template<typename T>
void circular_buffer<T>::push_back(const T& item)
{
    assert(!full());
    entry_[tail_] = item;
    ++tail_;
}

template<typename T>
void circular_buffer<T>::push_back(const T&& item)
{
    assert(!full());
    entry_[tail_] = item;
    ++tail_;
}

template<typename T>
void circular_buffer<T>::pop_front()
{
    assert(!empty());
    ++head_;
}

} //namespace champsim

#endif

