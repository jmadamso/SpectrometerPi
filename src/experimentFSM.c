/* ExperimentFSM.c
 * implements the state machine which will govern device behavior.
 * Receives commands from the server and runs the experiment.
 * 
 * 
 */
#include <wiringPi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../include/spectrometerDriver.h"
#include "../include/experimentFSM.h"


//#define VERBOSE

static char *getStateString(int s);
static char *specStructToIndexString(specSettings s);
static double findPeakValueWavelength(double *array);



static specSettings thisExperiment;
static char experimentStatusMessage[512] = "Uninitialized";
static int inited = 0;
static int update = 0;
static int readingsTaken = 0;

static int (*updateServer)();

static FILE *expFile;
static FILE *expIndex;


static double averagedArray[NUM_WAVELENGTHS], 
			  spectrumArray[NUM_WAVELENGTHS],
			  finalArray[NUM_WAVELENGTHS];


//simple linked list of arrays, since we accept different numbers of
//scans
typedef struct listNode {
	double array[1024];
	struct listNode *nextNode;
	} listNode;
	
static listNode *spectrumList;
	
static void writeExperimentFile(FILE *f, listNode *head, double *results);


	
static listNode *list_add(listNode *head,double *doubleArray);
static void list_print(listNode *head);
static void list_destroy(listNode *head);

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
    spectrumList = NULL;
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
            char reportPath[256];
            sprintf(reportPath,"./experiment_results/exp_%s.txt",thisExperiment.timestamp);
                        
			expFile = fopen(reportPath,"w");
			if (!expFile) {
				printf("could not create file! ");
				while(1);
			}
			
            experimentState = GETTING_SPECTRA;
            updateServer();
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

            //we have now taken one more reading:
            readingsTaken++;

            spectrumList = list_add(spectrumList,finalArray);
            

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
            list_destroy(spectrumList);
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
#ifdef VERBOSE
                printf("got timeout!\n");
#endif
            experimentState = GETTING_SPECTRA;
            runExperiment(SELF);
            break;

        case STOP_EXPERIMENT:
            inited = 0;
            experimentState = IDLE;
            list_destroy(spectrumList);
            break;

        }
        break;


    case WRITING_RESULTS:
        printf("\nfinished getting spectra.\nWRITING RESULTS!\n\n");
		
		//carve out a result array:
		double *resultArray = malloc(thisExperiment.numScans*sizeof(double));
		if(!resultArray) {
			printf("we didnt get the memo\n");
		}
		//i'm so happy this works:
		listNode *cur;
		int i = 0;
		for(cur = spectrumList; cur != NULL; cur = cur->nextNode) {
			resultArray[i++] = findPeakValueWavelength(cur->array);
		}
		
		
		//open the index file and write a serialized spec struct to it:
		expIndex = fopen("./experiment_results/INDEX","a");
		char *indexString = specStructToIndexString(thisExperiment);
		fprintf(expIndex,indexString);

		//printf everything to our file:
		writeExperimentFile(expFile,spectrumList,resultArray);

		//tidy up and return to idling:
		fclose(expFile);
		fclose(expIndex);
		free(resultArray);
        list_destroy(spectrumList);
        inited = 0;
        experimentState = IDLE;
        updateServer();
		//list_print(spectrumList);

        break;

    default:

        break;
    }
}

int experimentRunning()
{
    return experimentState == IDLE ? 0 : 1;
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
	static char str[512];
	
	if(s == IDLE) {
		return "Idle";
	} else {
		sprintf(str,"Finished measurement %i/%i with %i second intervals",readingsTaken,thisExperiment.numScans,thisExperiment.timeBetweenScans);
        return str;
	}
}


static specSettings indexStringToSpecStruct(char *indexString) 
{
	specSettings s;
	return s;
	
}

static char *specStructToStatusString(specSettings s) {
	
}

