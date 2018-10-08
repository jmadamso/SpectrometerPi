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


//function which opens and returns a correctly formatted index file
//if it for some reason doesn't exist:
static FILE *safelyOpenIndex();


//some functions to essentially serialize/deserialize spec instances:
static char *specStructToIndexString(specSettings s);


//peak detection work happens here:
static double findPeakValueWavelength(double *wavelengths, double *intensities);

static char *getStateString(int s);

static specSettings thisExperiment;
static char experimentStatusMessage[512] = "Uninitialized";
static int inited = 0;
static int update = 0;
static int readingsTaken = 0;
static int numSavedExperiments;

static int (*updateServer)();

static FILE *expFile;
static FILE *expIndex;

static double wavelengths[NUM_WAVELENGTHS],
			  averagedArray[NUM_WAVELENGTHS], 
			  spectrumArray[NUM_WAVELENGTHS],
			  finalArray[NUM_WAVELENGTHS];


//a quick and dirty single-linked list to allow arbitrary numbers of readings:
typedef struct listNode {
	double array[NUM_WAVELENGTHS];
	struct listNode *nextNode;
	} listNode;
	
static listNode *list_add(listNode *head,double *doubleArray);
static void list_print(listNode *head);
static void list_destroy(listNode *head);


static listNode *spectrumList;
	
	
	
static void writeExperimentFile(FILE *f, listNode *head, double *results);




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
    for(int i = 0; i < NUM_WAVELENGTHS; i++) {
		averagedArray[i] = 0;
		spectrumArray[i] = 0;
	}
	getSpectrometerWavelengthArray(wavelengths);
    spectrumList = NULL;
    inited = 1;
}

int runExperiment(char command)
{
    if (!inited) {
        printf("\n\n Tried to run experiment without init. \n\n");
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
				printf("Whoops! Somehow we haven't inited. This one's fatal :( \n");
				exit(-9);
			}

            /*
             * LABSMITH STUFF GOES HERE?!
             * IN FUTURE, MOVE TO A PRIMING_CHIP STATE 
             * 
             */ 
            
            //prepare a file for this experiment
            char reportPath[256];
            sprintf(reportPath,"./experiment_results/%s",thisExperiment.timestamp);
                        
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
            for (i = 0; i < NUM_WAVELENGTHS; i++) {
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
                    int tryCount = 0;
                    for(tryCount = 0; tryCount < 3; tryCount++) {
						//break out if we succeed
						printf("trying attempt %i\n",tryCount);

						if(!piThreadCreate(timerThread)) {
							break;
						}
					}
					if (tryCount == 3) {
						printf("Something went seriously wrong with your threads\n");
						exit(-11);
						//runExperiment(STOP_EXPERIMENT);
					}
                   
                }
                
				//not sure if we need to, but let's give the thread a 
				//moment to set up, because we break immediately after:
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
		updateServer();

		//carve out a result array:
		double *resultArray = malloc(thisExperiment.numScans*sizeof(double));
		if(!resultArray) {
			printf("we didnt get the memory\n");
		}
		
		//i'm so happy this works:
		listNode *cur;
		int i = 0;
		for(cur = spectrumList; cur != NULL; cur = cur->nextNode) {
			resultArray[i++] = findPeakValueWavelength(wavelengths,cur->array);
		}
		
		
		//open the index file and write a serialized spec struct to it:
		expIndex = safelyOpenIndex();
		char *indexString = specStructToIndexString(thisExperiment);
		fprintf(expIndex,indexString);

		//printf everything to our file:
		printf("trying to write result file...\n");
		writeExperimentFile(expFile,spectrumList,resultArray);

		//tidy up and return to idling:
		fclose(expFile);
		fclose(expIndex);
		
		char buf[512];

		//this beautiful line replaces the first line with the new experimentcount
		sprintf(buf,"(cd experiment_results; sed -e '1 s/.*.*/%i/g' INDEX > tmp; mv tmp INDEX)",++numSavedExperiments);
		system(buf);

		//list_print(spectrumList);
		printf("trying to free the memory\n");
		free(resultArray);
        list_destroy(spectrumList);
        inited = 0;
        experimentState = IDLE;
        updateServer();

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
	} else if(s == WRITING_RESULTS) {
		return "Performing post-processing/peak detection...";
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



//highly slimmed-down linked list of double arrays
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
			
			//allocate a new node and set up its contents
			listNode *tmp = malloc(sizeof(listNode));
			if(!tmp) {
				printf("we didnt get the memory for a node :(\n");
				while(1);
			}
			
			
			tmp->nextNode = NULL;
			for(i = 0; i < NUM_WAVELENGTHS; i++) {
				tmp->array[i] = doubleArray[i];
			}
			
			printf("added item %i to list\n",count);
			cur->nextNode = tmp;
			
			return head;
		}
	
		
}

