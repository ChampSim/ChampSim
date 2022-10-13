The cvp2champsim tracer comes as is with no guarantee that it covers every conversion case.

The tracer is used to convert the traces from the 2nd Championship Value 
Prediction (CVP) to a ChampSim-friendly format. 

CVP-1 Site: https://www.microarch.org/cvp1/

CVP-2 Site: https://www.microarch.org/cvp1/cvp2/rules.html

To use the tracer first compile it using g++:

    g++ cvp2champsim.cc -o cvp_tracer

To convert a trace execute:

    ./cvp_tracer TRACE_NAME.gz

The ChampSim trace will be sent to standard output so to keep and compress the output trace run:

    ./cvp_tracer TRACE_NAME.gz | gzip > NEW_TRACE.champsim.gz

Adding the "-v" flag will print the dissassembly of the CVP trace to standard 
error output as well as the ChampSim format to standard output.
