from test_parser import create_csv, display_graph
from recompiler import compile_all
import sys
import os
import subprocess
os.chdir("..")
print(os.getcwd())

warmup_instructions = 200000
simulated_instructions = 500000
log_path = "python/Test logs/champsim_"
path = 'bin/champsim'        # path for executable
tracer = 'tracer'            # path for tracer tests
predictor = 'branch'      # path for predictors 

args = len(sys.argv)

run_predictors = []
recompile_list = []
predictor_list = os.listdir(predictor)

print("use --help to see avaliable commands")
count = 0
recompile = False
print (sys.argv)
for i in sys.argv:
    count += 1
    if i == "--size":
        try:
            size = int(sys.argv[count])
        except:
            print("size needs to be a valid integer")
            exit()
    if i == "--recompile":
        for j in sys.argv[count:]:
            if j == "all":
                recompile_list = predictor_list
            temp = len(recompile_list)
            for k in predictor_list:
                if (j == k):
                    recompile_list.append(k)
                    break
            if temp == len(recompile_list):
                break
        recompile = True
    if i == "--predictors":
        for j in sys.argv[count:]:
            if j == "all":
                run_predictors = predictor_list
                break
            temp = len(run_predictors)
            for k in predictor_list:
                if (i == k):
                    run_predictors.append(k)
                    break
            if temp == len(run_predictors):
                break

if recompile:
    print("Recompiling the following predictors:")
    print(recompile_list)
    compile_all(recompile_list,size) # use -1 if we don't want to change the size

print(run_predictors)
# find all of the files in the tracer folder, then copy the names into tracelist 
file_list = os.listdir(tracer)
tracelist = []
for i in file_list:
    if (i.find('.xz') != -1): # filter out only tracer files 
        tracelist.append(i)


def create_test(instruction_list, predictor):
    log_name = log_path + predictor + "_log.txt"
    count = -1
    n = len(instruction_list) #Number of processess to be created
    for j in range(max(int(len(instruction_list)/n), 1)):
        with open(log_name,'w') as test_log:
            print("writing to:" + log_name)
            procs = [subprocess.Popen(i, shell=True, stdout=test_log) for i in instruction_list[j*n: min((j+1)*n, len(instruction_list))] ]
            #thread = threading.Thread(target = memchecker, args = [procs,log_name])
            #print("Starting Memchecker")
            #thread.start()
            for p in procs:
                if (count != -1):
                    print(procs[count].args + "Has finished Computing")
                p.wait()
                count += 1
            print(procs[count].args + "Has finished Computing")
            test_log.close()
            
            # thread.join()
            create_csv(log_name)
            # print("exiting memchecker, " + i +"has finished ")

