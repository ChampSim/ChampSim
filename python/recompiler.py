import threading
import json
import subprocess

# this file is ment to compile all of the make files so they are ready for the tests
# Compile the makefiles for each champsim instance with it's own predictor

def compile_champsim_instance(predictor):
    print("Compiling:" + predictor)
    # create a copy of the configuration json and create our own config and add it to the files 
    with open(("champsim_config.json"),'r') as file:
        data = json.load(file)
        remove = data['ooo_cpu'][0]['branch_predictor'] # remove this when not debugging 
        with open("python/Test_configs/" + predictor + "_config.json",'w') as config_file:
            data['ooo_cpu'][0]['branch_predictor'] = predictor # change the branch predictor
            data['executable_name'] = "champsim_" + predictor # change the executable name

            # implement changes and close files 
            config_file.seek(0)
            json.dump(data, config_file,indent=4)
            config_file.truncate()
            config_file.close()
            file.close()
            subprocess.Popen(["./config.sh", "python/Test_configs/" + predictor + "_config.json"])
            subprocess.run("make") 

# runs in series 
def compile_all(predictors):
    print("Compiling Champsim instances(This may take a few minutes)")
    for i in predictors: 
        compile_champsim_instance(i)
    print("Instance Compilation has finished)")


# # runs in parallel but has a weird clock skew error
# def compile_all(predictors):
#     print("Compiling Champsim instances(This may take a few minutes)")
#     for i in predictors:
#         thread = threading.Thread(target = compile_champsim_instance, args = [i])
#         thread.start()
#     for j in predictors:
#         thread.join()    
#     print("Instance Compilation has finished)")

                
