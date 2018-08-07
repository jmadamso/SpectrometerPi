/* ExperimentFSM.c
 * implements the state machine which will govern device behavior.
 * Receives commands from the server and runs the experiment.
 * 
 * 
 */
#include <wiringPi.h>
#include <stdlib.h>
#include <stdio.h>

#include "../include/spectrometerDriver.h"
#include "../include/experimentFSM.h"


//#define VERBOSE

static char *getStateString(int s);

static specSettings thisExperiment;
static char experimentStatusMessage[512] = "Uninitialized";
static int inited = 0;
static int update = 0;
static int readingsTaken = 0;

static int (*updateServer)();

static FILE *expFile;
static FILE *expIndex;

static int verbose = 1;

static double averagedArray[NUM_WAVELENGTHS], 
			  spectrumArray[NUM_WAVELENGTHS],
			  finalArray[NUM_WAVELENGTHS];

static enum experiment_states {
    IDLE,
    GETTING_SPECTRA,
    AWAITING_TIMEOUT,
    WRITING_RESULTS,
} experimentState = IDLE;


int initExperiment(specSettings spec, int (*updateFunction)())
{
    thisExperiment = spec;
    experimentState = IDLE;
    readingsTaken = 0;
    updateServer = updateFunction;
    for(int i = 0; i < 1024; i++) {
		averagedArray[i] = 0;
		spectrumArray[i] = 0;
	}
    
    inited = 1;
}

int runExperiment(char command)
{
	
    if (!inited) {
        printf("\n\n Aw damn. Tried to run experiment without init. \n\n");
        while (1);
    }

#ifdef VERBOSE
        printf("running fsm in state %i with command %i \n", experimentState, command);
#endif

    //this timer thread will post its event to the experiment FSM after the set amount of time

    PI_THREAD(timerThread)
    {
         printf("starting a timer for %i seconds\n", thisExperiment.timeBetweenScans);
        
        //delay accepts argument in milliseconds.
        //block for set time, then update the experiment
        delay(thisExperiment.timeBetweenScans * 1000);
        runExperiment(TIMEOUT);
    }

    int i, j;
    switch (experimentState) {

    case IDLE:

        switch (command) {
        case START_EXPERIMENT:

            if(!inited) {
				printf("Whoops! Somebody didn't init before starting. No defaults, either. :(\n");
				exit(-9);
			}

            //motor on
            //any other hardware stuff
            
            //prepare a file for this experiment
            char reportPath[1023];
            //eventually include a date string here...
            sprintf(reportPath,"./experiment_results/exp_%s.txt",thisExperiment.doctorName);
                        
			expFile = fopen(reportPath,"w");
			if (!expFile) {
				printf("could not create file! ");
				while(1);
			}
			
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
             printf("Collecting Spectrum\n\n");
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
                finalArray[i] = averagedArray[i];
                averagedArray[i] = 0;
            }

            //WE NOW HAVE ONE SPECTRUM READY TO PROCESS.
            //WRITE TO FILE AND PLACE IT IN THE LIST.
            

            readingsTaken++;
            fprintf(expFile,"Measurement %i:\n",readingsTaken);
			for (i = 0; i < NUM_WAVELENGTHS - 1; i++) {
				fprintf(expFile,"%.2f,",finalArray[i]);
			}
			fprintf(expFile,"%.2f\n",finalArray[NUM_WAVELENGTHS - 1]);
			
		
			
            led_OFF();

#ifdef VERBOSE
			printf("finished getting reading number %i\n", readingsTaken);
#endif                
            
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
                
                updateServer();
                //update = 1;
                
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
    
    #ifdef VERBOSE
                printf("awaiting timeout...\n");
    #endif
        
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
        printf("\nfinished getting spectra.\nWRITING RESULTS!\n\n");
		fprintf(expFile,"\nResult:\n");
		
		
        //TO DO:
        //whenever we make it to this state, we expect to have a complete
        //set of spectra taken, and may begin processing the results. 
        //spectral integration? peak detection? Whatever. Do it here. 
        //save to disk here too. 
		
		expIndex = fopen("./experiment_results/INDEX","a");
		
		fprintf(expFile,"EXPERIMENT END\n");
		fprintf(expIndex, "Exp. = %s\n",thisExperiment.doctorName);
		
		
		fclose(expFile);
		fclose(expIndex);
        experimentState = IDLE;
        
        //update = 1;
        updateServer();
        
        inited = 0;
        break;

    default:

        break;


    }
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

int readyToUpdate() {
	return update;
	}
	
	void clearUpdate() {
		update = 0;
		}

specSettings getExperimentSettings()
{
    return thisExperiment;
}

char *getExpStatusMessage()
{
    sprintf(experimentStatusMessage, "Experiment Status: %s", getStateString(experimentState));
    return experimentStatusMessage;
}

int experimentIsInited()
{
    return inited;
}


//private function to get strings from states

static char *getStateString(int s)
{
	static char str[1023];

    switch (s) {
    case IDLE:
        return "Idle";
        break;

    case GETTING_SPECTRA:
        return "Taking Measurement";
        break;

    case AWAITING_TIMEOUT:
		sprintf(str,"Finished measurement %i/%i with %i second intervals",readingsTaken,thisExperiment.numScans,thisExperiment.timeBetweenScans);
        return str;
        break;

    case WRITING_RESULTS:
        return "Writing Results to File";
        break;

    default:
        return "In an unknown state?!";
        break;
    }
}

//static int writeDoubleArrToCSV(FILE *f, double *arr,int numVals) {

//}
