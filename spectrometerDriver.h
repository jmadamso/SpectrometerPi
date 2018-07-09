
/*
 * Custom wrapper for the spectrometer project for easy function calls 
 * in response to incoming commands
 * 
 * 
 * /
***********************************************************************/
#ifndef SPECDRIVER_H
#define SPECDRIVER_H
 
 
 
int setIntegrationTime(int newTime);


/*getSpectrometerReading
 * Asks the spectrometer to take a reading, and place the results
 * into inBuff
 */
int getSpectrometerReading(int *inBuff);


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
void LED_OFF() ;



int boxcarAverage(int width, double *inputArray, double *outputArray, int numElements);


/* endSession();
 * 
 */
int endSession();
 

#endif
