# -*- coding: utf-8 -*-
"""
Created on Wed Sep 26 16:27:10 2018

@author: Joseph

Single-reading spectrum fitter/peak detector.

EXPECTS TO RECEIVE DATA ALREADY TRUNCATED TO .98 WINDOW.
DO THIS IN C BEFORE CALLING!!
"""

#!/usr/bin/python3
import numpy as np
import matplotlib.pyplot as plt
from scipy.optimize import curve_fit

#FILENAME TO FIT:
filename = './raw_data.txt'

#CUSTOM FIT PARAMETERS:
#stepsize to use for gaussian fit within window:
stepsize = 1
#number of data points on each side for side-point fitting:
numSidePoints = 10


def gaussian(x,a1,c1,w1):
    return a1 * np.exp(-(x - c1)**2 /w1)
    
def myGaussFit(wavelengths,intensities,stepsize = 1,):
    #generate our guess:
    peak_height = max(intensities)
    peak_index = np.where(intensities == peak_height)[0]
    peak_wavelength = wavelengths[peak_index]
    peak_width = 100
    
    guess = [peak_height, peak_wavelength, peak_width]  
    
    #fit the curve or return raw if failure
    try:
        p,pcov = curve_fit(gaussian, wavelengths[::stepsize], intensities[::stepsize], p0=guess)
        retVal =  gaussian(wavelengths, *p)
    except RuntimeError:
        print('error! returning raw')
        retVal = intensities
        
    return retVal

def myGaussFit_side(wavelengths,intensities,numSidePoints= 10):
    #generate our guess:
    peak_height = max(intensities)
    peak_index = np.where(intensities == peak_height)
    peak_wavelength = wavelengths[peak_index[0][0]]
    peak_width = 100
    
    #this should be a good guess:
    guess = [peak_height, peak_wavelength, peak_width]  
    
    #now take only the number of points from each side.
    #note, in python you can use a negative array index to count from the end!
    xData = wavelengths[:numSidePoints] #get leftmost points
    xData = np.append(xData,wavelengths[-numSidePoints:]) #get rightmost points
    
    yData = intensities[:numSidePoints] #get leftmost points
    yData = np.append(yData,intensities[-numSidePoints:]) #get rightmost points
    
    #fit the curve or return raw if failure
    try:
        p,pcov = curve_fit(gaussian, xData, yData, p0=guess)
        return gaussian(wavelengths, *p)
    except RuntimeError:
        print('error! returning raw')
        return intensities

#script starts here:
wavelengths = np.array([])
raw_intensities = np.array([])

inFile = open(filename,'r')
for row in inFile:
    columns = row.split()
    wavelengths = np.append(wavelengths,eval(columns[0]))
    raw_intensities = np.append(raw_intensities,eval(columns[1]))

inFile.close()

#uncomment this to normalize before fitting: 
#raw_intensities = raw_intensities / max(raw_intensities)

fitted = myGaussFit(wavelengths,raw_intensities)
#fitted = myGaussFit_side(wavelengths,raw_intensities)

#normalize and prepare for graphing:
x = wavelengths
y = raw_intensities / max(raw_intensities)
y_fit = fitted / max(fitted)

#find the peak wavelength:
im = np.where(y_fit == max(y_fit))
i_max = im[0][0]
peak_wavelength = wavelengths[i_max]

'''
#plot them
plt.figure()
plt.plot(x,y, '.')
plt.plot(x,y_fit)
plt.title('peak detected at %.2f nm' % peak_wavelength)

plt.show(block = True)
'''
#print('peak detected at:', peak_wavelength)

outfile = open('./peak_result.txt','w')
outfile.write('%.2f' % peak_wavelength)
outfile.close()
