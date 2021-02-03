#ifndef OPERABLE_H
#define OPERABLE_H

#include <iostream>

namespace champsim {

class operable {
    public:
    const double CLOCK_SCALE;

    double leap_operation = 0;
    uint64_t current_cycle = 0;

    explicit operable(double scale) : CLOCK_SCALE(scale-1) {}

    void _operate()
    {
        // skip periodically
        leap_operation += CLOCK_SCALE;

        if (leap_operation >= 1)
        {
            leap_operation -= 1;
            return;
        }

        ++current_cycle;

        operate();
    }

    virtual void operate() = 0;
};

class by_next_operate
{
    public:
        bool operator() (operable *lhs, operable *rhs) const
        {
            return lhs->leap_operation < rhs->leap_operation;
        }
};

}

#endif

