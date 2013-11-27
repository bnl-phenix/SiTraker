# -*- coding: utf-8 -*-

#GUI for DSVXTB
#version:       4
#author:        Andrey Sukhanov
#modified:      08/16/2013

#version:	5
#modified:	09/10/2013
# External trigger and DAQ interface controls.
# SVX4 downloading file changed from fem.fem to SVX_config.txt
# Changing between Internal and External trigger will automatically re-load the sequencer 
# using correspondig files: sqn_trig_internal.txt or sqn_trig_external.txt
# Diring Load Sequencer operation the verbosity dropped to avoid lengthy OKs.
#

#version:	6
#modified:	09/11/2013
# The output data file (when RS232 mode is selected) is changed to binary *.dq4 format
# for compatibility with the files, written in USB mode).
# Removed control of FPGA analog subsytem.

#version:	7
#modified:	10/09/2013
# Added Slave CSR configuration

#version:       8
#modified:	10/16/2013
# UART list populated.
# Version and Switches implemented and color coded. Download is color coded.
# The program analises the serial input stream for CSR status, thread.data changed to 16-slot fifo.
# Channel numbers affects the corresponding bits in CSR.

#   2013-10-22  AS
#*  v9 Start DAQ checks if sequencer was loaded

#   2013-10-29  AS
#*  v10 Checkbox Pedestal removed, Calibration added. CN color turned red when Channel Numbers released
#Per-line handshaking with DAQ is introduced for RS232 interface.
#The Record separator is changed from '$' to '\x1E'
#The DAQ is sending acknowledge '\x6' for each line
#This version works for dsvxtb-v119-105 and higher.

#  2013-10-30	AS
#* Filenames and writing progress in information window.
version = 11

#   2013-11-01  AS
#*  Correct the setting of number of modules to read in Start DAQ
version = 12

import sys
#from time import sleep
from PyQt4 import QtCore, QtGui
import serial
from collections import deque
#
import thread
import datetime
import os
import time
import random
import platform
import glob
from array import array

data_directory = "../../data/"
slave_reset = 3
slave_work = 4

try:
    from dsvxtb_ui import Ui_dsvxtb
except:
    print("dsvxtb_ui.pyc does not exist, to generate it use: \npyuic4 dsvxtb.ui -o dsvxtb_ui.py")
    sys.exit()

# Any dynamic configurations assumed to be in dsvxtb_config.
# It is not used in version 3 or above
#import dsvxtb_config
#g = dsvxtb_config.Config()
#myCOMPort = g.Run_COM_Port

def reverseBits(original,numbits):
        return sum(1<<(numbits-1-i) for i in range(numbits) if original>>i&1)

