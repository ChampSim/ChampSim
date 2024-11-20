Source of the learnt vector parameters. Query, Key, and Value matricies stored here.

DO NOT COMMIT YOUR MODEL TO MAIN.

Always save a local copy of your weights and we can select the best performing model later.


To adjust which files will be loaded adjust the `fileName` variable. 
`fileName` is the __prefix__ of your q,k,v binary files. 
ex:
```
input fileName = "myFile"

files_to_read = ["myFile_q.bin", "myFile_k.bin", "myFile_v.bin"]
```

Main.py will create your files with randomized data for you to start a fresh training session.

