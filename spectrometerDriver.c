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
//#define SPEC_CONNECTED //uncomment this if spectrometer is usb-connected
//#define ADC_CONNECTED //uncomment this if the ADC is connected

#define MILLISEC_TO_MICROSEC 1000
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

static specSettings thisSpec = {5, 60, 1000, 0, 3};

static double spectrumArray[NUM_WAVELENGTHS];

static int Hardware_Init();

int setIntegrationTime(int newTime)
{
    if (!inited) {
        if (Hardware_Init() != 0) {
            printf("Bad Times at setIntegTime");
            exit(-1);
        }
    }

#ifdef SPEC_CONNECTED
    seabreeze_set_integration_time_microsec(spectrometerIndex, &errorCode, newTime * MILLISEC_TO_MICROSEC);
#endif
}

int applySpecSettings(specSettings in)
{
    thisSpec.numScans = in.numScans;
    thisSpec.timeBetweenScans = in.timeBetweenScans;
    thisSpec.integrationTime = in.integrationTime;
    thisSpec.boxcarWidth = in.boxcarWidth;
    thisSpec.avgPerScan = in.avgPerScan;

    if (!inited) {
        if (Hardware_Init() != 0) {
            printf("Bad Times at applySpecSettings");
            exit(-1);
        }
    }

    //update hardware to new settings
    setIntegrationTime(thisSpec.integrationTime);
}

int getSpectrometerReading(double *inBuff)
{
    double tmpBuf[NUM_WAVELENGTHS];
    int i;

    if (!inited) {
        if (Hardware_Init() != 0) {
            printf("Bad Times at getReading");
            exit(-1);
        }
    }

    //default to y=x
    for (i = 0; i < NUM_WAVELENGTHS; i++) {
        spectrumArray[i] = i;
    }

#ifdef SPEC_CONNECTED
    seabreeze_get_formatted_spectrum(spectrometerIndex, &errorCode, spectrumArray, NUM_WAVELENGTHS);
#endif

    //copy results to input buffer
    //for (i = 0; i < NUM_WAVELENGTHS; i++) {
    //    inBuff[i] = spectrumArray[i];
    //}

    boxcarAverage(thisSpec.boxcarWidth, spectrumArray, inBuff, NUM_WAVELENGTHS);

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
#ifdef ADC_CONNECTED
    return analogRead(BASE);
#else 
    return 777;
#endif
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
    errorCode = 0;
    printf("Opening spectrometer...");
#ifdef SPEC_CONNECTED
    seabreeze_open_spectrometer(spectrometerIndex, &errorCode);
#endif

    if (errorCode) {
        printf("Could not find device.\n");
        getchar();
        return 1;
    }
    printf("done.\n");

    printf("Setting integration time to %i ms...", thisSpec.integrationTime);

#ifdef SPEC_CONNECTED
    seabreeze_set_integration_time_microsec(spectrometerIndex, &errorCode, thisSpec.integrationTime * MILLISEC_TO_MICROSEC);
#endif

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
    printf("applying boxcar width %i\n", width);


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

void printSpecSettings(specSettings in)
{
    printf("\n print spec settings \n");
    printf(" ===================\n");
    printf("numScans         = %i\n", in.numScans);
    printf("timeBetweenScans = %i\n", in.timeBetweenScans);
    printf("integrationTime  = %i\n", in.integrationTime);
    printf("boxcarWidth      = %i\n", in.boxcarWidth);
    printf("avgPerScan       = %i\n\n", in.avgPerScan);
}



