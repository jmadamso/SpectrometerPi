/*
 * Custom wrapper for the spectrometer project for easy function calls 
 * in response to incoming commands
 * 
 * 
 * /
/***********************************************************************/


#include "./spectrometerDriver.h"
#include "api/SeaBreezeWrapper.h"

#include <stdio.h>
#include <stdlib.h>

#include <wiringPi.h>
#include <mcp3004.h>


//DEFINES
#define MILLISEC_TO_MICROSEC 1000
#define NUM_READINGS 1024 //known for our spectrometer
#define MAX_INTENSITY 3500

#define BASE 100 //ADC stuff
#define SPI_CHAN 0

//wiringPi pin1 = pi header pin 12 = broadcom pin 18
//using at terminal "gpio readall" 
#define PWM_PIN 1
#define LED_PIN 25

//MODULE DEFINES AND FUNCTIONS
static int spectrometerIndex = 0;
static int errorCode = 0;
static FILE *outputFile;
static int inited = 0;
//default integration time 
static int integrationTimeMillisec = 1000;

static double spectrumArray[NUM_READINGS];

static int Hardware_Init();

int setIntegrationTime(int newTime)
{
    integrationTimeMillisec = newTime;
    if (inited) {
        //seabreeze_set_integration_time_microsec(spectrometerIndex, &errorCode, integrationTimeMillisec * MILLISEC_TO_MICROSEC);
    }
}

int getSpectrometerReading(int *inBuff)
{
    if (!inited) {
        if (Hardware_Init() != 0) {
            printf("Bad Times at getReading");
            exit(-1);
        }
    }
    
    seabreeze_get_formatted_spectrum(spectrometerIndex, &errorCode, inbuff, NUM_READINGS);

    if (errorCode) {
        printf("Error: problem getting spectrum\n");
        getchar();
        return (-1);
    }
    return 0;
}

int getPressureReading()
{

    if (!inited) {
        if (Hardware_Init()) {
            printf("Bad Times at getPressure");
            exit(-1);
        }
    }
    return analogRead(BASE);
}

void motor_ON()
{
    if (!inited) {
        if (Hardware_Init()) {
            printf("Bad Times at motor ON");
            exit(-1);
        }

    }
    pwmWrite(PWM_PIN, 540);

}

void motor_OFF()
{
    if (!inited) {
        if (Hardware_Init()) {
            printf("Bad Times at motor OFF");
            exit(-1);
        }
    }
    pwmWrite(PWM_PIN, 0);
}

void LED_ON()
{
    if (!inited) {
        if (Hardware_Init()) {
            printf("Bad Times at LED_ON");
            exit(-1);
        }
    }
    digitalWrite(LED_PIN, 1);
}

void LED_OFF()
{
    if (!inited) {
        if (Hardware_Init()) {
            printf("Bad Times at LED_OFF");
            exit(-1);
        }
    }
    digitalWrite(LED_PIN, 0);
}

int endSession()
{
    printf("Closing...");
    seabreeze_close_spectrometer(spectrometerIndex, &errorCode);
    if (errorCode) {
        printf("Unable to close spectrometer.\n");
        return 1;
    }
    return 0;
}

static int Hardware_Init()
{
    printf("Opening spectrometer...");
    //seabreeze_open_spectrometer(spectrometerIndex, &errorCode);

    if (errorCode) {
        printf("Could not find device.\n");
        getchar();
        return 1;
    }
    printf("done.\n");

    printf("Setting integration time to %i ms...", integrationTimeMillisec);
    //seabreeze_set_integration_time_microsec(spectrometerIndex, &errorCode, integrationTimeMillisec * MILLISEC_TO_MICROSEC);
    if (errorCode) {
        printf("Unable to set integration time.\n");
        getchar();
        return 1;
    }

    printf("Initializing wiringPi, PWM and ADC...");
    if (wiringPiSetup() == -1) {
        return -1;
    }
    //BASE sets the new pin base
    mcp3004Setup(BASE, SPI_CHAN);

    //set this pin up as PWM
    pinMode(PWM_PIN, PWM_OUTPUT);
    pinMode(LED_PIN, OUTPUT);



    printf("done.\n");
    inited = 1;
    printf("now inited.\n");
    return 0;
}

int boxcarAverage(int width, double *inputArray, double *outputArray, int numElements)
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




