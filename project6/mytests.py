#!/usr/bin/python

import os, sys, subprocess
fs_size_bytes = 1048576

def spawn_lnxsh():
    global p
    p = subprocess.Popen('./lnxsh', shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

def issue(command):
    p.stdin.write(command + '\n')

def check_fs_size():
    fs_size = os.path.getsize('disk')
    if fs_size > fs_size_bytes:
        print "** File System is bigger than it should be (%s) **" %(pretty_size(fs_size))
    else:
        print "Size is fine"

def do_exit():
    issue('exit')
    return p.communicate()[0]

# Tests the boundary case when 
# the client attempts to open a non existent
# file in read only mode
def open_fail_test():
    print('*****Open Failure*****')
    issue('mkfs')
    issue('open tempfile 1') # should fail
    print do_exit()
    print('***************')
    sys.stdout.flush()

# creates a directory and then attempts to link
def link_and_fail_test():
    print('*****Link Failure*****')
    issue('mkfs')
    issue('mkdir os')
    issue('link os ds') # should fail
    print do_exit()
    print('***************')
    sys.stdout.flush()

# attempts to create a file and directory of the same name in the same directory
def multiple_names_test():
    print('****Open Failure****')
    issue('mkfs')
    issue('open fun 3')
    issue('open lessfun 3')
    issue('open morefun 3')
    issue('mkdir fun') # should fail
    print do_exit()
    print('***************')
    sys.stdout.flush()
    
# attempts to remove parent directory
def remove_parent_test():
    print('****Remove Failure****')
    issue('mkfs')
    issue('mkdir directory')
    issue('rmdir ..') # should fail
    print do_exit()
    print('***************')
    sys.stdout.flush()

# attempts to change directories into a file
def change_directory_test():
    print('****Remove Failure****')
    issue('mkfs')
    issue('open randomfile 3')
    issue('mkdir directory') 
    issue('cd randomfile') # should fail
    print do_exit()
    print('***************')
    sys.stdout.flush()
    
print "......Starting my tests\n\n"
sys.stdout.flush()
spawn_lnxsh()
open_fail_test()
spawn_lnxsh()
link_and_fail_test()
spawn_lnxsh()
multiple_names_test()
spawn_lnxsh()
remove_parent_test()
spawn_lnxsh()
change_directory_test()
# Verify that file system hasn't grow too large
check_fs_size()
