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
protoGrepCommand = 'egrep -i "port error on port| 500 msec" <ip>.tmp | wc -l'
mouseGrepCommand = 'grep -i "found mouse" <ip>.tmp | wc -l'
keyboardGrepCommand = 'grep -i "found keyboard" <ip>.tmp | wc -l'
flashGrepCommand = 'grep -i " Unknown device with address" <ip>.tmp | wc -l'


# handle arguments

if (len(sys.argv) != 2):
    print "Usage: prototest.py <ip>"
    print "TEST FAILED"
    sys.exit(1)

# should do a reg expression to make sure the ip is ok....

pano_ip = sys.argv[1]

#print "Running USB protocol test on %s" % pano_ip
print "Running all USB tests on %s" % pano_ip


# kick off the protocol error test... with a grep...feed it into a word count

usbTestCommand = string.replace(usbTestCommand, "<ip>", pano_ip)
rmCommand = string.replace(rmCommand, "<ip>", pano_ip)
protoGrepCommand = string.replace(protoGrepCommand, "<ip>", pano_ip)
mouseGrepCommand = string.replace(mouseGrepCommand, "<ip>", pano_ip)
keyboardGrepCommand = string.replace(keyboardGrepCommand, "<ip>", pano_ip)
flashGrepCommand = string.replace(flashGrepCommand, "<ip>", pano_ip)

os.system(usbTestCommand)
time.sleep(30)  # extra time to clear out the io buffers, finish tests, etc.
protoResult = commands.getoutput(protoGrepCommand)
mouseResult = commands.getoutput(mouseGrepCommand)
keyboardResult = commands.getoutput(keyboardGrepCommand)
flashResult = commands.getoutput(flashGrepCommand)



#print "Protocol Errors detected from %s: %s" % (pano_ip, processResult)
print "Port Errors detected from %s: %s" % (pano_ip, protoResult)

mouseDetected = 0
keyboardDetected = 0
flashDetected = 0

if (int(mouseResult) > 0):
    print "Mouse Detected"
    mouseDetected = 1
if (int(keyboardResult) > 0):
    print "Keyboard Detected"
    keyboardDetected = 1
if (int(flashResult) > 0):
    print "USB Memory Detected"
    flashDetected = 1

# check for zeros in the word count

if (string.strip(protoResult) == "0" and mouseDetected == 1 and keyboardDetected == 1 and flashDetected == 1):
    # TEST PASSED on a match of zero
    print "Test has PASSED"
else:
    # TEST FAILED on a match of anything else
    print "Test has FAILED"

os.system(rmCommand)

sys.exit(0)






