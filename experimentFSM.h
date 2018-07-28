/* ExperimentFSM.h
 * Interface to the state machine which will govern device behavior.
 * Receives commands from the server and runs the experiment.
 * 
 * 
 */

#include "./spectrometerDriver.h"

enum FSM_commands {
    SELF,
	TIMEOUT,
	START_EXPERIMENT,
	STOP_EXPERIMENT
	
};


//initialize an experiment with a bundle of experiment
//settings, as well as the socket we want to communicate on
int initExperiment(specSettings spec);

//run the experiment with an incomming command. 
int runExperiment(char command);

int experimentRunning();

int experimentIsInited();

