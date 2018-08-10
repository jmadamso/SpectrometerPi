
/*
 * Custom wrapper for the spectrometer project for easy function calls 
 * in response to incoming commands
 * 
 * Joseph Adamson
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

    char *doctorName;
    char *patientName;
	char *timestamp;
} specSettings;

enum server_commands {
    MOTOR_ON = 97,
    MOTOR_OFF,
	LED_ON,
    LED_OFF,
    REQUEST_PRESSURE,
    SNAPSHOT,
    START_STREAM,
    STOP_STREAM,
    SETTINGS,
	EXP_START,       	//begin the experiment
    EXP_STOP,           //stop the experiment
    EXP_STATUS,         //return status (available/running) and settings
                                //of current experiment
    EXP_LIST,           //return a list of completed experiments
    EXP_LOOKUP,         //begin stream process of specific experiment
    EXP_DELETE,     
};



/*PrintSpecSettings
 * provides a printout of passed-in specsettings struct
 */
void printSpecSettings(specSettings in);

/*applySpecSettings
 * applies parameters from incoming struct to our instance
 */
int applySpecSettings(specSettings in);

/*setIntegrationTime
 * Sets the integration time of the spectrometer in MILLISECONDS
 */
int setIntegrationTime(int newTime);

/*getSpectrometerReading
 * Asks the spectrometer to take a reading, and place the results
 * into inBuff. If no spec connected, inbuff is populated with buf[i] = i
 * Returns 0 on success
 * Returns -1 on init or reading failure
 */
int getSpectrometerReading(double *inBuff);
int getSpectrometerWavelength(int index);


/*getPressureReading
 * Grab a reading from the ADC and pass it along. 
 * 
 * Returns reading on success
 * Returns 777 if no adc connected
 * Returns -1 if init failure
 * */
int getPressureReading();

/*Functions to turn the motor pin on and off. 
 * 
 * Exits upon init failure
 */
void motor_ON();
void motor_OFF();

/*Functions to turn the LED on or off
 * 
 * Exits upon init failure
 */
void led_ON();
void led_OFF();


/*boxcarAverage
 * produce a smooth array, using boxcar width Width operating on inputArray
 * 
 * Always returns 1
 */
int boxcarAverage(int width, double *inputArray, double *outputArray, int numElements);


/* endSession();
 * 
 */
int endSession();


#endif
