The cvp2champsim tracer comes as is with no guarantee that it covers every conversion case.

The tracer is used to convert the traces from the 2nd Championship Value 
Prediction (CVP) to a ChampSim-friendly format. 

CVP-1 Site: https://www.microarch.org/cvp1/

CVP-2 Site: https://www.microarch.org/cvp1/cvp2/rules.html


The current converter implements several improvements on top of the original one. These improvements are described in the paper **Rebasing Microarchitectural Research with Industry Traces**, which will be published in the 2023 IEEE International Symposium on Workload Characterization.

If you use this converter (or traces converted with it), please cite our paper:

    J. Feliu, A. Perais, D. Jiménez, A. Ros, "Rebasing Microarchitectural Research with Industry Traces", in IEEE International Symposium on Workload Characterization (IISWC), 2023.


To use the tracer first compile it using g++:

    g++ cvp2champsim.cc -o cvp2champsim

To convert a trace execute:

    ./cvp2champsim -t TRACE_NAME.gz

The ChampSim trace will be sent to standard output so to keep and compress the output trace run:

    ./cvp2champsim -t TRACE_NAME.gz | gzip > NEW_TRACE.champsim.gz


Adding the "-v" flag will print the dissassembly of the CVP trace to standard 
error output as well as the ChampSim format to standard output.

You can also enable/disable the conversion improvements described in the paper individually using the "-i" option. The list of available improvements includes the individual improvements: "imp_mem-regs", "imp_base-update", "imp_mem-footprint", "imp_call-stack", "imp_branch-regs", and "imp_flag-regs", three sets of improvements: "All_imps", "Memory_imps", and "Branch_imps", and "No_imp", which resorts to the original cvp2champsim converter. By default, all improvements are enabled. 


CVP traces download links:

CVP-1 public set: https://perscido.univ-grenoble-alpes.fr/datasets/DS382

CVP-1 secret set: https://perscido.univ-grenoble-alpes.fr/datasets/DS384