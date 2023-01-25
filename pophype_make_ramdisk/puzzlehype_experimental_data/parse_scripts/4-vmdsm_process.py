#! /usr/bin/env python
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright © 2018 jackchuang <jackchuang@mir>
#
# Distributed under terms of the MIT license.

"""
./app CENSUS APP_SOURCE_CODE
"""
import sys, os, pprint
import subprocess
from collections import OrderedDict

CENSUS=sys.argv[1]
APP_SOURCE_CODE=sys.argv[2]
print '\t ==================================================== \t'
print '\t\t' + sys.argv[0]+ ' ' + sys.argv[1] + ' ' + sys.argv[2]
print '\t\t' + sys.argv[0]+ ' ' + sys.argv[1] + ' ' + sys.argv[2]
print '\t\t' + sys.argv[0]+ ' ' + sys.argv[1] + ' ' + sys.argv[2]
print '\t ==================================================== \t'

# print only inner dict
def myprint(d):
    for k, v in d.iteritems():
        if isinstance(v, dict):
            myprint(v)
        else:
            print "{0} : {1}".format(k, v)

if len(sys.argv) < 2:
    sys.exit("Usage: %s <source_code_compiled_w/_-g>" % (sys.argv[0]))
    
if not os.path.isfile(APP_SOURCE_CODE):
    sys.exit("source_code not exit")

print("synamically importing data from: %s%s" % (CENSUS[:-3], CENSUS[-3:]))
census = __import__('%s' % (CENSUS[:-3]))
vmdsm_census = census

# e.g. pgfault: {k, v} = {info, count} = {'34355813389 7ffc41c0d000 W': 1629}
# e.g. vmdsm: {k, v} = {info, count} = {'34355813389 34355813389 34355813389 W 7ffc41c0d000 7ffc41c0d000 7ffc41c0d000': 1629}
#                                                                                   pgfault     rip          rbp
vmdsm_census_sorted = OrderedDict((k, v) for k, v in sorted(vmdsm_census.AllData.items(), key=lambda x: x[1], reverse=True))

# Usage: ./final.py | sort -n -k 3
threshold = 10
top_cnt = 30
#
sys.stdout.flush()
i=0
# Output
name = '%s' % CENSUS[:-3]
out_name = name.replace("_vmdsm_census", "")
out_name = '%s_vmdsm_traffic' % out_name
ff = open('%s' % out_name, 'w')
#print ('\n\n\n ====================================================')
for x in range(0, 1):
    print('Writing to %s' % out_name)
#print (' ====================================================')
print '=== threshold [[' + str(threshold) + ']], ' + 'list top [[' + str(top_cnt) + ']] vmdsm pgfaults ==='
print ('<rip>                        <pa>        <va>   <err_code>   <err_flag>     <cnt>      <addr2line vmlinux>')
for k, v in vmdsm_census_sorted.items():
    if v > threshold:
        #print k, v # print all including numbers (intergerss) for sorting 
        s = k.split() # to string
        rip = s[5]
        addr2line = os.popen('addr2line -e vmlinux %s' % rip).read()
        addr2line = addr2line.encode().decode('utf-8').replace('\n','') #py3 # .encode() just in case
        # order matters
        addr2line = addr2line.decode('utf-8').replace('/home/jackchuang/kh/./','')
        addr2line = addr2line.decode('utf-8').replace('/home/jackchuang/kernel_hype/./','')
        addr2line = addr2line.decode('utf-8').replace('/home/jackchuang/kernel_hype/','')
        addr2line = addr2line.decode('utf-8').replace('/home/jackchuang/kh/','') 
        #print ('%s' % addr2line) # dbg
        pa = s[6]
        va = s[7]
        err_code = s[8]
        err_flags1 = s[9]
        err_flags2 = s[10]
        err_flags3 = s[11]
        err_flags4 = s[12]
        print ('%s %16s %s %s %s %s %s %8s %s %s' % (rip, pa, va, err_code, err_flags1, err_flags2, err_flags3, err_flags4, v, addr2line))
        ff.write('%s %16s %s %s %s %s %s %8s %s %s\n' % (rip, pa, va, err_code, err_flags1, err_flags2, err_flags3, err_flags4, v, addr2line))

        #os.system('addr2line -e vmlinux %s' % rip);
        #TMP = subprocess.check_output(['addr2line -e vmlinux %s' % rip])
        #TMP_TRIM = TMP.decode('utf-8').replace('\n','') #py3
        #print ('%s' % TMP_TRIM)
        i += 1
        if i >= top_cnt:
            break



#print ("\n\n\n\n\n\n")
#i=0
## no need to ff.write
#for k, v in vmdsm_census_sorted.items():
#    if v > threshold:
#        print (k, v, flush=True)
#        #print(k.split()[1], '\t', v, '\t', end = '', flush=True) # end = '' no \n #flush=stdout.flush
#        str = k.split()
#        del str[0] # op
#        # cr2>>PG_SHIFT=str[0], rip=[1], rbp=[2]
#        del str[2] # bsp stack ptr
#        # TODO rip
#        for tmp in str:
#            #print ("%s: " % tmp)
#            print ("\t%s: " % (tmp), end = '', flush=True)
#            if hex(int(tmp, 16)) == 0xdddddddddddddddd:
#                print ('!is_valid_rbp')
#            else:
#                if int(tmp, 16) <= int('0x7fffffff0000', 16): # TODO >= 3aff7d / 3a788b630
#                    os.system('addr2line -e dsm_generate %s' % tmp);
#                else:
#                    os.system('addr2line -e vmlinux %s' % tmp);
#
#                #if os.path.exists('vmlinux'):
#                #    os.system('addr2line -e vmlinux %s' % tmp);
#                #else:
#                #    print("", flush=True)
#                #
#             #sys.stdout.flush() why cannot
#            #
#        #
#        i += 1
#        if i >= top_cnt:
#            break
#    #
#
#print ("\n\n\n\n\n\n")
