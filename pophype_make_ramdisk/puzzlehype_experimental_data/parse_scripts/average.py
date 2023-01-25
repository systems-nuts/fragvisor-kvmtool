#!/usr/bin/env python3
import argparse
import re
import glob
import sys
import tokenize
import csv

########## Begin
c_line=0
#ff = range(1, 10+1)
#ff = ["node4size64_data", "node4size256_data"]
#var1=None

var_1 = []
timeDiff_1 = []
timeDiff_2 = []
timeDiff_3 = []

filecnt=0

ff = None # currFileName
#for i in [64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384]: 
for i in [64, 256, 1024, 4096, 8192, 16384, 32768, 65536]: 
    ff = "node5size"+str(i)+"_data"
    #print(ff)
    for curr in [1]:
        print('try file: '+ str(ff))
        try: 
            f = open(str(ff),'r')
            c_line = 0
            del var_1[:]
            for line in f:
                #if c_line == 0:
                    #var_1[c_line] = float(line) #t[0]
                    var_1.append(float(line)) #t[0]
                    c_line += 1
                #else:
                #    print('Too many lines ')
                #end if
            #Do Math
            filecnt += 1
            #diff1 = var1 - 0	#
            
            #print('run '+ str(filecnt) +'\t\t1 '+str(diff1)+'\t2 '+str(diff2)+'\t3 '+str(diff3))
            #print('run '+ str(filecnt) +'\t\t1 '+str(diff1))
            
            #timeDiff_1.append(var1)

            #Do Avgerage avg for a file
            curAvg1=0
            for c in var_1:
                    curAvg1 += c
            Aa1 = curAvg1/c_line
            print('Average by ' + str(c_line) + '\t' + str(Aa1))

            #Now time to save to file!
            filename ='jack_total.csv'
            with open(filename,'a') as csvv:
                writer = csv.writer(csvv)
                writer.writerow(str(Aa1))
        
        except: 
            pass
            #Add diff1 to end of array timeDiff_1
            
    #end for ff (a file)
#form next name
