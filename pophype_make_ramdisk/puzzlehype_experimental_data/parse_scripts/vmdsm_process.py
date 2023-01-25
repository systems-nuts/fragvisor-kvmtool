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
print '=== threshold [[' + str(threshold) + ']], ' + 'list top [[' + str(top_cnt) + ']] vmdsm pgfaults ==='
sys.stdout.flush()
i=0
for k, v in vmdsm_census_sorted.items():
    if v > threshold:
        print k, v
        i += 1
        if i >= top_cnt:
            break

