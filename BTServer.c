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

int sendStringToClient(int Client, char *string);

static FILE *log;
static int pressureThreadRunning = 0;
static int spectraThreadRunning = 0;

int main(int argc, char **argv)
{
    printf("BTServer started with socket = %s\n", argv[1]);

    int i, k;
    int notCreated;
    int running = 1;
    int toggle = 1;
    int bytes_read;
    int client = atoi(argv[1]);

    char inBuf[1024];
    char outBuf[1024];
    char pressureReadingString[128];

    //NumScans;Time between;Integration time; boxcar width; averages; result
    specSettings mySpec = {5, 60, 1000, 0, 3, "", "", 0};

    printf("sanity check, socket = %i\n", client);

    //THREAD DECLARATIONS GO HERE BECAUSE ITS ANNOYING TO DECLARE
    //THEM OUTSIDE OF main()

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
            delay(5);
            k++;
        }
        printf("finished data stream! %i Strings sent\n", k);

    }



    sprintf(inBuf, "./log_%s.txt", "blerp");
    log = fopen(inBuf, "a");
    if (!log) {
        exit(-1);
    }

    /* Now, start the main loop. Listen for bytes, then check
     * the first byte for a command code and switch accordingly.
     */
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
            break;

        case LED_ON:
            sendStringToClient(client, "Turning on LED...\n");
            LED_ON();
            break;

        case LED_OFF:
            sendStringToClient(client, "Turning off LED...\n");
            LED_OFF();
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
            break;

        case START:

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

    return 777;
}

/*
 * Sends input string, up to 256 in lengh, across bluetooth client
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
        fprintf(log, "Response: %s\n", buf);
        return 1;
    }
}



