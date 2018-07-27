/* ExperimentFSM
 * Interface to the state machine which will govern device behavior.
 * Receives commands from the server and runs the experiment.
 * 
 * 
 */ 
 
 #include "./spectrometerDriver.h"

enum {
	WAITING,
	EXP_STARTING,
	EXP_IN_PROGRESS,
	EXP_FINISHED
	} experimentState;
 

int initExperiment(specSettings spec,int BTSocket);

int runExperiment(char command, int BTSocket);

int getExperimentState();

