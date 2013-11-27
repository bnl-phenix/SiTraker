#!/usr/bin/python

# Logic analyzer diplay of a steam of ascii words

import sys
from matplotlib.widgets import Cursor
#import numpy as np
import matplotlib.pyplot as plt

def mybin(val): # convert integer to 16 binary digits
        rc = [0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0]
        ibit = 1
        for ii in range (0,16):
                if val & ibit !=0 :
                        rc[ii] = 1
                ibit = ibit << 1
        return rc

values = list()
 
sfil = open(sys.argv[1],'r')

#first line is headers
#temps = sfil.readline();
#headers = temps.split()
#for i in range(0, len(headers)):

for i in range(0,16+1):
	values.append(list())

valnum = 0

for line in sfil:
        if line[0] == '#' or line[0] == '$':
                continue
	value = line.split()
	#print 'ln='+str(linnum)+' value=',value
	for i in range(0, len(value)):
                values[0].append(valnum)
                valnum = valnum+1
		#values[i].append(str(float(value[i])*.5+i))
                binbits = mybin(int(value[i],16))
                #print binbits
                for ii in range(0, len(binbits)):
                        values[ii+1].append(str(float(binbits[ii])*.5+ii))
	#for ii in range(0, len(binbits)):
                #print 'values[',ii,']=',values[ii]
        #if valnum > 16:
        #        print 'enough'
        #        #sys.exit()
        #        break
fig = plt.figure(figsize=(16, 8))
fig.suptitle(sys.argv[1],fontsize=12)
fig.subplots_adjust(left=0.02)
fig.subplots_adjust(right=0.98)
fig.subplots_adjust(bottom=0.05)
fig.subplots_adjust(top=0.98)
ax = fig.add_subplot(111, axisbg='#FFFFCC')

#print 'len(values)=',len(values)
#print 'values[0]=',values[0]
#print 'values[4]=',values[4]

for i in range(1, len(values)):
	ax.plot(values[0], values[i], '-')

cursor = Cursor(ax, useblit=True, color='red', linewidth=2 )
 
plt.show()
