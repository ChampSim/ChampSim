import pandas as pd
import numpy as np
from recompiler import find_2_pow
import warnings
import datetime
import matplotlib.pyplot as plt
import os
import json
warnings.simplefilter(action='ignore', category=FutureWarning)


python_path = "python/"
# parses the relavant part of the output for branch prediction 

def PARSE_JSON(log,test):
    params=["Test",
            "Branch Prediction Accuracy",
            "instructions",
            "cycles",
            "MPKI",
            "Avg ROB occupancy at mispredict",
            "IPC"]

    mispredict = [  "BRANCH_CONDITIONAL",
                    "BRANCH_DIRECT_JUMP",
                    "BRANCH_DIRECT_CALL",
                    "BRANCH_INDIRECT",
                    "BRANCH_INDIRECT_CALL",
                    "BRANCH_RETURN"]
    
    output = [0,0,0,0,0,0,0,0,0,0,0,0,0]
    with open(log,"r") as file:
        data = json.load(file)
        output[0] = data[test]['traces'][0]
        a = data[test]['sim']['cores'][0]
        for i in range(1,len(params)-1):
            output[i] = data[test]['sim']['cores'][0][params[i]]
        output[6] = data[test]['sim']['cores'][0][params[4]]/data[test]['sim']['cores'][0][params[3]]
        for i in range(0,len(mispredict)):
            output[i+7] = data[test]['sim']['cores'][0]['mispredict'][mispredict[i]]
        for i in mispredict:
            params.append(i)
        print(output)
    return pd.DataFrame(list([output]),columns= params)




# def Subparse(string,log_text):
#     # ChampSim/tracer/600.perlbench_s-210B.champsimtrace.xz


#     start = string.find("tracer/") + len("tracer/")
#     stop = string.find(".champsimtrace.xz")
#     output[0] = string[start:stop]

#     start = string.find("CPU 0 cumulative IPC: ") + len("CPU 0 cumulative IPC:")
#     stop = string.find(" instructions: ",start)
#     output[1] = float(string[start:stop])

#     start = stop + len(" instructions: ")
#     stop = string.find("cycles:",start)
#     output[2] = float(string[start:stop])

#     start = stop + len("cycles:")
#     stop = string.find("CPU 0 Branch Prediction Accuracy:",start)
#     output[3] = float(string[start:stop])

#     start = stop + len("CPU 0 Branch Prediction Accuracy:")
#     stop = string.find("% MPKI: ",start)
#     output[4] = float(string[start:stop])

#     start = stop + len("% MPKI: ")
#     stop = string.find(" Average ROB Occupancy at Mispredict: ",start)
#     output[5] = float(string[start:stop])

#     start = stop + len(" Average ROB Occupancy at Mispredict: ")
#     stop = string.find("Branch type MPKI",start)
#     output[6] = float(string[start:stop])

#     start = string.find("BRANCH_DIRECT_JUMP:") + len("BRANCH_DIRECT_JUMP:")
#     stop = string.find("BRANCH_INDIRECT:",start)
#     output[7] = float(string[start:stop])

#     start = stop + len("BRANCH_INDIRECT:")
#     stop = string.find("BRANCH_CONDITIONAL:",start)
#     output[8] = float(string[start:stop])

#     start = stop + len("BRANCH_CONDITIONAL:")
#     stop = string.find("BRANCH_DIRECT_CALL:",start)
#     output[9] = float(string[start:stop])

#     start = stop + len("BRANCH_DIRECT_CALL:")
#     stop = string.find("BRANCH_INDIRECT_CALL:",start)
#     output[10] = float(string[start:stop])

#     start = stop + len("BRANCH_INDIRECT_CALL:")
#     stop = string.find("BRANCH_RETURN:",start)
#     output[11] = float(string[start:stop])

#     start = stop + len("BRANCH_RETURN:")
#     output[12] = float(string[start:len(string)])

#     start = string.find("(Simulation time: ") + len("(Simulation time: ")
#     stop = string.find(")",start)
#     output[13] = string[start:stop]

#     append_data = pd.DataFrame(list([output]),columns=params)
#     # print(append_data)
#     return append_data

# find the relavent text blocks in the champsim_log output 
# parse the individual components of the text block into a dataframe
# concatenate the tests together to get one dataframe of tests
def create_csv(log):
    # print ("creating a csv file for: " + log)
    params =["Test",
            "Branch Prediction Accuracy",
            "instructions",
            "cycles",
            "MPKI",
            "Avg ROB occupancy at mispredict",
            "IPC",
            "BRANCH_CONDITIONAL",
            "BRANCH_DIRECT_JUMP",
            "BRANCH_DIRECT_CALL",
            "BRANCH_INDIRECT",
            "BRANCH_INDIRECT_CALL",
            "BRANCH_RETURN"]
    data = pd.DataFrame(columns=params)
    test = 0
    while True:
        try:
            append_data = PARSE_JSON(log,test)
            data = pd.concat([data,append_data])
            test += 1
        except: 
            break
        
                #print(data)


    # find the name of the branch predictor from the json, the csv file will be named after it 
    # print(len("python/Test_logs/"))
    # print(len(".json"))
    # print(log)
    # print(log[len("python/Test_logs/"):(len(log) - len(".json"))])
    # print(data)
    data.to_csv("python/Trace_tests/" + log[len("python/Test_logs/"):(len(log) - len(".json"))] +".csv",mode='w+')
    print ("Created file:" + log +".csv")



