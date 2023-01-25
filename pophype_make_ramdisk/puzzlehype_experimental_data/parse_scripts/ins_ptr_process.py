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
from collections import OrderedDict

CENSUS=sys.argv[1]
APP_SOURCE_CODE=sys.argv[2]
print '\t ==================================================== \t'
print '\t\t' + sys.argv[0]+ ' ' + sys.argv[1] + ' ' + sys.argv[2]
print '\t\t' + sys.argv[0]+ ' ' + sys.argv[1] + ' ' + sys.argv[2]
print '\t\t' + sys.argv[0]+ ' ' + sys.argv[1] + ' ' + sys.argv[2]
print '\t ==================================================== \t'

if len(sys.argv) < 2:
    sys.exit("Usage: %s <source_code_compiled_w/_-g>" % (sys.argv[0]))
    
if not os.path.isfile(APP_SOURCE_CODE):
    sys.exit("source_code not exit")

print("importing data from %s" % CENSUS[:-3])
census = __import__("%s" % CENSUS[:-3])
ins_census = census

ins_census_sorted = OrderedDict((k, v) for k, v in sorted(ins_census.AllData.items(), key=lambda x: x[1], reverse=True))

# Usage: ./final.py | sort -n -k 3
i = 0
threshold = 10
top_cnt = 20
print '=== threshold [[' + str(threshold) + ']], ' + 'list top [[' + str(top_cnt) + ']] pgfaults ==='
sys.stdout.flush()
for k, v in ins_census_sorted.items():
    if v > threshold: 
        print('%s %s - ' % (k, v) ),
        sys.stdout.flush()
        s = k.split(' ')
        os.system('addr2line %s -e %s' % (s[0], APP_SOURCE_CODE)) # % (a, b, c, ...)
        i += 1
        if i >= 20:
            break