//free the whole list
static void list_destroy(listNode *head) {
	if(head == NULL) {
		printf("List destroyed!\n");
		return;
	}
	list_destroy(head->nextNode);
	free(head);
}

//recursive one-way iterator? why not!! 
static void list_print_recurse(listNode *head, int count) {
	if (head == NULL) {
		printf("end list_print\n");
		return;
	}
	printf("first 3 of array %i = %f %f %f\n",count,head->array[0],head->array[1],head->array[2]);
	list_print_recurse(head->nextNode,++count);
}

//want to recurse without supplying count? we got you.
static void list_print(listNode *head) {
	if (head == NULL) {
		printf("list is empty\n");
		return;
	}
	list_print_recurse(head,0);
}

//this is where we do the peak detection work;
//or rather, where we have python do it! 
static double findPeakValueWavelength(double *wavelengths, double *intensities) {
	
	char str[256];
	double peakWavelength = 0;
	float low = 0, high = 0, raw_peak = 0;
	int peakIndex = 0;
	FILE *rawData = fopen("./raw_data.txt","w");
	
	if(!rawData || wavelengths == NULL || intensities == NULL) {
		printf("file problems or bad array!\n");
		return 0;
	}
	

	//loop through and get the index of the peak
	for(int i = 0; i < NUM_WAVELENGTHS; i++) {
		if (intensities[i] > raw_peak) {
			raw_peak = intensities[i];
			peakIndex = i;
		}
	}
	//printf("found peak %f with index %i\n",raw_peak,peakIndex);
	
	//iterate forwards to get high bound:
	int i = peakIndex + 1;
	while(i < NUM_WAVELENGTHS) {
		if(intensities[i] <= .98 * raw_peak) {
			high = wavelengths[i];
			break;
		}
		i++;
	}
	
	//iterate backwards to get low bound:
	i = peakIndex - 1;
	while(i >= 0) {
		if(intensities[i] <= .98 * raw_peak) {
			low = wavelengths[i];
			break;
		}
		i--;
	}
	
	
	for(int i = 0; i < NUM_WAVELENGTHS; i++) {
		double wave = wavelengths[i];
		double intens = intensities[i];
		if (wave >= low && wave <= high ) {
			fprintf(rawData,"%.2lf	%.2lf\n",wave,intens);
		}
	}
	//fprintf(rawData,"DATA END");
	
	printf("done writing to file! working with window = %.2f nm\n",high-low);
	
		fclose(rawData);
	
	printf("Now starting python...\n");
	system("sudo python3 ./PeakDetector.py");
	
	//now python's output file will be fitted:
	FILE *fittedData = fopen("./peak_result.txt","r");
	if(!fittedData) {
		printf("file problems!\n");
		exit(-1);
	}
	
	fscanf(fittedData,"%lf",&peakWavelength);
	fclose(fittedData);
	
	printf("done!! we found wavelength = %.2lf\n", peakWavelength);

	
    return peakWavelength;

	
}


//write the measurements and results to the file
static void writeExperimentFile(FILE *f, listNode *head, double *results) {
	char line[2048] = "";
	char tmp[2048];

	//line = specStruct2descriptor OR SOMETHING
	fprintf(f,"EXPERIMENT HEADER\n");

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

//when first creating a new index file we need to make sure to place
//the header at the top
static FILE *safelyOpenIndex() {
	
		//using "r" should return null if doesn't exist
		FILE *fptr =  fopen("./experiment_results/INDEX","r");
			if (!fptr) {
				//the file doesn't exist, so create it wwith "w"
				printf("creating index!\n");
				fptr = fopen("./experiment_results/INDEX", "w");
				fprintf(fptr, "0\n");
				numSavedExperiments = 0;
				fclose(fptr);
			} else {
				fscanf(fptr,"%i",&numSavedExperiments);
				printf("we think there are %i experiment results here\n",numSavedExperiments);
				fclose(fptr);
			}

	//finally, open it for appending and return:
	return fopen("./experiment_results/INDEX","a");
	
	
	}








