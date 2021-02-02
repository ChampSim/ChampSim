#ifndef OPERABLE_H
#define OPERABLE_H

namespace champsim {

class operable {
    public:
    const double CLOCK_SCALE;

    double leap_operation = 0;
    uint64_t current_cycle = 0;

    explicit operable(double scale) : CLOCK_SCALE(1-scale) {}

    void _operate()
    {
        // skip periodically
        leap_operation += CLOCK_SCALE;
        if (leap_operation > 1)
        {
            leap_operation -= 1;
            return;
        }

        ++current_cycle;

        operate();
    }

    virtual void operate() = 0;
};

}

#endif