class SleepPrint(QtCore.QThread):
        procDone = QtCore.pyqtSignal(bool)
        line = QtCore.pyqtSignal(str)
        procIter = QtCore.pyqtSignal(int)
        chipskop_in_progress = 0
	# data will store last 16 lines
        data = deque()
        for ii in range(16):
            data.append('')
        sfil = None
        def run(self):
	    print('SleepPrint started')
	    while myapp.connected:
 		txt = myapp.ser.readline()
		if len(txt) == 0:
			time.sleep(0.1)
			continue
		#print('sleep: '+txt)
       		drop = self.data.popleft()
		self.data.append(txt)
		#if txt[0] == '$':      #chipskop data
		if txt[0] == '\x1E':      #chipskop data
		    #print("streaming out \'"+txt+"\'")
		    if txt[1] == 'E':	# DAQ data
                        try:
                            elapsed_sec = time.time() - start
                        except:
                            pass
			try:
			    if not self.sfil.closed:
                                fpos = self.sfil.tell()
                                txtout = 'Closed '+self.sfil.name+'['+str(fpos)+'] after '+str(round(elapsed_sec,1))+'s'
				print(txtout)
				self.line.emit(txtout)
				self.sfil.close()
			except:
                            print('ERROR Data for closed file:'+txt[1:])
			    continue
			# for windows
			#os.system('start /B python waveplot_logic_hex.py '+self.sfil.name)
			#linux equivalent:
			if self.chipskop_in_progress == 1:	# launch the command to plot the chipskop data
			    os.system('python waveplot_logic_hex.py '+self.sfil.name+'&')
			elif self.chipskop_in_progress == 2:
			    notification_file = open(data_directory+"daqcapture.dq0",'w')
			    notification_file.write(self.sfilname)
			    notification_file.close()
			self.chipskop_in_progress = 0
			continue
		    elif self.chipskop_in_progress == 0:
			if txt[2] =='C':	# chipskop data
			    self.chipskop_in_progress = 1
			    self.sfil = open(datetime.datetime.today().strftime(data_directory+"wpl_%y%m%d%H%M.wpl"),'w')
			elif txt[2] =='D':
			    self.chipskop_in_progress = 2
			    self.sfilname = datetime.datetime.today().strftime("%y%m%d%H%M.dq4")
			    self.sfil = open(data_directory+self.sfilname,'wb')
			if not self.sfil.closed:
                            txtout = 'Opened '+self.sfil.name
			    print(txtout)
			    self.line.emit(txtout)
			    start = time.time()
			    olddifftime = 0
		    else:
			if txt[1] == '$':	#skip the comment
			    continue
			if self.chipskop_in_progress == 2:
                            #DAQ data, write them to file
			    myapp.ser.write('\6')  # send back acknowledge
			    ar = array('B')
			    #print(int(len(txt)-2)/2)
			    try:
			      for ii in range((len(txt)-2)/2):
				  ar.append(int(txt[ii*2+1:ii*2+3],16))
			    except:
			      pass
			    ar.tofile(self.sfil)
                            difftime = int((time.time() - start))
                            if difftime/10 != olddifftime/10:
                                txtout = str(difftime)+'s: bytes out: '+str(round(self.sfil.tell()/1000.,1))+'kB'
                                self.line.emit(txtout)
                            olddifftime = difftime
 			else:
			    self.sfil.write(txt[1:])
		#elif txt[0] == 'V':
		#    #print('V: '+txt.rstrip())
		#    myapp.analogFile.write(txt.rstrip()+'\n')
		#    myapp.analogFile.flush()
		else:
		    self.line.emit(txt.rstrip())
	    print('SleepPrint ended')        
        
def list_serial_ports():
    system_name = platform.system()
    if system_name == "Windows":
        # Scan for available ports.
        available = []
        for i in range(256):
            try:
                s = serial.Serial(i)
                #print('Port '+s.name)
                available.append(s.name)
                s.close()
            except serial.SerialException:
		pass
        return available
    elif system_name == "Darwin":
        # Mac
        return glob.glob('/dev/tty*') + glob.glob('/dev/cu*')
    else:
        # Assume Linux or something else
        return glob.glob('/dev/ttyUSB*') # + glob.glob('/dev/ttyS*')

