/* ExperimentFSM.h
 * Interface to the state machine which will govern device behavior.
 * Receives commands from the server and runs the experiment.
 * 
 * 
 */
 #ifndef FSM_H
 #define FSM_H
 

#include "./spectrometerDriver.h"

enum FSM_commands {
    SELF,
	TIMEOUT,
	START_EXPERIMENT,
	STOP_EXPERIMENT
};

//initialize an experiment with a bundle of experiment
//settings, as well as the socket we want to communicate on.
//we pass in whatever update method the server wants us to use: 
int initExperiment(specSettings spec, int (*updateFunction)());
int experimentIsInited();

//run the experiment with an incomming command. 
int runExperiment(char command);

//return true if running
int experimentRunning();

//return the settings being used currently
specSettings getExperimentSettings();

//returns a human readable string describing current experiment status
char *getExpStatusMessage();

#endif


