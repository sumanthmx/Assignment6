#!/usr/bin/python

import os, sys, subprocess
fs_size_bytes = 1048576

# Tests the functionality of lseek
# should not seek too far as this test requires
# should fail
def lseek_fail_test():
    print('*****New Lseek Test*****')
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

# Tests the boundary case when 
# the client attempts to open a non existent
# file in read only mode
def open_fail_test():
    print('*****Open Failure*****')
    issue('mkfs')
    issue('open tempfile 1') # should fail
    print do exit()
    print('***************')
    sys.stdout.flush()

# creates a directory and then attempts to link
def link_and_fail_test():
    print('*****Link Failure*****')
    issue('mkfs')
    issue('mkdir os')
    issue('link os ds') # should fail
    print do exit()
    print('***************')
    sys.stdout.flush()

# attempts to close non existing file / dir
def close_non_existing_test()
    print('*****Close Failure*****')
    issue('mkfs')
    issue('mkdir os')
    issue('open os') 
    issue('close ds') # should fail
    print do exit()
    print('***************')
    sys.stdout.flush()

# attempts to close more than you open
def close_more_than_open_test()
    print('*****Close Failure Again*****')
    issue('mkfs')
    issue('open fun 3')
    issue('open fun 3')
    issue('open fun 3')
    issue('close fun 3')
    issue('close fun 3')
    issue('close fun 3')
    issue('close fun 3') # should fail
    print do exit()
    print('***************')
    sys.stdout.flush()

# attempts to create a file and directory of the same name in the same directory
def multiple_names_test()
    print('****Open Failure****')
    issue('mkfs')
    issue('open fun 3')
    issue('open lessfun 3')
    issue('open morefun 3')
    issue('mkdir fun') # should fail
    print do exit()
    print('***************')
    sys.stdout.flush()