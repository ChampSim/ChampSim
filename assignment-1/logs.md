This is a document to track the progress by performing various experiments on assignment.

## Problem

Goal

Evaluate 3 benchmarks, which can be found in the link https://dpc3.compas.cs.stonybrook.edu/champsim-traces/speccpu/

-   649.fotonik3d_s-8225B.champsimtrace.xz - Avinash
-   607.cactuBSSN_s-4248B.champsimtrace.xz - Naman
-   623.xalancbmk_s-325B.champsimtrace.xz - Shubham

For the following branch predition schemes

-   G Share (Available in champsim)
-   Perceptron (Available in champsim)
-   Tage (Took the code from https://github.com/KanPard005/RISCY_V_TAGE/blob/master/branch/tage.h)

#### Sept 23, Avinash

-   Forked the champsim repository to collaborate and work on the assignment.
-   Have added the tage and hybrid predictors, we need to be careful with the budget size of the branch predictors.

Given hardware cost is 128KB, give or take 2KB.

TODO: Estimate the sizing for each branch prediction scheme

So for the perceptron,
