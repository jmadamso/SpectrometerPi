/*
 * BTServerC
 * 
 * 
 * before compiling, stop the OS from running this with:
 * sudo systemctl stop BTServer
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <time.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>

#include "./include/spectrometerDriver.h"
#include "./include/experimentFSM.h"


#define PHONE_MAC 88:AD:D2:F1:A2:83
#define PI_MAC B8:27:EB:AF:AC:37

#define PRESSURE_READING_RATE 750

static int getClient();
static int sendStringToClient(int client, char *string); 
static int sendDoubleArrayToClient(int client,double *arr, char command);
static char *specStructToCommandString(specSettings s);
static specSettings CommandStringToSpecStruct(char *cmdStr);




static FILE *log;
static int pressureThreadRunning = 0;
static int spectraThreadRunning = 0;

int main(int argc, char **argv)
{
    char inBuf[1024];
    char outBuf[1024];
    char pressureReadingString[128];
    char dn[1024], pn[1024], ts[1024];



    int i, k;
    int notCreated;

    int deviceConnected = 0;

    int toggle = 1;
    int bytes_read;

    int serverSock = 0, client = 0;



    //NumScans;Time between;Integration time; boxcar width; averages; result
    specSettings mySpec = {5, 60, 1000, 0, 3, "PI_DEFAULT_DR", "PI_DEFAULT_PAT","12_31_91_2359"};

    /*spectraThread
     * When started, beams several strings containing spectrum data
     * String delimited by ';'
     * [command][index offset];[reading]; (*8)
     */
    PI_THREAD(spectraThread)
    {
        double specBuffer[NUM_WAVELENGTHS];

        //get a reading and place it into our buffer
        //if spec not connected, default to buffer y = x
        getSpectrometerReading(specBuffer);

		//now zap it over:
		sendDoubleArrayToClient(client, specBuffer,SNAPSHOT);

        //if we are streaming readings, fire up another one of these threads before return
        //spectraThreadRunning = 0;
        if (spectraThreadRunning) {
            int notCreated = piThreadCreate(spectraThread);
            if (piThreadCreate(spectraThread)) {
                printf("pi thread failed somehow!\n");
                exit(5);
            }
        }

    }

    /*pressureThread
     * when started, streams pressure readings at specified rate.
     * Terminates when main() sets pressureThreadRunning back to 0. 
     */
    PI_THREAD(pressureThread)
    {
        while (pressureThreadRunning) {
            sprintf(pressureReadingString, "%c%i", REQUEST_PRESSURE, getPressureReading());
            sendStringToClient(client, pressureReadingString);
            delay(PRESSURE_READING_RATE);
        }
    }
    





    /*statusThread
     * when started, streams info regarding the current experiment
     */
    PI_THREAD(statusThread)
    {
		char buffer[1024];

		//start with some default settings that we don't really care
		//about if the experiment is idle.
		specSettings s = {0,0,0,0,0,"","",""};
        if (experimentIsInited()) {
			s = getExperimentSettings();	
		}
		
		strcpy(buffer,specStructToCommandString(s));
        deviceConnected = sendStringToClient(client, buffer);
        //printf("sending status string: %s\n",outBuf);
    }
    
    //and a simple wrapper to start it.
    //using this so that we have a function pointer
    //to send to the FSM process
    int startStatusThread() {
		return piThreadCreate(statusThread) ? 1 : 0;
	}




    //main loop: continually seek a connection and fire off threads
    //to handle it
    while (1) {
        sprintf(inBuf, "./log_%s.txt", "blerp");
        log = fopen(inBuf, "a");
        if (!log) {
            exit(-1);
        }

        serverSock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
        client = getClient(serverSock);
        deviceConnected = 1;

        while (deviceConnected) {

            // prepare a clean buffer... 
            memset(inBuf, 0, sizeof (inBuf));
            //...and read data from the client into inBuf
            bytes_read = read(client, inBuf, sizeof (inBuf));

            if (bytes_read > 0) {
                printf("received [%s]\n", inBuf);
                //fprintf(log, "received [%s]\n", inBuf);
            } else {
                printf("Client has disconnected. Noticed upon Read.\n");
                deviceConnected = 0;
                break;
            }

            //big main switch statement here switching on command char:
            switch (inBuf[0]) {

            case MOTOR_ON:
                //sendStringToClient(client, "Turning on motor...\n");
                motor_ON();
                break;

            case MOTOR_OFF:
                //sendStringToClient(client, "Turning off motor...\n");
                motor_OFF();
                break;

            case LED_ON:
                //sendStringToClient(client, "Turning on LED...\n");
                led_ON();
                break;

            case LED_OFF:
                //sendStringToClient(client, "Turning off LED...\n");
                led_OFF();
                break;

			//wen exiting the hardware screen, reset everything
			case HARDWARE_OFF:
				pressureThreadRunning = 0;
				spectraThreadRunning = 0;
				led_OFF();
				motor_OFF();
			
				break;

            case REQUEST_PRESSURE:

                //if this command comes, start up the thread to
                //continually send pressure readings
                if (pressureThreadRunning) {
                    pressureThreadRunning = 0;
                } else {
                    pressureThreadRunning = 1;
                    notCreated = piThreadCreate(pressureThread);
                    if (notCreated) {
                        printf("pi thread failed somehow!\n");
                        exit(5);
                    }
                }
                break;

            case SNAPSHOT:

                //if this command comes, start the thread to transmit spectrum
                //sendStringToClient(client, "Received spectrum request...\n");
                spectraThreadRunning = 0;
                notCreated = piThreadCreate(spectraThread);
                if (notCreated) {
                    printf("pi thread failed somehow!\n");
                    exit(5);
                }
                break;

			case START_STREAM:
				spectraThreadRunning = 1;
				notCreated = piThreadCreate(spectraThread);
                if (notCreated) {
                    printf("pi thread failed somehow!\n");
                    exit(5);
                }
				break;
				
			case STOP_STREAM:
				spectraThreadRunning = 0;
				break;
	
            case SETTINGS:

                //if this command comes, we expect to receive settings. read them
                //in from the message to the struct.
                
                //Read from &inbuf[1] because 1st char contains the command itself

                //NumScans;Time between;Integration time; boxcar width; averages
                
                //commandStringToSpecStruct(string,mySpec);
                sscanf(&inBuf[1], "%i;%i;%i;%i;%i;%[^\n]", &mySpec.numScans, &mySpec.timeBetweenScans,
                        &mySpec.integrationTime, &mySpec.boxcarWidth, &mySpec.avgPerScan,
                         outBuf);
                
                printf("trying to tokenize %s\n",outBuf);
                
                //since sscanf is finnicky with strings, we just scan
                //in one above, then tokenize that big string, knowing what
                //order they will be in. Clunky but functional. 
                //strsep catches empty strings so we use that 
                //char *ptr = strtok(outBuf,";");                
                char *ptr = strtok(outBuf,";");
                if(ptr) {
					strcpy(dn,ptr);
				}
                ptr = strtok(NULL,";");
                if(ptr) {
					strcpy(pn,ptr);
				}
				
                ptr = strtok(NULL,";");
                if(ptr) {
					strcpy(ts,ptr);
				}
				
				mySpec.doctorName = dn;
                mySpec.patientName = pn;
                mySpec.timestamp = ts;
				
				applySpecSettings(mySpec);
                printSpecSettings(mySpec);
                
				//now that we have gotten the strings, check to see
				//if we also want to start the experiment:
				ptr = strtok(NULL,";");
				
				if(ptr && strcmp(ptr,"Engage thrusters")) {
					//if we get here, the command string included
					//a request to start the experiment. 
					initExperiment(mySpec, startStatusThread);
					runExperiment(START_EXPERIMENT);
				} 


                break;

            case EXP_STOP:
                runExperiment(STOP_EXPERIMENT);
                break;

                //if the user wants status, create a worker thread to beam it over
            case EXP_STATUS:        
                if (piThreadCreate(statusThread)) {
                    printf("pi thread failed somehow!\n");
                    exit(5);
                }
                break;
                
            case EXP_LIST:; //semicolon lets us declare vars in a switch statement
				char *savedExperiments[10] = {"hey","friend","here","is","a","list","of","experiments","for","you"};  //= getExperimentList();
				int numSavedExperiments = 10; //getExperimentListLength();
				char buf[512];

				
				for(int i = 0; i < numSavedExperiments; i++) {
					
					if(i == 0) {
						sprintf(buf,"%c;Header",EXP_LIST);
						sendStringToClient(client,buf);
						delay(50);
						}
						
					sprintf(buf,"%c%s",EXP_LIST,savedExperiments[i]);
					sendStringToClient(client,buf);
					delay(50);
					}
					
				sprintf(buf,"%c;Footer",EXP_LIST);
				sendStringToClient(client,buf);

				
				
				break;

            case 'F':
                deviceConnected = sendStringToClient(client, "You have found a debug message! hehe :)\n");
                break;

            default:
                if ((int) inBuf[0] == 0) {
                    printf("got null\n", client);
                }
                deviceConnected = sendStringToClient(client, "Unrecognized Inbound Message!!\n");
                break;
            }
        }

        printf("restarting listening loop!\n");

        pressureThreadRunning = 0;
        spectraThreadRunning = 0;

        close(client);
        close(serverSock);
        fclose(log);

    }//end main listening loop

	//we will almost certainly never get here: 
    printf("SESSION END\n");

    return 0;
}

