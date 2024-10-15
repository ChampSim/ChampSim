import subprocess
import os
import time
import sys
import threading
import json
from test_parser import create_csv, display_graph
from recompiler import compile_all

# What I want for this file: Run a command line with different predictors, if that predictor does not exist, generate the executable, if it does already exist do not generate the executable
# Need some way to figure out if the executable is up to date, I could just try using the make, becuase once the json is created we can just do the make 


os.chdir("..")
print(os.getcwd())

printout = 1
warmup_instructions = 2000000
simulated_instructions = 5000000
log_path = "python/Test logs/champsim_"
path = 'bin/champsim'        # path for executable
tracer = 'tracer'            # path for tracer tests
predictor = 'branch'      # path for predictors 


run_predictors = []
recompile_list = []
predictor_list = os.listdir(predictor)
# print(predictor_list)
# Take command line arguments 
args = len(sys.argv)

count = 0
recompile = False
print("use --help to see avaliable commands")
for i in sys.argv:
    count += 1
    if i == "--warmup-instructions":
        warmup_instructions = int(sys.argv[count])
    elif i == "--simulation_instructions":
        simulated_instructions = int(sys.argv[count])
    elif i == "--all":
        recompile = True
        run_predictors = predictor_list
        recompile_list = predictor_list
    elif i == "--predictors":
        for j in sys.argv[count:]:
            if j == "all":
                run_predictors = predictor_list
                break
            temp = len(run_predictors)
            for k in predictor_list:
                if (j == k):
                    run_predictors.append(k)
                    break
            if temp == len(run_predictors):
                break
    elif i == "--recompile":
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
    elif i == "--help":
        print ("Avaliable instructions \n" +
               "--warmup-instructions : Number of warmup instructions to run \n" +
               "--simulation_instructions : Number of total instructions to run \n" +
               "--all: compiles and runs all avaliable predictors\n" + 
               "--predictors: list the predictors you want to run in the format --predictors predictor1 predictor2 ... \n" +
               "--predictors all: Runs all avaliable predictors\n" +
               "--recompile: Recompiles the instances of champsim, enable this if you have changed any of the predictor files. this in the format --recompile predictor1 predictor2 \n" +
               "--recompile all: Recompiles all avaliable predictors, use this command the first time you run the program")

if recompile:
    print("Recompiling the following predictors:")
    print(recompile_list)
    compile_all(recompile_list,-1) # use -1 if we don't want to change the size

print(run_predictors)

# find all of the files in the tracer folder, then copy the names into tracelist 
file_list = os.listdir(tracer)
tracelist = []
for i in file_list:
    if (i.find('.xz') != -1): # filter out only tracer files 
        tracelist.append(i)


# from https://stackoverflow.com/questions/30686295/how-do-i-run-multiple-subprocesses-in-parallel-and-wait-for-them-to-finish-in-py


# Create a thread for each list of tests, the argument is a list of instructions 
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




for i in run_predictors:
    # Create the list of instructions through concatenation
    instruction_list = []
    for j in tracelist:
        instruction = (path + "_" + i +
                    " --warmup-instructions " + str(warmup_instructions) +
                    " --simulation-instructions "  + str(simulated_instructions) + 
                    " " + tracer + "/" + j)
        instruction_list.append(instruction)
    print(instruction_list)
    thread = threading.Thread(target = create_test , args = [instruction_list,i])
    print ("Creating thread" )
    thread.start()
for i in run_predictors:
    thread.join()


if (len(run_predictors) > 0):
    display_graph(run_predictors)
# import subprocess
# import os
# import time
# import sys
# import threading
# import psutil
# import json
# from test_parser import create_csv, display_graph
# from recompiler import compile_all

# # What I want for this file: Run a command line with different predictors, if that predictor does not exist, generate the executable, if it does already exist do not generate the executable
# # Need some way to figure out if the executable is up to date, I could just try using the make, becuase once the json is created we can just do the make 


# os.chdir("..")
# print(os.getcwd())
# def memchecker(pids,log):
#     time.sleep(10)
#     for p in pids:
#         a = psutil.Process(p.pid)
#         for i in a.children():
#             print ("PID:" + str(i.pid))
#             print("Memory amount: " + str((i.memory_info().rss/(1024*1024))) + "MB")
#             with open(log,'w') as data_log:
#                 data_log.write("Memory Info: (" + p.args + "|" + str((i.memory_info().rss/(1024*1024))) + "MB )")  

