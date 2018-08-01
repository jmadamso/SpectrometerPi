/*
 * BTServerC
 * 
 * Receives a socket from the python service and serves the client,
 * responding to commands 
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

#include "./spectrometerDriver.h"
#include "./experimentFSM.h"

#define PHONE_MAC 88:AD:D2:F1:A2:83
#define PI_MAC B8:27:EB:AF:AC:37

#define PRESSURE_READING_RATE 750

int getClient();
int sendStringToClient(int Client, char *string);

static FILE *log;
static int pressureThreadRunning = 0;
static int spectraThreadRunning = 0;

int main(int argc, char **argv)
{
    char inBuf[1024];
    char outBuf[1024];
	char pressureReadingString[128];



    int i, k;
    int notCreated;
	
    int deviceConnected = 0;
	
    int toggle = 1;
    int bytes_read;

    int serverSock = 0, client = 0;

    

    //NumScans;Time between;Integration time; boxcar width; averages; result
    specSettings mySpec = {5, 60, 1000, 0, 3, "Dr. Smith", "John Deere", 0, 0};

    /*spectraThread
     * When started, beams several strings containing spectrum data
     * String delimited by ';'
     * [command][index offset];[reading]; (*8)
     */
    PI_THREAD(spectraThread)
    {
        int index, offset = 0, k = 0;
        double specBuffer[NUM_WAVELENGTHS];

        char specString[256] = "";
        char tmpBuf[128] = "";

        //get a reading and place it into our buffer
        //if spec not connected, default to buffer y = x
        getSpectrometerReading(specBuffer);

        //now iterate through and apend 8 readings per string
        //send index and then 8 values for offsets 0-7
        for (index = 0; index < NUM_WAVELENGTHS; index += 8) {

            sprintf(specString, "%c%i;", REQUEST_SPECTRA, index);
            for (offset = 0; offset < 7; offset++) {
                sprintf(tmpBuf, "%.2f;", specBuffer[index + offset]);
                strcat(specString, tmpBuf);
            }
            //and leave the ';' out of last entry:
            sprintf(tmpBuf, "%.2f", specBuffer[index + 7]);
            strcat(specString, tmpBuf);

            sendStringToClient(client, specString);
            //delay(15);
            k++;
        }
        printf("finished data stream! %i Strings sent\n", k);
		
		//if we are streaming readings, fire up another one of these threads before return
		spectraThreadRunning = 0;
		if(spectraThreadRunning) {
			int notCreated = piThreadCreate(spectraThread);
                    if (notCreated) {
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
		if(!experimentIsInited()) {
			initExperiment(mySpec);
		}
		
		specSettings s = getExperimentSettings();
		
		sprintf(outBuf,"%c;%i;%s;%s;%i;%i;%i;%i;%i", 
						EXP_STATUS,
						experimentRunning(),
						s.doctorName,
						s.patientName,
						s.numScans, 
						s.timeBetweenScans,
                        s.integrationTime, 
						s.boxcarWidth, 
						s.avgPerScan);
		//sendStringTo Client returns 1 if successful write
		deviceConnected = sendStringToClient(client,outBuf);
        
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
                sendStringToClient(client, "Turning on motor...\n");
                motor_ON();
                break;

            case MOTOR_OFF:
                sendStringToClient(client, "Turning off motor...\n");
                motor_OFF();
                break;

            case LED_ON:
                sendStringToClient(client, "Turning on LED...\n");
                led_ON();
                break;

            case LED_OFF:
                sendStringToClient(client, "Turning off LED...\n");
                led_OFF();
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

            case REQUEST_SPECTRA:

                //if this command comes, start the thread to transmit spectrum
                //sendStringToClient(client, "Received spectrum request...\n");
				spectraThreadRunning = 0;
                notCreated = piThreadCreate(spectraThread);
                if (notCreated) {
                    printf("pi thread failed somehow!\n");
                    exit(5);
                }
                break;

            case SETTINGS:

                //if this command comes, we expect to sync settings. read them
                //in from the message to the struct.
                //** Reading from &inbuf[1] because 1st char contains the command itself

                //NumScans;Time between;Integration time; boxcar width; averages
                sscanf(&inBuf[1], "%i;%i;%i;%i;%i", &mySpec.numScans, &mySpec.timeBetweenScans,
                        &mySpec.integrationTime, &mySpec.boxcarWidth,
                        &mySpec.avgPerScan);
                applySpecSettings(mySpec);
                printSpecSettings(mySpec);
                break;

            case CALIBRATE:
                //start or stop an infinite spectrum stream thread here
				spectraThreadRunning = spectraThreadRunning ? 0 : 1;
				notCreated = piThreadCreate(spectraThread);
                    if (notCreated) {
                        printf("pi thread failed somehow!\n");
                        exit(5);
                    }
                break;

			//start and stop commands -- 
            case EXP_START:
				applySpecSettings(mySpec);
                initExperiment(mySpec);
                runExperiment(START_EXPERIMENT);
                break;

            case EXP_STOP:
                runExperiment(STOP_EXPERIMENT);
                break;

			//if the user wants status, create a worker thread to beam it over
			case EXP_STATUS:
				notCreated = piThreadCreate(statusThread);
                    if (notCreated) {
                        printf("pi thread failed somehow!\n");
                        exit(5);
                    }
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




    // close connection
    //fprintf(log, "SESSION END\n\n");
    printf("SESSION END\n");
    //allow thread to close. Not sure if we need to do this but
    //it doesn't hurt.


    return 0;
}

/*
 * getClient
 * Accepts an open server socket, listens on the socket for connections,
 * and returns the client it finds. 
 */
int getClient(int serverSock)
{

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
    fprintf(stderr, "accepted connection from %s\n", inBuf);
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
        //this is very unlikely to happen, as the read will notice first
        printf("Client Disconnected. Noticed upon write.\n");
        return 0;
    } else {
        //we want to return 1 if still running
        printf("Response: %s\n", buf);
        //fprintf(log, "Response: %s\n", buf);
        return 1;
    }
}



