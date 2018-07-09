/***********************************************************************//**
 * spectrometer.c
 * main file for the portable yanik SDP	
 *
 * 
 * 
 * 
 * This file used a code sample as its base. Substantial modifications
 * have been made, inc  
 The original documentation for this file was as follows:
 VisualCppConsoleDemo.cpp
 *
 * This file represents a simple command-line SeaBreeze test using Visual C++
 * and the C++ interface to SeaBreezeWrapper.
 *
 * Tested under Win7-32 and Visual Studio 2010 by mzieg on 20-Feb-2014.
 *
 * LICENSE:
 *
 * SeaBreeze Copyright (C) 2014, Ocean Optics Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#include "api/SeaBreezeWrapper.h"

#include <stdio.h>
#include <stdlib.h>
#include "shapes.h"

#include "fontinfo.h"

#include <wiringPi.h>
#include <mcp3004.h>
  
 
//DEFINES
#define MILLISEC_TO_MICROSEC 1000
#define NUM_READINGS 1024 //known for our spectrometer
#define MAX_INTENSITY 3500

//MODULE DEFINES AND FUNCTIONS
static int spectrometerIndex = 0;
static int errorCode = 0;
static int integrationTimeMillisec = 1000;
static FILE *outputFile;

static int Hardware_Init();
static int boxcarAverage(int width, double *inputArray, double *outputArray, int numElements);

int main()
{
    int height, width;
    int i, j, k;
    int highestSeen, peakIndex;
    int scansToAverage = 1, numMeasurements = 1, boxcarWidth = 5;
    double spectrumArray[NUM_READINGS];
    double smoothSpectrumArray[NUM_READINGS];
    double averagedSpectrumArray[NUM_READINGS];
    double wavelengthArray[NUM_READINGS];
    char charBuffer[1024] = "";
 
    outputFile = fopen("data.txt", "w");
    if (!outputFile) {
        printf("failed to open file.\n");
        getchar();
        return 1;
    }

    if (Hardware_Init() == 1) {
        //something failed
        getchar();
        return 1;
    }

    //printf("numPixels = %i\n", seabreeze_get_formatted_spectrum_length(spectrometerIndex, &errorCode));
    seabreeze_get_wavelengths(spectrometerIndex, &errorCode, wavelengthArray, NUM_READINGS);

    init(&width, &height); //start gfx, return resolution

    //Start(width, height); // Start the picture
    //printf("%i %i\n",width,height);
    //while(1);
    //Start(width, height); // Start the picture
    Background(0, 0, 0); //paint it black
    Stroke(0, 255, 0, 1);

int filecount = 0;
char buf[1024];
while(1) {

filecount++;
sprintf(buf,"data_%i",filecount);
outputFile = fopen(buf, "w");
    if (!outputFile) {
        printf("failed to open file.\n");
        getchar();
        return 1;
    }
    for (i = 0; i < NUM_READINGS; i++) {
        averagedSpectrumArray[i] = 0;
    }

    for (i = 0; i < scansToAverage; i++) {


        //get the measurements
        seabreeze_get_formatted_spectrum(spectrometerIndex, &errorCode, spectrumArray, NUM_READINGS);

        if (errorCode) {
            printf("Error: problem getting spectrum\n");
            getchar();
            return (-1);
        }

        boxcarAverage(boxcarWidth, spectrumArray, smoothSpectrumArray, NUM_READINGS);

        highestSeen = 0;
        peakIndex = 0;

        Stroke(0, 255, 0, 1);
        //here is the loop where we iterate through the data
        for (j = 1; j < NUM_READINGS; j++) {

            if (smoothSpectrumArray[j] > highestSeen) {
                highestSeen = smoothSpectrumArray[j];
                peakIndex = j;
            }

            //some options to get at the data in terminal and file:	
            //printf("Wavelength: %5.2f nm; Intensity: %i\n", (float)wavelengthArray[i], (int)spectrumArray[i]);
            //fprintf(outputFile, "Wavelength: %5.2f nm; Intensity: %i\n", (float)wavelengthArray[i], (int)spectrumArray[i]);
            //fprintf(outputFile, "%5.2f, %i\n", (float)wavelengthArray[i], (int)spectrumArray[i]);
			//graph the smoothed value
            Line(j - 1, smoothSpectrumArray[j - 1] * height / MAX_INTENSITY, j, smoothSpectrumArray[j] * height / MAX_INTENSITY);



        }//end pixel iterate

        Stroke(255, 0, 0, 1);
        for (j = 1; j < NUM_READINGS; j++) {
			//graph the rough values
            //Line(j-1 , spectrumArray[j - 1] * height / MAX_INTENSITY, j , spectrumArray[j] * height / MAX_INTENSITY);
        }//end pixel iterate



        Stroke(255, 255, 255, 1); //white circle
        Circle(peakIndex, smoothSpectrumArray[peakIndex] * height / MAX_INTENSITY, 10);
        Fill(255, 255, 255, 1); // White text
        sprintf(charBuffer, "Peak Value: %5i Wavelength: %5f", (int) smoothSpectrumArray[peakIndex], wavelengthArray[peakIndex]);
        Text(1, 25, charBuffer, SerifTypeface, height / 50); // write header

        for (j = 0; j < NUM_READINGS; j++) {
            averagedSpectrumArray[j] += smoothSpectrumArray[j];
        }//end pixel iterate


    } //end for(i)
    Stroke(255, 255, 255, 1);
    for (i = 0; i < NUM_READINGS; i++) {
        averagedSpectrumArray[i] /= scansToAverage;
        fprintf(outputFile, "%5.2f, %i\n", (float)wavelengthArray[i], (int)averagedSpectrumArray[i]);

		if(i != 0) {
			Line(i - 1, averagedSpectrumArray[i - 1] * height / MAX_INTENSITY, i, averagedSpectrumArray[i] * height / MAX_INTENSITY);
		}
    }


    End(); //end graphics to display canvas
    AreaClear(0, 0, width, height); //blank for next sweep
fclose(outputFile);
}

    //getchar();
    printf("Closing...");
    seabreeze_close_spectrometer(spectrometerIndex, &errorCode);
    if (errorCode) {
        printf("Unable to close spectrometer.\n");
        return 1;
    }
    finish();
    //fclose(outputFile);
    printf("done.\n");

    return 0;
}

static int Hardware_Init()
{

	

    printf("Opening spectrometer...");
    seabreeze_open_spectrometer(spectrometerIndex, &errorCode);

    if (errorCode) {
        printf("Could not find device.\n");
        getchar();
        return 1;
    }
    printf("done.\n");

    printf("Setting integration time to %i ms...",integrationTimeMillisec);
    seabreeze_set_integration_time_microsec(spectrometerIndex, &errorCode, integrationTimeMillisec * MILLISEC_TO_MICROSEC);
    if (errorCode) {
        printf("Unable to set integration time.\n");
        getchar();
        return 1;
    }
    printf("done.\n");
    return 0;
}

static int boxcarAverage(int width, double *inputArray, double *outputArray, int numElements)
{
    signed int i, j, k, l;

    if (width < 0) {
        printf("Boxcar width must be zero or positive integer. Defaulting to 0.\n");
        width = 0;
    }

    if (width == 0 || width == 1) {
        for (i = 0; i < numElements; i++) {
            outputArray[i] = inputArray[i];
        }
        return 1;
    }


    for (i = 0; i < numElements; i++) {
        //these are a bit funky with integers, but should still make 
        //a window with the input value in the center.
        
        //printf("for element %i and width %i, averaging input at: ",i,width);
        outputArray[i] = 0;
        for (j = 0; j < width; j++) {

            if (j == 0) {
                k = i - (width / 2);
            }
			
			//using l for the arrays ensures k doesnt get stuck when
			//starting a few before 0
            if (k < 0) {
                l = 0; //clamp the index
            } else if (k >= numElements) {
                l = numElements - 1; //clamp the index
            } else {
                l = k;
            }
            //printf(" %i  ",l);
            outputArray[i] += inputArray[l]; //sum them
            k++;
        }
        //printf("\n");
        outputArray[i] /= width; //perform the average here
    }
    return 1;
}