/*
 * getClient
 * Accepts an open server socket, listens on the socket for connections,
 * and returns the client it finds. 
 */
static int getClient(int serverSock)
{
	
	//ideally this would securely open a connection but nonfunctional:
	//return system("sudo ./py/advertiser.py");
	
	
	//doing it in c WITHOUT uuid:
	
    char inBuf[1023];
    // allocate socket
    struct sockaddr_rc loc_addr = {0}, rem_addr = {0};
    socklen_t opt = sizeof (rem_addr);


    // bind socket to port 1 of the first available 
    // local bluetooth adapter
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = *BDADDR_ANY;
    loc_addr.rc_channel = (uint8_t) 1;
    printf("Attempting to bind socket...\n");


    bind(serverSock, (struct sockaddr *) &loc_addr, sizeof (loc_addr));

    // put socket into listening mode
    printf("Listening for connections...\n");

    listen(serverSock, 1);

    // accept one connection
    int client = accept(serverSock, (struct sockaddr *) &rem_addr, &opt);

    ba2str(&rem_addr.rc_bdaddr, inBuf);
    fprintf(stderr, "accepted connection from %s with client = %i\n", inBuf,client);
    //fprintf(log, "accepted connection from %s\n", inBuf);

    return client;
    
}

/*
 * Sends input string, up to 256 in lengh, across bluetooth client.
 * returns 1 if connected and message sent; else returns 0
 */