# printout = 1
# warmup_instructions = 200000
# simulated_instructions = 500000
# log_path = "python/Test logs/champsim_"
# path = 'bin/champsim'        # path for executable
# tracer = 'tracer'            # path for tracer tests
# predictor = 'branch'      # path for predictors 


# run_predictors = []
# recompile_list = []
# predictor_list = os.listdir(predictor)
# # print(predictor_list)
# # Take command line arguments 
# args = len(sys.argv)

# count = 0
# recompile = False
# print("use --help to see avaliable commands")
# for i in sys.argv:
#     count += 1
#     if i == "--warmup-instructions":
#         warmup_instructions = int(sys.argv[count])
#     elif i == "--simulation_instructions":
#         simulated_instructions = int(sys.argv[count])
#     elif i == "--all":
#         recompile = True
#         run_predictors = predictor_list
#         recompile_list = predictor_list
#     elif i == "--predictors":
#         for j in sys.argv[count:]:
#             if j == "all":
#                 run_predictors = predictor_list
#                 break
#             temp = len(run_predictors)
#             for k in predictor_list:
#                 if (j == k):
#                     run_predictors.append(k)
#                     break
#             if temp == len(run_predictors):
#                 break
#     elif i == "--recompile":
#         for j in sys.argv[count:]:
#             if j == "all":
#                 recompile_list = predictor_list
#             temp = len(recompile_list)
#             for k in predictor_list:
#                 if (j == k):
#                     recompile_list.append(k)
#                     break
#             if temp == len(recompile_list):
#                 break
#         recompile = True
#     elif i == "--help":
#         print ("Avaliable instructions \n" +
#                "--warmup-instructions : Number of warmup instructions to run \n" +
#                "--simulation_instructions : Number of total instructions to run \n" +
#                "--all: compiles and runs all avaliable predictors\n" + 
#                "--predictors: list the predictors you want to run in the format --predictors predictor1 predictor2 ... \n" +
#                "--predictors all: Runs all avaliable predictors\n" +
#                "--recompile: Recompiles the instances of champsim, enable this if you have changed any of the predictor files. this in the format --recompile predictor1 predictor2 \n" +
#                "--recompile all: Recompiles all avaliable predictors, use this command the first time you run the program")

# if recompile:
#     print("Recompiling the following predictors:")
#     print(recompile_list)
#     compile_all(recompile_list)

# print(run_predictors)

# # find all of the files in the tracer folder, then copy the names into tracelist 
# file_list = os.listdir(tracer)
# tracelist = []
# for i in file_list:
#     if (i.find('.xz') != -1): # filter out only tracer files 
#         tracelist.append(i)


# # from https://stackoverflow.com/questions/30686295/how-do-i-run-multiple-subprocesses-in-parallel-and-wait-for-them-to-finish-in-py


# # Create a thread for each list of tests, the argument is a list of instructions 
# def create_test(instruction_list, predictor):
#     log_name = log_path + predictor + "_log.txt"
#     count = -1
#     n = len(instruction_list) #Number of processess to be created
#     for j in range(max(int(len(instruction_list)/n), 1)):
#         with open(log_name,'w') as test_log:
#             print("writing to:" + log_name)
#             procs = [subprocess.Popen(i, shell=True, stdout=test_log) for i in instruction_list[j*n: min((j+1)*n, len(instruction_list))] ]
#             #thread = threading.Thread(target = memchecker, args = [procs,log_name])
#             #print("Starting Memchecker")
#             #thread.start()
#             for p in procs:
#                 if (count != -1):
#                     print(procs[count].args + "Has finished Computing")
#                 p.wait()
#                 count += 1
#             print(procs[count].args + "Has finished Computing")
#             test_log.close()
            
#             # thread.join()
#             create_csv(log_name)
#             # print("exiting memchecker, " + i +"has finished ")




# for i in run_predictors:
#     # Create the list of instructions through concatenation
#     instruction_list = []
#     for j in tracelist:
#         instruction = (path + "_" + i +
#                     " --warmup-instructions " + str(warmup_instructions) +
#                     " --simulation-instructions "  + str(simulated_instructions) + 
#                     " " + tracer + "/" + j)
#         instruction_list.append(instruction)
#     print(instruction_list)
#     thread = threading.Thread(target = create_test , args = [instruction_list,i])
#     print ("Creating thread" )
#     thread.start()
# for i in run_predictors:
#     thread.join()


# if (len(run_predictors) > 0):
#     display_graph(run_predictors)