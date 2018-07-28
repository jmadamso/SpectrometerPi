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
static FILE *waves;
static int inited = 0;
static double spectrumArray[NUM_WAVELENGTHS];
static specSettings thisSpec = {5, 60, 1000, 0, 3};

static int specConnected = 0;
static int adcConnected = 0;



static int Hardware_Init();

int setIntegrationTime(int newTime)
{
    if (!inited) {
        if (Hardware_Init() != 0) {
            printf("Init failure at setIntegrationTime()\n");
            return -1;
        }
    }

    if (specConnected) {
        seabreeze_set_integration_time_microsec(spectrometerIndex, &errorCode, newTime * MILLISEC_TO_MICROSEC);
        if (errorCode) {
            printf("Integration time failure in connected spectrometer :(\n");
        }
        return errorCode;
    } else {
        return -1;
    }
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
            printf("Init failure at applySpecSettings()\n");
            return -1;
        }
    }

    //update hardware to new settings
    if (specConnected) {
        return setIntegrationTime(thisSpec.integrationTime);
    }

    return 0;
}

int getSpectrometerReading(double *inBuff)
{
    int i;

    if (!inited) {
        if (Hardware_Init() != 0) {
            printf("Init failure at getSpectrometerReading()\n");
            return -1;
        }
    }

    //default to y=x
    for (i = 0; i < NUM_WAVELENGTHS; i++) {
        spectrumArray[i] = i;
        inBuff[i] = i;
    }

    errorCode = 0;
    if (specConnected) {
        seabreeze_get_formatted_spectrum(spectrometerIndex, &errorCode, spectrumArray, NUM_WAVELENGTHS);
        printf("spec appears connected\n");
    }

    boxcarAverage(thisSpec.boxcarWidth, spectrumArray, inBuff, NUM_WAVELENGTHS);

    if (errorCode) {
        printf("Error: problem getting spectrum\n");
        return -1;
    }
    return 0;
}

int getPressureReading()
{
    if (!inited) {
        if (Hardware_Init()) {
            printf("Init failure at getPressureReading\n");
            return -1;
        }
    }

    if (adcConnected) {
        return analogRead(BASE);
        printf("ADC appears connected\n");
    } else {
        return 777;
    }


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

void led_ON()
{
    if (!inited) {
        if (Hardware_Init()) {
            printf("Bad Times at LED_ON");
            exit(-1);
        }
    }
    digitalWrite(LED_PIN, 1);
}

void led_OFF()
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

    //try to open the spec and set flag accordingly
    printf("Opening spectrometer...");
    seabreeze_open_spectrometer(spectrometerIndex, &errorCode);

    if (errorCode) {
        printf("no device connected; applying defaults\n");
        specConnected = FALSE;
    } else {
        specConnected = TRUE;
    }

    //apply  integration time if connected
    if (specConnected) {
        printf("done.\n");
        printf("Setting integration time to %i ms...", thisSpec.integrationTime);
        seabreeze_set_integration_time_microsec(spectrometerIndex, &errorCode, thisSpec.integrationTime * MILLISEC_TO_MICROSEC);
        if (errorCode) {
            printf("Unable to set integration time.\n");
            return 1;
        }


    }

    //try to do other hardware
    printf("Initializing wiringPi, PWM and ADC...");
    if (wiringPiSetup() == -1) {
        return -1;
    }
    //BASE sets the new pin base.
    //From digging through wiringpi source, mcp3004 setup returns TRUE
    //when it sets up successfully. This is unfortunately opposite of 
    //the convention i have been using.




    if (mcp3004Setup(BASE, SPI_CHAN)) {
        adcConnected = TRUE;
        printf("ADC appears connected.\n");
    } else {
        adcConnected = FALSE;
        printf("no ADC found.\n");
    }

    //DISABLE ANALOG READINGS FOR DEBUG:
    //adcConnected = FALSE;

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
        printf("Boxcar width must be an integer betwwen 0 and 16. Defaulting to 0.\n");
        width = 0;
    }
    if (width > 16) {
        printf("Boxcar width must be an integer betwwen 0 and 16. Defaulting to 16.\n");
        width = 16;
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
    printf("Doctor           = %s\n", in.doctorName);
    printf("Patient          = %s\n", in.patientName);
    printf("numScans         = %i\n", in.numScans);
    printf("timeBetweenScans = %i\n", in.timeBetweenScans);
    printf("integrationTime  = %i\n", in.integrationTime);
    printf("boxcarWidth      = %i\n", in.boxcarWidth);
    printf("avgPerScan       = %i\n\n", in.avgPerScan);
}