static char *specStructToIndexString(specSettings s) {
	static char str[512];
	sprintf(str, "exp_%s;%s;%s;%i;%i;%i;%i;%i\n",
		thisExperiment.timestamp,
		thisExperiment.doctorName,
		thisExperiment.patientName,
		thisExperiment.numScans,
		thisExperiment.timeBetweenScans,
		thisExperiment.integrationTime,
		thisExperiment.boxcarWidth,
		thisExperiment.avgPerScan);
		
		return str;
	}




static listNode *list_add(listNode *head,double *doubleArray) {
		int i;
		int count = 1;
	
		if(head == NULL) {
			printf("STARTED LIST with item 0\n");
			head = (listNode*)malloc(sizeof(listNode));
			
			if(head == NULL) {
				printf("empty head.\n");
				while(1);
			}
			
			for(i = 0; i < NUM_WAVELENGTHS; i++) {
				head->array[i] = doubleArray[i];
			}
			head->nextNode = NULL;

			return head;
		} else {
			listNode *cur = head;
			//iterate to the end of the list:
			while(cur->nextNode != NULL) {
				cur = cur->nextNode;
				count++;
			}
			
			listNode *tmp = malloc(sizeof(listNode));
			tmp->nextNode = NULL;
			for(i = 0; i < NUM_WAVELENGTHS; i++) {
				tmp->array[i] = doubleArray[i];
			}
			
			printf("added item %i to list\n",count);
			cur->nextNode = tmp;
			
			return head;
		}
	
		
}


static void list_destroy(listNode *head) {
		if(head == NULL) {
			return;
		}
	
		listNode *cur = head;
		listNode *tmp;
		
		while(cur->nextNode != NULL) {
			listNode *tmp = cur->nextNode;
			free(cur);
			cur = tmp;
		}
		
}


static void list_print(listNode *head) {
	int count = 0;
	if (head == NULL) {
		return;
	}
	listNode *cur = head;
	while(cur != NULL) {
		printf("first 3 of array %i = %f %f %f\n",count++,cur->array[0],cur->array[1],cur->array[2]);
		cur = cur->nextNode;
	}
}
//this is where we do the peak detection work
static double findPeakValueWavelength(double *array) {
	int i;
	int largestSeen = 0;
	int peakIndex = 0;
	//printf("Attempting to find peak wavelength\n");
	
	if(array == NULL) {
		printf("got bad array!\n");
		return 580;
	}
	
	for(i = 0; i < NUM_WAVELENGTHS; i++) {
		//largestSeen = array[i] <= largestSeen ? largestSeen : array[i];
		//peakIndex = array[i] == largestSeen ? i : peakIndex;
		//translation:
		if (array[i] > largestSeen) {
			largestSeen = array[i];
			peakIndex = i;
		}
						 
	}
	
	return getSpectrometerWavelength(peakIndex);
}



static void writeExperimentFile(FILE *f, listNode *head, double *results) {
	char line[512] = "";
	char tmp[512];

	//line = specStruct2descriptor OR SOMETHING
	fprintf(f,"DESCRIPTOR STRING\n");

	int i,j = 0;
	for(i = 0; i < thisExperiment.numScans; i++) {
		sprintf(tmp,"Reading %i\t",i + 1);
		strcat(line,tmp);
	}
	strcat(line,"Results\n");
	fprintf(f,line);
	strcpy(line,"");

	listNode *cur = head;

	
	for(i = 0; i < NUM_WAVELENGTHS; i++) {
		cur = head;
		//printf("traversed time and space %i/%i times\n",++j,NUM_WAVELENGTHS);
		while(cur != NULL) {
			sprintf(tmp,"%-11.2f\t",cur->array[i]);
			strcat(line,tmp);
			cur = cur->nextNode;
		}
		if (i < thisExperiment.numScans) {
			sprintf(tmp,"%-11.2f\n",results[i]);
			strcat(line,tmp);
		} else {
			strcat(line,"\n");
		}
		fprintf(f,line);
		strcpy(line,"");
	}
}










