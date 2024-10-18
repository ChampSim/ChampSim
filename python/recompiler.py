import threading
import json
import subprocess
import os
# this file is ment to compile all of the make files so they are ready for the tests
# Compile the makefiles for each champsim instance with it's own predictor
def delete_make():
    # with open('_configuration.mk', "w") as config_file:
    #     config_file.seek(0)
    #     config_file.write("")
    #     config_file.close()
    os.chdir('.csconfig')
    files = os.listdir()
    for i in files:
        print("folder: " + i)
        for j in os.listdir(i):
            if os.path.isdir(i+"/"+j):
               # print("    folder: " + j) 
                for k in os.listdir(i+"/"+j):
                    if os.path.isdir(i+"/"+j +"/"+k):
                       # print("         folder:" + k)
                        for l in os.listdir(i+"/"+j +"/"+k):
                            if os.path.isdir(i+"/"+j +"/"+k + "/" + l):
                                a =2 
                            #    print("             folder:" + l)
                            else:
                        #        print("             file:" + l)
                                os.remove(i+"/"+j +"/"+k + "/" + l)
                    else:
                   #     print("         file:" + k)
                        os.remove(i+"/"+j +"/"+k)
            else: 
              #  print("    file: " + j)
                os.remove(i+"/"+j)
    for i in files:
       # print("folder: " + i)
        for j in os.listdir(i):
            for k in os.listdir(i+"/"+j):
                for l in os.listdir(i+"/"+j +"/"+k):
                    os.rmdir(i+"/"+j +"/"+k + "/" +l)
                os.rmdir(i+"/"+j +"/"+k)
            os.rmdir(i+"/"+j)
        os.rmdir(i)
    os.chdir('..')

# find all the powers of two below the input number
def find_2_pow(max):
    count = 0
    while (pow(2,count) <= max):
        count += 1
    return count


def compile_champsim_instance(*args):
    # account for whether or not we are compiling to a certain size 
    predictor = args[0]
    if len(args) > 1:
        size = args[1] 
    else:
        size = -1
    print("Compiling Predictor: " + predictor)
    if (size != -1):
        print("Compilation size = " + str(pow(2,size)*1024) + "Bits")
        with open(("branch/"+predictor+"/"+predictor+".cc"),'r+') as c_file:
            c_code = c_file.read()
           # print(c_code)
            match predictor:
                case 'gshare':
                    start = c_code.find("GS_HISTORY_TABLE_SIZE = ") + len("GS_HISTORY_TABLE_SIZE = ")
                    end = c_code.find(";", start)
                    c_code = c_code.replace(c_code[start:end],str(pow(2,size)*256),1)
                case 'global_history':
                    start = c_code.find("BIMODAL_TABLE_SIZE = ") + len("BIMODAL_TABLE_SIZE = ")
                    end = c_code.find(";", start)
                    c_code = c_code.replace(c_code[start:end],str(pow(2,size)*256),1)
                    start = c_code.find("HISTORY_LENGTH = ") + len("HISTORY_LENGTH = ")
                    end = c_code.find(";", start)
                    c_code = c_code.replace(c_code[start:end],str(size+8),1)
                case 'bimodal': 
                    start = c_code.find("BIMODAL_TABLE_SIZE = ") + len("BIMODAL_TABLE_SIZE = ")
                    end = c_code.find(";", start)
                    print("replacing table size with: " + str(pow(2,size)*256))
                    c_code = c_code.replace(c_code[start:end],str(pow(2,size)*256),1) # multiply by the n * 2k * 4 = n*8096 = nkb

                case "local_history":
                    start = c_code.find("BIMODAL_TABLE_SIZE = ") + len("BIMODAL_TABLE_SIZE = ")
                    end = c_code.find(";", start)
                    print(str(start)+"|"+str(end))
                    print("replacing table size with: " + str(pow(2,size)*256))
                    c_code = c_code.replace(c_code[start:end],str(pow(2,size)*256),1) # multiply by the n * 2k * 4 = n*8096 = nkb
                    
                    start = c_code.find("HISTORY_TABLE_SIZE = ") + len("HISTORY_TABLE_SIZE = ")
                    end = c_code.find(";", start)
                    print(str(start)+"|"+str(end))
                    print("replacing table size with: " + str(pow(2,size)*32))
                    c_code = c_code.replace(c_code[start:end],str(pow(2,size)*32),1) # multiply by the n * 2k * 4 = n*8096 = nkb

                    start = c_code.find("INDEX_SIZE = ") + len("INDEX_SIZE = ")
                    end = c_code.find(";", start)
                    print(str(start)+"|"+str(end))
                    print("History length with " + str(size+8))
                    c_code = c_code.replace(c_code[start:end],str(size+8),1) # multiply by the n * 2k * 4 = n*8096 = nkb
            c_file.seek(0)
            c_file.write(c_code)
            c_file.truncate()
            c_file.close()

    # create a copy of the configuration json and create our own config and add it to the files 
    with open(("champsim_config.json"),'r') as file:
        data = json.load(file)
        remove = data['ooo_cpu'][0]['branch_predictor'] # remove this when not debugging 
        with open("python/Test_configs/" + predictor + "_config.json",'w') as config_file:
            data['ooo_cpu'][0]['branch_predictor'] = predictor # change the branch predictor
            if (size != -1):
                data['executable_name'] = "champsim_" + predictor + str(pow(2,size)) + "k"# change the executable name
                print("champsim_" + predictor + str(pow(2,size)) + "k")
            else:
                data['executable_name'] = "champsim_" + predictor # change the executable name
            # implement changes and close files 
            config_file.seek(0)
            json.dump(data, config_file,indent=4)
            config_file.truncate()
            config_file.close()
            file.close()
        # becuase we can't change the variables in the C code we need to make scaling size for each branch predictor
        # case doesn't exist in before python 3.10 so I have to do an ugly elif tree
        # we are scaling size to be the number of kilobytes that we will use
        # we are going give all of the predictors 4 counter bits for simplicity
    try:
        print ("config file: python/Test_configs/" + predictor + "_config.json")
        print("compiling...")
        subprocess.run(["./config.sh", "python/Test_configs/" + predictor + "_config.json"])
        subprocess.run(["make","-j","-s"], check=True)
        subprocess.run(["make","-j","clean"], check=True)

    except subprocess.CalledProcessError as e:
        print(f"Error running make: {e}")
    
    



# runs in series 
def compile_all(predictors,size):
    print("Compiling Champsim instances(This may take a few minutes)")
    size = find_2_pow(size)
    for i in predictors: 
        if (size != -1):
            print(size)
            for k in range(0,size):
                compile_champsim_instance(i,k)
        else:
            compile_champsim_instance(i)
    print("Instance Compilation has finished")

# os.chdir("Z:/shared_files/Champsim/BranchPrediction/Champsim")
# print(os.getcwd())
# compile_champsim_instance("bimodal",4)

# runs in parallel but has a weird clock skew error
# def compile_all(predictors,size):
#     print("Compiling Champsim instances(This may take a few minutes)")
#     size = find_2_pow(size)
#     for i in predictors:
#         for k in range(0,size):
#             thread = threading.Thread(target = compile_champsim_instance, args = [i,k])
#             print("starting thread for compiling:" + i + "of size " + str(pow(2,k)) + "kb")
#             thread.start()
#     for j in predictors:
#         thread.join()    
#     print("Instance Compilation has finished)")

                
