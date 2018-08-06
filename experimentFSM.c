/* ExperimentFSM.c
 * implements the state machine which will govern device behavior.
 * Receives commands from the server and runs the experiment.
 * 
 * 
 */
#include <wiringPi.h>
#include <stdlib.h>
#include <stdio.h>

#include "./spectrometerDriver.h"
#include "./experimentFSM.h"

static char *getStateString(int s);




static specSettings thisExperiment;
static char experimentStatusMessage[512] = "Uninitialized";
static int inited = 0;
static int readingsTaken = 0;

static int verbose = 1;

static double averagedArray[1024], spectrumArray[1024];

static enum experiment_states {
    IDLE,
    GETTING_SPECTRA,
    AWAITING_TIMEOUT,
    WRITING_RESULTS,
} experimentState = IDLE;

int initExperiment(specSettings spec)
{
    thisExperiment = spec;
    experimentState = IDLE;
    readingsTaken = 0;
    inited = 1;
}

int runExperiment(char command)
{
    if (!inited) {
        printf("\n\n Aw damn. Tried to run experiment without init. \n\n");
        while (1);
    }

    if (verbose) {
        printf("running fsm in state %i with command %i \n", experimentState, command);
    }

    sprintf(experimentStatusMessage, "The experiment process is currently %s\n", getStateString(experimentState));


    //this timer thread will post its event to the experiment FSM after the set amount of time

    PI_THREAD(timerThread)
    {
        if (verbose) {
            printf("starting a timer for %i seconds\n", thisExperiment.timeBetweenScans);
        }
        //delay accepts argument in milliseconds
        delay(thisExperiment.timeBetweenScans * 1000);
        runExperiment(TIMEOUT);
    }

    int i, j;
    switch (experimentState) {

    case IDLE:

        switch (command) {
        case START_EXPERIMENT:

            //TO DO:	
            //CHECK HERE IF INITED!!!!!

            //motor ON

            experimentState = GETTING_SPECTRA;
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
            if (verbose) {
                printf("about to get a spectrum\n");
            }
            led_ON();

            //grab and total some readings...
            for (i = 0; i < thisExperiment.avgPerScan; i++) {
                getSpectrometerReading(spectrumArray);
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
            led_OFF();

            if (verbose) {
                printf("finished getting reading number %i\n", readingsTaken);
            }
            //now, check to see if we have taken enough scans. if not, set a timer and keep waiting. 
            if (readingsTaken < thisExperiment.numScans) {
                //start a timer thread. these threads return 0 if successfully started:

                if (piThreadCreate(timerThread)) {
                    printf("pi thread failed somehow!\n");
                    exit(5);
                }

                //give the thread a moment to set up, because we break immediately after:
                delay(100);

                experimentState = AWAITING_TIMEOUT;
                break;
            } else {
                experimentState = WRITING_RESULTS;
                //we run ourselves
                runExperiment(SELF);
                break;
            }

            break; //break SELF

        case STOP_EXPERIMENT:
            inited = 0;
            experimentState = IDLE;
            break;

        default:

            break;


        }
        break; //break GETTING_SPECTRA

    case AWAITING_TIMEOUT:
        if (verbose) {
            printf("awaiting timeout...\n");
        }
        switch (command) {

            //when we finally get here, the timer has expired and we
            //are ready to grab another measurement.
            //note, the timer will never expire during a time when we 
            //don't need at least one more reading, so we always transition
            //upon this timer. 
        case TIMEOUT:
            if (verbose) {
                printf("got timeout!\n");
            }
            experimentState = GETTING_SPECTRA;
            runExperiment(SELF);
            break;

        case STOP_EXPERIMENT:
            inited = 0;
            experimentState = IDLE;
            break;

        }
        break;


    case WRITING_RESULTS:
        if (verbose) {
            printf("\nfinished getting spectra.\nWRITING RESULTS!\n\n");
        }

        //TO DO:
        //whenever we make it to this state, we expect to have a complete
        //set of spectra taken, and may begin processing the results. 
        //spectral integration? peak detection? Whatever. Do it here. 
        //save to disk here too. 

        //also, is there some way here to automatically update the app?

        experimentState = IDLE;
        inited = 0;
        break;

    default:

        break;



    }


    //if (verbose) {
    //printf("FSM awaits next run command.\n");

    //}
}

int experimentRunning()
{

    //return experimentState == IDLE ? 0 : 1;

    if (experimentState == IDLE) {
        return 0;
    } else {
        return 1;
    }
}

specSettings getExperimentSettings()
{
    return thisExperiment;
}

char *getExpStatusMessage()
{
    sprintf(experimentStatusMessage, "The experiment process is currently %s\n", getStateString(experimentState));
    return experimentStatusMessage;
}

int experimentIsInited()
{
    return inited;
}


//private function to get strings from states

static char *getStateString(int s)
{
    switch (s) {
    case IDLE:
        return "Idle";
        break;

    case GETTING_SPECTRA:
        return "Taking Measurement";
        break;

    case AWAITING_TIMEOUT:
        return "Awaiting Next Measurement";
        break;

    case WRITING_RESULTS:
        return "Writing Results to File";
        break;

    default:
        return "In an unknown state?!";
        break;
    }
}
