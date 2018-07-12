#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <time.h>
#include <wiringPi.h>

#include "./spectrometerDriver.h"

#define PHONE_MAC 88:AD:D2:F1:A2:83
#define PI_MAC B8:27:EB:AF:AC:37

#define PRESSURE_READING_RATE 750

int sendStringToClient(int Client, char *string);
void spectraTest(int client);


static FILE *log;
static int pressureThreadRunning = 0;
static int spectraThreadRunning = 0;

static enum {
    MOTOR_ON = 49,
    MOTOR_OFF,
    REQUEST_PRESSURE,
    REQUEST_SPECTRA,
    SETTINGS,
    QUIT
} commands;

int main(int argc, char **argv)
{
    int i, k;
    int running = 1;

    //NumScans;Time between;Integration time; boxcar width; averages
    specSettings mySpec = {5, 60, 1000, 0, 3};

    struct sockaddr_rc loc_addr = {0}, rem_addr = {0};
    char inBuf[1024];
    char outBuf[1024];
    int s, client, bytes_read, err;
    socklen_t opt = sizeof (rem_addr);

    char timeString[128];
    int time;

    char pressureReadingString[128];

	/*pressureThread
	 * when started, streams pressure readings at specified rate.
	 * Terminates when main() sets pressureThreadRunning back to 0. 
	 */
    PI_THREAD(pressureThread)
    {
        while (pressureThreadRunning) {
            sprintf(pressureReadingString, "%c%i", REQUEST_PRESSURE, getPressureReading());
            //sprintf(pressureReadingString,"%c%i",REQUEST_PRESSURE,777);

            sendStringToClient(client, pressureReadingString);
            delay(PRESSURE_READING_RATE);
        }
    }

	/*spectraThread
	 * When started, beams one huge string to application.
	 * String delimited by ';'
	 */
	PI_THREAD(spectraThread)
    {
        int i;
		double specBuffer[NUM_WAVELENGTHS];
		
		char specString[9600] = "";
		char tmpBuf[128] = "";
		sprintf(specString,"%c",REQUEST_SPECTRA);
		
		//get a reading and place it into our buffer
		//if spec not connected, default to buffer y = x
		getSpectrometerReading(specBuffer);

		//now iterate through and apend each reading to this bigass string:
		//for (i = 0; i < 5; i++) { //only send 5 for debugging
		for (i = 0; i < NUM_WAVELENGTHS-1; i++) {
			sprintf(tmpBuf, "%.2f;", specBuffer[i]);       
			strcat(specString,tmpBuf);
		}
		//and leave the ';' out of last entry:
		sprintf(tmpBuf, "%.2f", specBuffer[i]);       
		strcat(specString,tmpBuf);
		
		sendStringToClient(client, specString);
		printf("finished data stream!\n");
    }

    PI_THREAD(overcurrentThread)
    {
        //junk here to continually read the current LEDs
    }

    sprintf(inBuf, "./log_%s.txt", "blerp");
    log = fopen(inBuf, "a");
    if (!log) {
        exit(-1);
    }

    // allocate socket
    s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

    // bind socket to port 1 of the first available 
    // local bluetooth adapter
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = *BDADDR_ANY;
    loc_addr.rc_channel = (uint8_t) 1;
    printf("Attempting to bind socket...\n");


    bind(s, (struct sockaddr *) &loc_addr, sizeof (loc_addr));

    // put socket into listening mode
    printf("Listening for connections...\n");

    listen(s, 1);

    // accept one connection
    client = accept(s, (struct sockaddr *) &rem_addr, &opt);

    ba2str(&rem_addr.rc_bdaddr, inBuf);
    fprintf(stderr, "accepted connection from %s\n", inBuf);
    fprintf(log, "accepted connection from %s\n", inBuf);

    while (running) {

        // prepare a clean buffer... 
        memset(inBuf, 0, sizeof (inBuf));
        //...and read data from the client into inBuf
        bytes_read = read(client, inBuf, sizeof (inBuf));

        if (bytes_read > 0) {
            printf("received [%s]\n", inBuf);
            fprintf(log, "received [%s]\n", inBuf);
        } else {
            printf("Client has disconnected. Noticed upon Read.\n");
            running = 0;
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

            static int toggle = 1;
            if (toggle) {
                LED_ON();
                //start current protection here thread here
                toggle = 0;
            } else {
                LED_OFF();
                toggle = 1;
            }

            break;

        case REQUEST_PRESSURE:
            //if this command comes, start up the thread to
            //continually send pressure readings

            if (pressureThreadRunning) {
                pressureThreadRunning = 0;
            } else {
                pressureThreadRunning = 1;
                int notCreated = piThreadCreate(pressureThread);
                if (notCreated) {
                    printf("pi thread failed somehow!\n");
                    exit(5);
                }
            }
            break;

        case REQUEST_SPECTRA:
            sendStringToClient(client, "Received spectrum request...\n");
            int notCreated = piThreadCreate(spectraThread);
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

        case QUIT:
        case 'q':
            sendStringToClient(client, "Now Quitting!\n");
            running = 0;
            break;

        case 'F':
            running = sendStringToClient(client, "You have found a debug message! hehe :^)\n");
            break;


        default:
            if ((int) inBuf[0] == 0) {
                printf("got null\n", client);
            }
            running = sendStringToClient(client, "Unrecognized Inbound Message!!\n");
            break;
        }

    }


    // close connection
    pressureThreadRunning = 0;
    fprintf(log, "SESSION END\n\n");
    printf("SESSION END\n");
    //allow thread to close. Not sure if we need to do this but
    //it doesn't hurt.
    delay(2000);
    fclose(log);
    close(client);
    close(s);
    return 0;
}

/*
 * Sends input string, up to 9600 in lengh, across bluetooth client
 */ 
int sendStringToClient(int client, char *string)
{
    int err;
    char buf[9600];
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
        fprintf(log, "Response: %s\n", buf);
        return 1;
    }

}