class myControl(QtGui.QMainWindow):
    #helper functions
    def scmd(self,cmd):
	print("sending "+"\'"+str(cmd)+"\'")
	self.ser.write(str(cmd))

    def connect(self):
        self.updateTxt("GUI v"+str(version))
        self.ser = serial.Serial()
        #self.ser.baudrate = g.Run_COM_BaudRate
        self.ser.baudrate = int(self.ui.my_COM_BAUD.currentText())        
        # no effect #self.ser.xonxoff = True
        # no effect #self.ser.writeTimeout = 1
        
        #IMPORTANT setting for serial data taking,
        #0.01 is too small for windows7
        #0.1 is good for 460800 bod, data taking rate is 60 ev/s
        #0.5 is good for 115200 bod as well as for 460800, data taking rate is 60 ev/s
        self.ser.timeout = 0.5
        
        portsList = list_serial_ports()
        #self.ser.port = portsList[len(portsList)-1]
        #print('port: '+self.ui.my_COM.currentText())
        self.ser.port = str(self.ui.my_COM.currentText())
        try:
	    self.ser.open()
        except serial.serialutil.SerialException:
            print(("ERROR: could not open port " + self.ser.port))
            #trying to call #serial.tools.list_ports()
            #os.system("python -m serial.tools.list_ports")
            #sys.exit()
        if self.ser.isOpen():
	    self.connected = True
	    print('Serial port ' +  str(self.ser.name) + ' opened')
	    print('timeout:'+str(self.ser.timeout)+' xonxoff:'+str(self.ser.xonxoff)+' rtscts:'
                  +str(self.ser.rtscts)+' CharTimeout: '+str(self.ser.interCharTimeout)+' WTimeOut='+str(self.ser.writeTimeout))
	    self.ser.flushInput();
	    self.scmd("s")	#print status
	else:
	    print('ERROR. Could not open serial port ' + str(self.ser.port))
	    #exit()
	
    def analogOpen(self):
                self.analogFile = open('analog.log','a')
                self.analogFile.write('New\n')
        
    # GUI stuff
    def __init__(self, parent=None):
        QtGui.QWidget.__init__(self, parent)
        self.ui = Ui_dsvxtb()
        self.ui.setupUi(self)
        #
        # Class variables
        self.exiting = 0
        self.greading_stopped = True
        self.thread = SleepPrint()
        self.wtext = self.thread.data
        self.connected = False
        self.Verbosity = 0
        self.lastDACCmd = 0
        self.ramped = 0
	
        # Init list of serial ports
	portsList = list_serial_ports()
	print("Available ports: "+str(portsList))
	for ii in range(len(portsList)):
            self.ui.my_COM.insertItem(ii,portsList[ii])
        self.ui.my_COM.setCurrentIndex(len(portsList)-1)

        # Init the GUI items
        self.ui.my_COM_BAUD.insertItem(0,'4800')
        self.ui.my_COM_BAUD.insertItem(1,'9600')
        self.ui.my_COM_BAUD.insertItem(2,'19200')
        self.ui.my_COM_BAUD.insertItem(3,'38400')
        self.ui.my_COM_BAUD.insertItem(4,'57600')
        self.ui.my_COM_BAUD.insertItem(5,'115200')
        self.ui.my_COM_BAUD.insertItem(6,'230400')
        self.ui.my_COM_BAUD.insertItem(7,'460800')
        self.ui.my_COM_BAUD.insertItem(8,'921600')
        self.ui.my_COM_BAUD.setCurrentIndex(7)
        
        # Fill the Interface items
        self.ui.my_DAQ_Interface.insertItem(0,'RS232')
        self.ui.my_DAQ_Interface.insertItem(1,'USB')
        self.ui.my_DAQ_Interface.setCurrentIndex(0)
        
        #self.analogOpen()
        self.connect()
        if self.ser.isOpen():
                        self.ui.my_Connect.setCheckState(QtCore.Qt.Checked)
                        #self.reg_Write(2,0xF0000000)    # clear bits of pulsed command field to avoid confusion of pulsing bit which is already set
    # Connect the buttons
        QtCore.QObject.connect(self.ui.my_Run_Exit,QtCore.SIGNAL("clicked()"),self.my_Run_Exit)
        QtCore.QObject.connect(self.ui.my_Run_Status,QtCore.SIGNAL("clicked()"),self.my_Run_Status)
        QtCore.QObject.connect(self.ui.my_Run_Command,QtCore.SIGNAL("clicked()"),self.my_Run_Command)
        QtCore.QObject.connect(self.ui.my_Run_Command_Txt,QtCore.SIGNAL("returnPressed()"),self.my_Run_Command)
        QtCore.QObject.connect(self.ui.my_Run_Command_Repeat,QtCore.SIGNAL("clicked()"),self.my_Run_Command_Repeat)
        QtCore.QObject.connect(self.ui.my_Run_Command_Recording,QtCore.SIGNAL("released()"),self.my_Run_Command_Recording)
        QtCore.QObject.connect(self.ui.my_Run_EFROM,QtCore.SIGNAL("clicked()"),self.my_Run_EFROM)
        QtCore.QObject.connect(self.ui.my_Run_Reg_Read,QtCore.SIGNAL("clicked()"),self.my_Run_Reg_Read)
        QtCore.QObject.connect(self.ui.my_Run_Reg_Input,QtCore.SIGNAL("clicked()"),self.my_Run_Reg_Input)
        QtCore.QObject.connect(self.ui.my_Run_Reg_Set,QtCore.SIGNAL("clicked()"),self.my_Run_Reg_Set)
        QtCore.QObject.connect(self.ui.my_Run_Reg_Clear,QtCore.SIGNAL("clicked()"),self.my_Run_Reg_Clear)
        QtCore.QObject.connect(self.ui.my_Run_Reg_Pulse,QtCore.SIGNAL("clicked()"),self.my_Run_Reg_Pulse)
        QtCore.QObject.connect(self.ui.my_Run_Reg_Add,QtCore.SIGNAL("clicked()"),self.my_Run_Reg_Add)
        QtCore.QObject.connect(self.ui.my_Dbg_Verbosity,QtCore.SIGNAL("clicked()"),self.my_Dbg_Verbosity)
        QtCore.QObject.connect(self.ui.my_Dbg_Verbosity_Txt,QtCore.SIGNAL("returnPressed()"),self.my_Dbg_Verbosity)
        QtCore.QObject.connect(self.ui.my_Run_Stop,QtCore.SIGNAL("clicked()"),self.my_Run_Stop)
        QtCore.QObject.connect(self.ui.my_Run_Delay,QtCore.SIGNAL("clicked()"),self.my_Run_Delay)
        QtCore.QObject.connect(self.ui.my_Run_Chipskop,QtCore.SIGNAL("clicked()"),self.my_Run_Chipskop)
        QtCore.QObject.connect(self.ui.my_Run_Chipskop_Arm,QtCore.SIGNAL("clicked()"),self.my_Run_Chipskop_Arm)
        QtCore.QObject.connect(self.ui.my_Run_Chipskop_Trig,QtCore.SIGNAL("clicked()"),self.my_Run_Chipskop_Trig)
        QtCore.QObject.connect(self.ui.my_Connect,QtCore.SIGNAL("released()"),self.my_Connect)
        QtCore.QObject.connect(self.ui.my_CTest,QtCore.SIGNAL("clicked()"),self.my_CTest)
        QtCore.QObject.connect(self.ui.my_STest,QtCore.SIGNAL("clicked()"),self.my_STest)
        QtCore.QObject.connect(self.ui.my_Download,QtCore.SIGNAL("clicked()"),self.my_Download)
        QtCore.QObject.connect(self.ui.my_DAQ_Start,QtCore.SIGNAL("clicked()"),self.my_DAQ_Start)
        QtCore.QObject.connect(self.ui.my_DAQ_Stop,QtCore.SIGNAL("clicked()"),self.my_DAQ_Stop)
        QtCore.QObject.connect(self.ui.my_Reset,QtCore.SIGNAL("clicked()"),self.my_Reset)
        QtCore.QObject.connect(self.ui.my_Help,QtCore.SIGNAL("clicked()"),self.my_Help)
        QtCore.QObject.connect(self.ui.my_Load_Sequencer,QtCore.SIGNAL("clicked()"),self.my_Load_Sequencer)
        QtCore.QObject.connect(self.ui.my_DAQ_ExTrig,QtCore.SIGNAL("released()"),self.my_DAQ_ExTrig)
        QtCore.QObject.connect(self.ui.my_Calibration,QtCore.SIGNAL("released()"),self.my_Calibration)
        QtCore.QObject.connect(self.ui.my_Channel_Numbers,QtCore.SIGNAL("released()"),self.my_Channel_Numbers)
        QtCore.QObject.connect(self.ui.my_DAQ_Interface,QtCore.SIGNAL("activated(QString)"),self.my_DAQ_Interface)
        QtCore.QObject.connect(self.ui.my_Slave_CSR,QtCore.SIGNAL("clicked()"),self.my_Slave_CSR)
        QtCore.QObject.connect(self.ui.my_Slave_Reset,QtCore.SIGNAL("clicked()"),self.my_Slave_Reset)
        
        # Start threads
        #thread.start_new_threAAad(threadFeedBack,("FB",""))
        self.thread.line.connect(self.updateTxt)
        #self.thread.procDone.connect(self.fin)
        #self.thread.procIter.connect(self.changeWindowTitle)
        self.thread.start()
        
    # Slots
    def my_Run_Exit(self):
        # safe exiting
        self.exiting = 1
        sys.exit(app.exec_())
        
    def my_Run_Status(self):
        self.scmd("s")
        
    def my_Run_Command(self):
        line = self.ui.my_Run_Command_Txt.text()
        line += ' ' #to easy split
        words = line.split(' ')
        try:
            i = int(str(words[1]),0)
            words[1] = str(i)
        except:
            pass
        line = words[0] + ' ' + words[1] + ' ' #client needs separator at the end
        self.scmd(str(line)) 
        
    def my_Run_Command_Repeat(self):
	ntimes = self.ui.my_Run_Command_Repeat_NTimes.text()
	line = "p"+ntimes+' '
	self.scmd(str(line))
                
    def my_Run_Command_Recording(self):
        if self.ui.my_Run_Command_Recording.checkState():
            self.ui.my_Run_Command_Repeat.setEnabled(False)
            self.scmd("[")
        else:
            self.ui.my_Run_Command_Repeat.setEnabled(True)
            self.scmd("]")
            
    def my_Run_EFROM(self):
	self.scmd("e")
                
    def my_Run_Reg_Reader(self,reg):
	line = "r"+str(int(self.ui.my_Run_Reg.text())+reg)+' '
	self.scmd(line)
                
    def my_Run_Reg_Read(self):
        self.my_Run_Reg_Reader(0)
                
    def my_Run_Reg_Input(self):
        self.my_Run_Reg_Reader(1)
                
    def reg_Write(self, reg, val):
        line = "w"+str(reg)+' '+str(val)+' '
        self.scmd(line)
                
    def my_Run_Reg_Writer(self,reg):
        reg = int(self.ui.my_Run_Reg.text())+reg
        val = int(str(self.ui.my_Run_Reg_Value.text()),0)
        self.reg_Write(reg,val)
                
    def my_Run_Reg_Write(self):
        self.my_Run_Reg_Writer(0) # avoid using it, use bit sets and bit clears instead 
                
    def my_Run_Reg_Set(self):
        self.my_Run_Reg_Writer(1)
                
    def my_Run_Reg_Clear(self):
        self.my_Run_Reg_Writer(2)
                
    def my_Run_Reg_Pulse(self):
        self.my_Run_Reg_Writer(0)
                
    def my_Run_Reg_Add(self):
	reg = self.ui.my_Run_Reg.text()
	val = int(str(self.ui.my_Run_Reg_Value.text()),0)
	line = "a"+reg+' '+str(val)+' '
	self.scmd(line)
                
    def my_Dbg_Verbosity(self):
	self.Verbosity = int(str(self.ui.my_Dbg_Verbosity_Txt.text()),0)
	line = "l"+str(self.Verbosity)+' '
	self.scmd(line)
                
    def my_Run_Stop(self):
        self.scmd("xx")# double x will stop repeating process
                
    def my_Run_Delay(self):
	reg = int(float(str(self.ui.my_Run_Delay_Txt.text()))*1000.)
	line = "d"+str(reg)+' '
	self.scmd(line)
                
    def my_Run_Chipskop_Arm(self):
        self.reg_Write(0,0x20000000)  # pulse bit mask
        self.ui.my_Run_Chipskop.setEnabled(True)
        
    def my_Run_Chipskop_Trig(self):
                self.reg_Write(0,0x00000200)   # pulse bit mask
                
    def my_Run_Chipskop(self):
        self.scmd('c')
        self.ui.my_Run_Chipskop.setEnabled(False)
                              
    def my_Connect(self):
	if self.ui.my_Connect.checkState():
	    self.connect()
	    if self.ser.isOpen():
			self.thread.start()
	else:
	    self.connected = False
	    self.ser.close()
	    print('serial connection closed')
                        
    def updateTxt(self,txtt):
        self.ui.my_Run_OutputWindow.append(txtt)

    def my_CTest(self):
	self.scmd('t0 0')
	
    def my_STest(self):
	self.scmd('t0 1')
	
    def my_Download(self):
	dwnl_file = open('SVX_config.txt','r')
        print(('Opened '+dwnl_file.name))
        string = '0b'
        for line in dwnl_file:
	    if line[0] != '#':
		#print(line)
		string += line
	ostr = string.translate(None,' \r\n') #concatenate all strings
	num = int(ostr,2)
	ostr = hex(num)
	ostr = ostr[2:]
	ostr = ostr.translate(None,'L') #remove trailing 'L'
	ostr = ostr.rjust(48,'0') #recover leading zeroes
	#if len(ostr) != 48:
	#    print("ERROR, Length of the configuration string != 192.")
	#    print('['+str(len(ostr))+"]: \'"+ostr+"\'")
	#    return -1
	nSVX = str(self.ui.my_NSVX4.text())
	ostr = 'o'+nSVX+' '+ostr+' ' #extra space at the end is necessary
        self.scmd(ostr)
	dwnl_file.close()
	time.sleep(0.1)  # wait until last message from board was received
	#print('Response: '+self.wtext[len(self.wtext)-1]+'|')
	
	lastline = self.wtext[len(self.wtext)-1]
	if  lastline[:2] == 'OK' or lastline[:3] == 'Cal': #CalStrobe sent\r\n':
            self.ui.my_Download.setStyleSheet("QPushButton { background-color : rgb(128,255,128); color : black; }")
            txtx='Download OK'
        else:
            self.ui.my_Download.setStyleSheet("QPushButton { background-color : rgb(255,100,100); color : black; }")
            txtx='Download FAILED'
        self.updateTxt('host: '+txtx)

    def my_DAQ_Start(self):
        # check if sequenser was loaded
	self.my_Reset()
	self.scmd('r320 1 ')
	self.scmd('r320 1 ')
	time.sleep(0.1)
	val = 0
	nn = 1
	if self.wtext[len(self.wtext)-1][:2] == 'OK':
            nn = 2
	try:
            val = int(self.wtext[len(self.wtext)-nn].split()[2],0)
        except:
            pass
	if val != 0x20:
            print('Sequenser not loaded')
            self.ui.my_Load_Sequencer.setStyleSheet("QPushButton { background-color : rgb(255,100,100); color : black; }")
            # it is enough to turn it redi
            #return
	
	self.ui.my_DAQ_ExTrig.setEnabled(False)
	#self.ui.my_Channel_Numbers.setEnabled(False)
	self.ui.my_DAQ_Interface.setEnabled(False)
	self.ui.my_Download.setEnabled(False)
	#self.ui.my_Load_Sequencer.setEnabled(False)
	self.my_Reset()	# to reset event number
	ostr = "q"
	ostr += str(int(float(str(self.ui.my_DAQ_NEvents.text()))))
	mode = 0
	if self.ui.my_Retrigger.checkState():
	    mode |= 1
	if self.ui.my_DAQ_Simulate.checkState():
	    print("Data will be simulated!")
	    mode |= 2
	if (self.ui.my_DAQ_Interface.currentText() == 'USB'):
	    mode |= 4
	if self.ui.my_Channel_Numbers.checkState():
	    mode |= 8
	if self.ui.my_DAQ_ExTrig.checkState():
	    mode |= 0x10
	if self.ui.my_DAQ_Handshaking.checkState():
	    mode |= 0x20
	# set the number of modules to read
	nMods = int(self.ui.my_NSVX4.text())
	print('DAQ will read '+str(nMods)+' SVX4s')
	# set nMods 	
	# for DSVXTB version prior to v120
	#nMods = (nMods>>1)&0x7
	#self.ser.write('w2 '+str(0x7<<12)+' ')  # clear nMods bits
	#self.ser.write('w1 '+str(nMods<<12)+' ') # set new nMods bits
	# for DSVXTB version v120 and later
	self.ser.write('w64 ' + str((nMods<<4)|slave_work) + ' ')
	#
	ostr += " " + str(mode) + " "
        self.scmd(ostr)
        
    def my_DAQ_Stop(self):
	self.ui.my_DAQ_ExTrig.setEnabled(True)
	self.ui.my_Channel_Numbers.setEnabled(True)
	self.ui.my_DAQ_Interface.setEnabled(True)
	self.ui.my_Download.setEnabled(True)
	self.ui.my_Load_Sequencer.setEnabled(True)
	mode = 0x8000
	ostr = "q0 " + str(mode) + " "
        self.scmd(ostr)
        
    def my_Reset(self):
	self.scmd('w0 0x10000000 ')
	
    def my_Help(self):
	self.scmd('h ')
	
    def my_Load_Sequencer(self):
	self.scmd("l3 ")	#change verbosity to minimal
	self.my_Reset()
	if self.ui.my_DAQ_ExTrig.checkState():
	    seq_file = open('sqn_trig_external.txt','r')
	else:
	    if self.ui.my_Calibration.checkState():
	      seq_file = open('sqn_calibration.txt','r')
	    else:
	      seq_file = open('sqn_pedestals.txt','r')
        print(('Opened '+seq_file.name))
        for line in seq_file:
	    if line[0] == '#':
		continue
	    ostr = 'w320 '+line.split()[0]+' '
	    self.scmd(ostr)
	seq_file.close()
	self.my_Dbg_Verbosity()	# recover verbosity
        self.ui.my_Load_Sequencer.setStyleSheet("QPushButton { background-color : rgb(128,255,128); color : black; }")
	
    def my_DAQ_Interface(self):
	if (self.ui.my_DAQ_Interface.currentText() == 'USB'):
	    print("Data destination interface changed to USB")
	    self.ui.my_DAQ_NEvents.setEnabled(False)
 	    #self.ui.my_DAQ_Writing.setEnabled(False)
 	else:
	    print("Data destination interface changed to RS232")
	    self.ui.my_DAQ_NEvents.setEnabled(True)
 	    #self.ui.my_DAQ_Writing.setEnabled(True)
 	    
    def my_DAQ_ExTrig(self):
	self.my_Load_Sequencer()

    def my_Calibration(self):
	self.my_Load_Sequencer()

    def my_Channel_Numbers(self):
	self.my_Load_Sequencer()
        self.ui.my_CN.setStyleSheet("QLabel { background-color : rgb(255,100,100); color : black; }")

    def my_Slave_CSR(self):
	self.scmd("l3 ")	#change verbosity to minimal
	self.ser.write('w2 0xff ')	#clear CSR0[7:0]
	self.ser.write('w1 0x100 ') #set immediate command mode
	reg = int(str(self.ui.my_SlaveCSR.text()),0)
	if self.ui.my_Channel_Numbers.checkState():
            reg |= 0x40 # prepare to set bit6 in slave CSR
	else:
            reg &= ~0x40
        print('Setting slave CSR to '+hex(reg))
	val = reg
	#print('Conf='+str(reg))
	# sequence to get CSR bits, represented as bit07 in 'r64'
	for ii in range(16):
	    if ii>7:
		# shift out the configuration value
		if (val&0x80) != 0:
		    self.ser.write('w1 4 ')	# to set PRIN
		else:
		    self.ser.write('w2 4 ')	# to clear PRIN	
		val = val<<1;
	    time.sleep(.001)
	    self.ser.write('w0 8 ')	#pulse FECLK
	    time.sleep(.001)
	    self.ser.write('r64 1 ')

	# pack bit07 from last 16 reading into the CSR
	time.sleep(0.1)  # wait until last message from board was received
	val = 0
	for ii in range(16):
		line = self.wtext[ii]
		#print(str(ii)+': '+line)
		val <<= 1
		try:
			if int(line.split()[2],0)&0x8000 != 0:
				val |= 1
		except:
			continue
	version = val>>8
	switches = val&0x3f
	print('CSR='+str(hex(val))+' vers:'+str(version)+' switches:'+bin(switches))
	self.ui.my_Label_Version.setText(hex(version))
	readCN = (val>>6)&1
	self.ui.my_CN.setText(str(readCN))
	#source = self.sender()
	#print(str(source))
	txtx = ''
	if (version == 0xFF) or (version == 0):
            self.ui.my_Label_Version.setStyleSheet("QLabel { background-color : rgb(255,100,100); color : black; }")
            txtx = 'ERROR. Slave not powered or disconnected?'
        else:
            self.ui.my_Label_Version.setStyleSheet("QLabel { background-color : rgb(128,255,128); color : black; }")
        if val&0xff != reg&0xff:
            self.ui.my_Label_Switches.setStyleSheet("QLabel { background-color : rgb(255,100,100); color : black; }")
            txtx='ERROR. Token passing with slaves is broken, check connection and jumpers JP4'
        else:
            self.ui.my_Label_Switches.setStyleSheet("QLabel { background-color : rgb(128,255,128); color : black; }")
        print('CN,ChN='+str(readCN==1)+','+str(self.ui.my_Channel_Numbers.checkState()))
        if (readCN==1) == (self.ui.my_Channel_Numbers.checkState()!=0):
            self.ui.my_CN.setStyleSheet("QLabel { background-color : rgb(128,255,128); color : black; }")
        else:
            self.ui.my_CN.setStyleSheet("QLabel { background-color : rgb(255,100,100); color : black; }")

        if len(txtx) >0 :
            time.sleep(.1)
            print(txtx)
            self.updateTxt('host: '+txtx) #this will appear before the printout of 'r64 '
	self.ui.my_Label_Switches.setText(bin(switches))
	self.my_Dbg_Verbosity()	# recover verbosity

    def my_Slave_Reset(self):
	self.reg_Write(64,slave_reset)	#Shorten the ROC_BCLK to reset the slave, 
	self.reg_Write(64,slave_work)	#Recover the ROC_BCLK
	    
   #Other methods
    def isExiting(self):
        return self.exiting
        
    def reading_stopped(self):
                return self.greading_stopped
    
if __name__ == "__main__":
    print('Running dsvxtb.py as application.')
    #print 'sys.argv['+str(len(sys.argv))+']='+sys.argv[0]
    #if len(sys.argv)>1:
        #        myCOMPort = sys.argv[1]
        #        print 'port='+myCOMPort
    app = QtGui.QApplication(sys.argv)
    myapp = myControl()
    myapp.show()
    sys.exit(app.exec_())