def display_size_graph(input,range):
    file_list = os.listdir(python_path + "Trace_tests")
    
    # get the types of predictors we will be running based on the names
    predictor_list = []
    predictors = []
    for i in input:
        count = 0
        for j in reversed(i[:len(i)-1]):
            try: 
                a = int(j)
                count += 1
            except:
                break
        predictors.append([i[:len(i)-count-1],i[len(i)-count-1:len(i)-1],0])
        predictor_list.append(i[:len(i)-count-1])
    # print(predictors)
    predictor_list = set(predictor_list)

    csvlist = []
    for i in file_list:
        for j in input:
            if (i.find(j) != -1):
                if (i.find('.csv') != -1): # filter out only tracer files 
                    csvlist.append(i)
    # print(csvlist)
    count = 1
    x_axis = []
    while (count <= range):
        x_axis.append(str(count))
        count = count*2
    
    for p in predictors:
        for c in csvlist:
            branch_csvs = pd.read_csv(python_path + "Trace_tests/" + c)
            #print(c)
            #print(branch_csvs)
            #print(p[0] + str(p[1]) + "k.csv" + "|" + c)
            if (p[0] + str(p[1]) + "k.csv" == c):
                p[2] =  branch_csvs["Branch Prediction Accuracy"].mean()

    for pl in predictor_list:
        temp_pred_list = []
        for p in predictors:
            if (p[0] == pl):
                temp_pred_list.append(p[2])
        #print(temp_pred_list)
        #rint(x_axis)
        plt.plot(x_axis,temp_pred_list, label= pl)

    plt.legend()
    plt.ylabel("Prediction Accuracy")
    plt.xlabel("Predictor size (kbits)")
        # plt.plot(x_axis,y_axis)
    plt.show()

    # input: list of names of the predictors you want graphed 
def display_graph(input):
    # This is the section of the program that graphs the data 

    print(os.getcwd())
    # Find the CSV logs 
    # then add them to the test if they have unique names 
    file_list = os.listdir(python_path + "Trace_tests") 
    csvlist = []
    for i in file_list:
        for j in input:
            if (i.find(j) != -1):
                if (i.find('.csv') != -1): # filter out only tracer files 
                    csvlist.append(i)
                    
    print(csvlist)
    csvlist.sort(key = str.casefold)
    print(csvlist)
    # initialization data for the plot 
    fig,ax = plt.subplots(figsize =(16, 9))
    branch_csvs = []
    barwidth = 1/(len(csvlist)+1)
    count = 0

    # todo, this code looks awful please rewrite if showing anyone else
    for i in csvlist:
        
        branch_csvs = pd.read_csv(python_path + "Trace_tests/" + i)
        BPA = branch_csvs["Branch Prediction Accuracy"]
        bar_offset = []
        a = np.arange(float(len(BPA)))
        for i in a:
            bar_offset.append(i + barwidth*count)
        bar = plt.bar(bar_offset, BPA, width=barwidth, label = csvlist[count])
        for i in bar:
            height = i.get_height()
            plt.text( i.get_x() + i.get_width()/2, height, f'{height:.2f}', ha='center', va='bottom')
        count += 1

    branch_csvs = pd.read_csv(python_path + "Trace_tests/" + csvlist[0])
    # label axis
    plt.xlabel('Trace Benchmarks', fontweight ='bold', fontsize = 15) 
    plt.ylabel('Branch Prediction Accuracy', fontweight ='bold', fontsize = 15) 

    # Add Text watermark
    fig.text(0.125, 0.875, 'Project Claros', fontsize = 12,
            color ='grey', ha ='left', va ='top',
            alpha = 0.7)
    a = csvlist[0]
    # Create appropriate X axis labels 
    plt.xticks([r+barwidth for r in range(len(branch_csvs))],branch_csvs["Test"])

    plt.legend()
    plt.show()

#create_csv("ChampSim/python/Test_logs/bimodal1k.json")

# for windows only
# for i in os.listdir("champsim/python/Test logs"):
#     create_csv("champsim/python/Test logs/"+ i)

# display_size_graph(["bimodal1k","bimodal2k","bimodal4k","bimodal8k","bimodal16k","bimodal32k","bimodal64k","bimodal128k",
#                    "gshare1k","gshare2k","gshare4k","gshare8k","gshare16k","gshare32k","gshare64k","gshare128k",
#                    "global_history1k","global_history2k","global_history4k","global_history8k","global_history16k","global_history32k","global_history64k","global_history128k"],128)


#,"bimodal256k","bimodal512k","bimodal1024k "gshare256k","gshare512k","gshare1024k","global_history256k","global_history512k","global_history1024k"