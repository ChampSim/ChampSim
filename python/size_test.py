from test_parser import create_csv, display_graph
from recompiler import compile_all, compile_champsim_instance, find_2_pow
import sys
import os
import subprocess
os.chdir("..")
print(os.getcwd())

warmup_instructions = 200000
simulated_instructions = 500000
log_path = "python/Test logs/champsim_"
exec_path = 'bin/'        # path for executable
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
        power = 0
        for j in sys.argv[count:]:
            if j == "all":
                run_predictors = predictor_list
                break
            temp = len(run_predictors)
            for k in predictor_list:
                if (j == k):
                    while pow(2,power) <= size:
                        run_predictors.append(k+str(pow(2,power))+"k")

                        power += 1
                    break
            if temp == len(run_predictors):
                break


# check if all of the tests the user requested are compiled 

compiled_execs = os.listdir(exec_path)
execs_to_compile = []

found = False
for i in run_predictors:
    for j in compiled_execs:
        # print(("champsim_"+i)  + " |"+ j + "|" + str(found))
        if (("champsim_"+i) == j): # search for existing compiled instances
            found = True # if we find the instance already exists we can break
            break
    if (found == False): # if we didn't find an existing predictor we added to the list of predictors to be compiled 
        # print(i)
        execs_to_compile.append(i)
    found = False # reset the found variable 
            
print(execs_to_compile)
response = input("The tests above haven't been compiled yet, would you like to compile them (y/n)")

if response == "y":
# compile the missing executables
# the following section is a cognito-hazard, I do not recommend looking at it if you wish to retain your sanity 
    for i in execs_to_compile:
        #print(i)
        tempsize = []
        for j in reversed(i): # traverse the name of the instance to compile from the back
            if j == "k":  # ignore the first k
                continue 
            else:
                try: 
                    tempsize.insert(0,int(j)) # insert each element into the tempsize variable, the try accept structure causes a break if int(j) cannot find any more ints 
                except:
                    break
        i = i[0:len(i)-(len(tempsize)+1)] # remove the size of the predictor from the name
        int_result = int(''.join(map(str, tempsize))) # cursed way to join lists of ints into one int 
        #print(i)
        #print(int_result)
        compile_champsim_instance(i,int_result) # compile the predictor i with our joined integer list 
elif response == "n":
    a=2 # nop
else:
    print("invalid response, exiting")
    exit()
    

if recompile:
    print("Recompiling the following predictors:")
    print(recompile_list)
    compile_all(recompile_list,find_2_pow(size)) # use -1 if we don't want to change the size

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

