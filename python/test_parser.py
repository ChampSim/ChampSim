import pandas as pd
import numpy as np
import warnings
import datetime
warnings.simplefilter(action='ignore', category=FutureWarning)


python_path = "python/"
# parses the relavant part of the output for branch prediction 
def Subparse(string,log_text):
    # ChampSim/tracer/600.perlbench_s-210B.champsimtrace.xz


    start = string.find("tracer/") + len("tracer/")
    stop = string.find(".champsimtrace.xz")
    output[0] = string[start:stop]

    start = string.find("CPU 0 cumulative IPC: ") + len("CPU 0 cumulative IPC:")
    stop = string.find(" instructions: ",start)
    output[1] = float(string[start:stop])

    start = stop + len(" instructions: ")
    stop = string.find("cycles:",start)
    output[2] = float(string[start:stop])

    start = stop + len("cycles:")
    stop = string.find("CPU 0 Branch Prediction Accuracy:",start)
    output[3] = float(string[start:stop])

    start = stop + len("CPU 0 Branch Prediction Accuracy:")
    stop = string.find("% MPKI: ",start)
    output[4] = float(string[start:stop])

    start = stop + len("% MPKI: ")
    stop = string.find(" Average ROB Occupancy at Mispredict: ",start)
    output[5] = float(string[start:stop])

    start = stop + len(" Average ROB Occupancy at Mispredict: ")
    stop = string.find("Branch type MPKI",start)
    output[6] = float(string[start:stop])

    start = string.find("BRANCH_DIRECT_JUMP:") + len("BRANCH_DIRECT_JUMP:")
    stop = string.find("BRANCH_INDIRECT:",start)
    output[7] = float(string[start:stop])

    start = stop + len("BRANCH_INDIRECT:")
    stop = string.find("BRANCH_CONDITIONAL:",start)
    output[8] = float(string[start:stop])

    start = stop + len("BRANCH_CONDITIONAL:")
    stop = string.find("BRANCH_DIRECT_CALL:",start)
    output[9] = float(string[start:stop])

    start = stop + len("BRANCH_DIRECT_CALL:")
    stop = string.find("BRANCH_INDIRECT_CALL:",start)
    output[10] = float(string[start:stop])

    start = stop + len("BRANCH_INDIRECT_CALL:")
    stop = string.find("BRANCH_RETURN:",start)
    output[11] = float(string[start:stop])

    start = stop + len("BRANCH_RETURN:")
    output[12] = float(string[start:len(string)])

    start = string.find("(Simulation time: ") + len("(Simulation time: ")
    stop = string.find(")",start)
    output[13] = string[start:stop]

    append_data = pd.DataFrame(list([output]),columns=params)
    # print(append_data)
    return append_data

# find the relavent text blocks in the champsim_log output 
# parse the individual components of the text block into a dataframe
# concatenate the tests together to get one dataframe of tests
def create_csv(log):
    print ("creating a csv file for: " + log)
    global params 
    params =   ("Test",
                "IPC",
                "Instructions",
                "Cycles",
                "Branch Prediction Accuracy",
                "MPKI",
                "Avg ROB occupancy at mispredict",
                "Branch indirect Jump",
                "Branch Indirect",
                "Branch Conditional",
                "Branch Direct Call",
                "Branch Indirect Call",
                "Branch_Return",
                "Test Length")
    global data 
    data = pd.DataFrame(columns=params)

    global output 
    output = [0,0,0,0,0,0,0,0,0,0,0,0,0,0]

    testlog = open(log,'r')
    log_text = testlog.read()
    test_start = 0
    test_end = 0
    test_it = 0
    while (test_end != -1):
        test_it =+ 1
        test_start = log_text.find("Simulation complete",test_end)
        test_end = log_text.find("LLC TOTAL",test_start)
        if (test_start == -1 or test_end == -1):
            break
        else:
            temp_string = log_text[test_start:test_end]
            append_data = Subparse(temp_string,log_text)
            data = pd.concat([data,append_data])

    # find the name of the branch predictor from the json, the csv file will be named after it 
    start = log.find(python_path + "Test logs/") + len(python_path + "Test logs/")
    end = log.find("_log")
    predictor_name = log[start:end]
    data.to_csv(python_path + "Trace_tests/" + predictor_name +".csv")
    print ("Created file:" + predictor_name +".csv")



    # input: list of names of the predictors you want graphed 
def display_graph(input):
    # This is the section of the program that graphs the data 
    import matplotlib.pyplot as plt
    import os
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



# create_csv("champsim/Test logs/champsim_Jack_log.txt")
# display_graph()