pipe_path = "/tmp/pinToolPipe"
with open(pipe_path, "w") as pipe:
   pipe.write("start\n")
with open(pipe_path, "w") as pipe:
   pipe.write("new_trace_file.txt\n")
print("Hello world!")
with open(pipe_path, "w") as pipe:
   pipe.write("stop\n")