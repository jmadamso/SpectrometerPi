
/*
 * Custom wrapper for the spectrometer project for easy function calls 
 * in response to incoming commands
 * 
 * 
 * /
 ***********************************************************************/

//!! a different secret message <3!!

#ifndef SPECDRIVER_H
#define SPECDRIVER_H

#define NUM_WAVELENGTHS 1024 //known for our spectrometer


//specSettings: struct containing spectrometer paramaters and defaults

typedef struct {
    int numScans;
    int timeBetweenScans;
    int integrationTime;
    int boxcarWidth;
    int avgPerScan;
} specSettings;

/*PrintSpecSettings
 * provides a printout of passed-in specsettings struct
 */
void printSpecSettings(specSettings in);



/*setIntegrationTime
 * Sets the integration time of the spectrometer in MILLISECONDS
 */
int setIntegrationTime(int newTime);

/*applySpecSettings
 * applies parameters from incoming struct to our instance
 */
int applySpecSettings(specSettings in);

/*getSpectrometerReading
 * Asks the spectrometer to take a reading, and place the results
 * into inBuff
 */
int getSpectrometerReading(double *inBuff);


/*getPressureReading
 * Grab a reading from the ADC and pass it along
 * */
int getPressureReading();

/*
 * Functions to turn the motor pin on and off
 * 
 */
void motor_ON();
void motor_OFF();

/*
 * Functions to turn the LED on or off
 * 
 * 
 */
void LED_ON();
void LED_OFF();


/*boxcarAverage
 * produce a smooth array, using boxcar width Width operating on inputArray
 */
int boxcarAverage(int width, double *inputArray, double *outputArray, int numElements);


/* endSession();
 * 
 */
int endSession();


#endif
