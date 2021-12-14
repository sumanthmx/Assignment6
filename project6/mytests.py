#!/usr/bin/python

import os, sys, subprocess
fs_size_bytes = 1048576

# Tests the functionality of lseek
# should not seek too far as this test requires
# should fail
def lseek_fail_test():
    print('*****Lseek Test*****')
    issue('mkfs')
    issue('open testf 3')
    issue('write 0 cos318rules')
    issue('lseek 0 30000000000000')
    issue('read 0 3') # fail here
    issue('close 0')
    issue('cat testf')
    print do_exit()
    print('***************')
    sys.stdout.flush()