#! /usr/bin/env python

import commands
import constants
import time

# class/methods for the Digital Loggers, Inc remote AC unit

class remoteAC:

    def __init__(self):
        self.deviceIP = constants.remoteACaddr
        self.numRACs = len(constants.remoteACaddr)
        self.numSockets = constants.remoteACnumSockets
        
    def resetPort(self, deviceIP, socket):
        self.turnOff(deviceIP, socket)
        time.sleep(2)
        self.turnOn(deviceIP, socket)

    def turnOffAll(self, deviceIP):
        for i in range(1,self.numSockets+1):
            self.turnOff(deviceIP, i)

    def turnOnAll(self, deviceIP):
        for i in range(1,self.numSockets+1):
            time.sleep(5)
            self.turnOn(deviceIP, i)
            
    def turnOff(self, deviceIP, socket):
        RACcommand = constants.remoteACcommand
        RACcommand = RACcommand.replace("<ip:port>", deviceIP)
        RACcommand = RACcommand.replace("<login:password>", constants.remoteACloginPW)
        RACcommand = RACcommand.replace("<command>", (str(socket)+"off"))
        response = commands.getoutput(RACcommand)
        return response

    def turnOn(self, deviceIP, socket):
        RACcommand = constants.remoteACcommand
        RACcommand = RACcommand.replace("<ip:port>", deviceIP)
        RACcommand = RACcommand.replace("<login:password>", constants.remoteACloginPW)
        RACcommand = RACcommand.replace("<command>", (str(socket)+"on"))
        response = commands.getoutput(RACcommand)
        return response

    def getStatus(self, deviceIP, socket):
        RACcommand = constants.remoteACcommand
        RACcommand = RACcommand.replace("<ip:port>", deviceIP)
        RACcommand = RACcommand.replace("<login:password>", constants.remoteACloginPW)
        RACcommand = RACcommand.replace("<command>", (str(socket)+"status"))
        print RACcommand
        response = commands.getoutput(RACcommand)
        return response

    
