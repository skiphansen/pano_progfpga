#! /usr/bin/env python


import commands
import string
import sys
import os
import time

usbTestCommand = './usbtest -t usb -a <ip>:8321 | tee <ip>.tmp'
rmCommand = "rm <ip>.tmp"
#processCommand = 'grep -i "got status" <ip>.tmp | grep -v -i "enoent" | grep -v -i "epipe" | wc -l'
#processCommand = 'grep -i "got status" <ip>.tmp | grep -v -i "enoent" | grep -v -i "epipe" | grep -v -i "eproto" |wc -l'
processCommand = 'egrep -i "port error on port| 500 msec" <ip>.tmp | wc -l'

# handle arguments

if (len(sys.argv) != 2):
    print "Usage: prototest.py <ip>"
    print "TEST FAILED"
    sys.exit(1)

# should do a reg expression to make sure the ip is ok....

pano_ip = sys.argv[1]

#print "Running USB protocol test on %s" % pano_ip
print "Running USB port test on %s" % pano_ip


# kick off the protocol error test... with a grep...feed it into a word count

usbTestCommand = string.replace(usbTestCommand, "<ip>", pano_ip)
rmCommand = string.replace(rmCommand, "<ip>", pano_ip)
processCommand = string.replace(processCommand, "<ip>", pano_ip)

os.system(usbTestCommand)
time.sleep(30)  # extra time to clear out the io buffers, finish tests, etc.
processResult = commands.getoutput(processCommand)

#print "Protocol Errors detected from %s: %s" % (pano_ip, processResult)
print "Port Errors detected from %s: %s" % (pano_ip, processResult)

# check for zeros in the word count

if (string.strip(processResult) == "0"):
    # TEST PASSED on a match of zero
    print "Test has PASSED"
else:
    # TEST FAILED on a match of anything else
    print "Test has FAILED"

os.system(rmCommand)

sys.exit(0)






