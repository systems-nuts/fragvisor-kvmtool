#! /usr/bin/env python
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright © 2020 jackchuang <jackchuang@mir>
#
# Distributed under terms of the MIT license.
import sys, os, pprint
import subprocess
from collections import OrderedDict
"""

"""


#arg = ''
#ret_date = os.popen('date%s' % arg).read()
ret_date = os.popen('date').read()
ret_date = ret_date.encode().decode('utf-8').replace('\n','') # 'str' object has no attribute 'decode' BUG => SOL: .encode()
# order matters
ret_date = ret_date.encode().decode('utf-8').replace(':','_')
print (ret_date)
print ('')


print ('sol:')
os.system('date');
