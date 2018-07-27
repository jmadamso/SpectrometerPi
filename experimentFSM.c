/* ExperimentFSM.c
 * implements the state machine which will govern device behavior.
 * Receives commands from the server and runs the experiment.
 * 
 * 
 */

#include "./spectrometerDriver.h"
#include "./experimentFSM.h"

static specSettings thisExperiment;
static int mySocket;
static int inited = 0;
static int readingsTaken = 0;

static double averagedArray[1024], spectrumArray[1024]

static enum {
    IDLE,
    GETTING_SPECTRA,
    AWAITING_TIMEOUT,
    WRITING_RESULTS,
} ExpStates experimentState;

int initExperiment(specSettings spec, int BTSocket)
{
    thisExperiment = spec;
    mySocket = BTSocket;
    experimentState = IDLE;
    readingsTaken = 0;
    inited = 1;
}

/*
    int numScans;
    int timeBetweenScans;
    int integrationTime;
    int boxcarWidth;
    int avgPerScan;
 */


int runExperiment(char command)
{

    //this timer thread will post its event to the experiment FSM after the set amount of time

    PI_THREAD(timerThread)
    {
        //delay accepts argument in milliseconds
        delay(thisExperiment.timeBetweenScans * 1000);
        runExperiment(TIMEOUT);
    }

    int i, j;
    switch (experimentState) {

    case IDLE:

        switch (command) {
        case START:
            //motor ON

            state = GETTING_SPECTRA;
            //now we run ourself, since this is an internal transition.
            //shouldnt recurse too much during normal operation :)
            runExperiment(SELF);
            break;

        default:

            break;

        }
        break; //break IDLE

        //upon entering this state, we grab a spectrum reading first thing: 
    case GETTING_SPECTRA:
        switch (command) {
        case SELF:
            LED_ON();

            //grab and total some readings...
            for (i = 0; i < thisExperiment.avgPerScan; i++) {
                getSpectrumReading(spectrumArray);
                for (j = 0; j < 1024; j++) {
                    averagedArray[j] += spectrumArray[j];
                }
            }

            //...then perform the averaging
            for (i = 0; i < 1024; i++) {
                averagedArray[i] /= thisExperiment.avgPerScan;
            }

            //WE NOW HAVE ONE SPECTRUM READY TO PROCESS.
            //PLACE IT IN THE LIST.

            readingsTaken++;
            LED_OFF();


            //now, check to see if we have taken enough scans. if not, set a timer and keep waiting. 
            if (readingsTaken < thisExperiment.numScans) {

                //start a timer thread. these threads return 0 if successfully started:
                if (piThreadCreate(pressureThread);) {
                    printf("pi thread failed somehow!\n");
                    exit(5);
                }
                state = AWAITING_TIMEOUT;
                break;
            } else {
                state = WRITING_RESULTS;
                //we run ourselves
                runExperiment(SELF);
                break;
            }

            break; //break SELF

        case QUIT:

            break;

        default:

            break;


        }
        break; //break GETTING_SPECTRA

    case AWAITING_TIMEOUT:
        switch (command) {

            //when we finally get here, the timer has expired and we
            //are ready to grab another measurement.
            //note, the timer will never expire during a time when we 
            //don't need at least one more reading, so we always transition
            //upon this timer. 
        case TIMEOUT:
            state = GETTING_SPECTRA;
            runExperiment(SELF);
            break;

        case QUIT:

            break;

        }
        break;


    case WRITING_RESULTS:

        //whenever we make it to this state, we expect to have a complete
        //set of spectra taken, and may begin processing the results. 
        //spectral integration? peak detection? Whatever. Do it here. 

        break;

    default:

        break;



    }



}

int getExperimentState()
{
    if (state == WAITING) {
        //return AVAILABLE
    } else {
        //return IN PROGRESS	
    }

}

int isInited()
{
    return inited;
}