int sendStringToClient(int client, char *string)
{
    int err;
    char buf[256];
    memset(buf, 0, sizeof (buf));
    sprintf(buf, string);
    err = write(client, buf, strlen(buf));

    if (err < 0) {
        //we want to return 0 if the client isn't there anymore.
        //eg, status thread tries to run while the researcher is eating lunch
        printf("Client Disconnected. Noticed upon write.\n");
        return 0;
    } else {
        //we want to return 1 if still running
        printf("Sent to client: %s\n", buf);
        //fprintf(log, "Response: %s\n", buf);
        return 1;
    }
}

int sendDoubleArrayToClient(int client,double *arr, char command) {
			char specString[256] = "";
			char tmpBuf[128] = "";
			int index, offset = 0, k = 0;
			int retVal;
        
        //now iterate through and apend 8 readings per string
        //send index and then 8 values for offsets 0-7
        for (index = 0; index < NUM_WAVELENGTHS; index += 8) {

            sprintf(specString, "%c%i;", command, index);
            for (offset = 0; offset < 7; offset++) {
                sprintf(tmpBuf, "%.2f;", arr[index + offset]);
                strcat(specString, tmpBuf);
            }
            //and leave the ';' out of last entry:
            sprintf(tmpBuf, "%.2f", arr[index + 7]);
            strcat(specString, tmpBuf);

			//this guy will return 1 when it succeeds
            if(sendStringToClient(client, specString) == 0) {
				return 0;
			}
            
            //delay(15);
            k++;
			}
			printf("finished data stream! %i Strings sent\n", k);
			return 1;
			
		}


static char *specStructToCommandString(specSettings s) {
			static char buffer[256];
			sprintf(buffer, "%c%i;%s;%s;%i;%i;%i;%i;%i;%s\n",
                EXP_STATUS,
                experimentRunning(),
                s.doctorName,
                s.patientName,
                s.numScans,
                s.timeBetweenScans,
                s.integrationTime,
                s.boxcarWidth,
                s.avgPerScan,
                getExpStatusMessage());
                
                return buffer;
		}
		
static specSettings CommandStringToSpecStruct(char *cmdStr) {
			
		}

