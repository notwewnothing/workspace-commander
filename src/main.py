
from fileinput import filename
import os,sys
import subprocess
import glob
from os import path
i = 0
r = 0
while True:
    i += 1
    r = i + r
    # now i need to implement a way that will output the r into an outut.txt
    print(r)
    with open("output.txt", "a") as f:
        f.write(str(r) + "\n")  
