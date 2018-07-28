#!/usr/bin/python

#This script initiates a secure connection to the mobile app
#using PyBluez, and then passes the socket in to the C portion.
#
#This is run repeatedly by the os using the BTServerPython service,
#custom defined in /etc/systemd/system
# stop with: sudo systemctl stop BTServer
#

import logging
import logging.handlers
import argparse
import sys
import os
import time
from bluetooth import *


class LoggerHelper(object):
    def __init__(self, logger, level):
        self.logger = logger
        self.level = level

    def write(self, message):
        if message.rstrip() != "":
            self.logger.log(self.level, message.rstrip())


def setup_logging():
    # Default logging settings
    LOG_FILE = "./ServerLog.log"
    LOG_LEVEL = logging.INFO

    # Define and parse command line arguments
    argp = argparse.ArgumentParser(description="Raspberry PI Bluetooth Server")
    argp.add_argument("-l", "--log", help="log (default '" + LOG_FILE + "')")
    argp.add_argument("-e","--eff",type=int)

    # Grab the log file from arguments
    args = argp.parse_args()
    if args.log:
        LOG_FILE = args.log
    if args.eff == 5:
        print "wow!"

    # Setup the logger
    logger = logging.getLogger(__name__)
    # Set the log level
    logger.setLevel(LOG_LEVEL)
    # Make a rolling event log that resets at midnight and backs-up every 3 days
    handler = logging.handlers.TimedRotatingFileHandler(LOG_FILE,
        when="midnight",
        backupCount=3)

    # Log messages should include time stamp and log level
    formatter = logging.Formatter('%(asctime)s %(levelname)-8s %(message)s')
    # Attach the formatter to the handler
    handler.setFormatter(formatter)
    # Attach the handler to the logger
    logger.addHandler(handler)

    # Replace stdout with logging to file at INFO level
    #sys.stdout = LoggerHelper(logger, logging.INFO)
    # Replace stderr with logging to file at ERROR level
    #sys.stderr = LoggerHelper(logger, logging.ERROR)


# Main loop
def main():
    # Setup logging
    setup_logging()

    # We need to wait until Bluetooth init is done
    time.sleep(5)

    # Make device visible
    os.system("hciconfig hci0 piscan")
    

 
	

    # Create a new server socket using RFCOMM protocol
    server_sock = BluetoothSocket(RFCOMM)
    # Bind to any port
    server_sock.bind(("", PORT_ANY))
    # Start listening
    server_sock.listen(1)

    # Get the port the server socket is listening
    port = server_sock.getsockname()[1]

    # The service UUID to advertise
    uuid = "00001101-0000-1000-8000-00805F9B34FB"

    # Start advertising the service
    advertise_service(server_sock, "RaspiBtSrv",
                       service_id=uuid,
                       service_classes=[uuid, SERIAL_PORT_CLASS],
                       profiles=[SERIAL_PORT_PROFILE])

    print "Waiting for connection on RFCOMM channel %d" % port
    #print "Server socket = %i" % server_sock.fileno()
    try:
        client_sock = None

        # This will block until we get a new connection
        client_sock, client_info = server_sock.accept()
        #and, if we have accepted a connection, go back invisible:
        os.system("hciconfig hci0 noscan")
        stop_advertising(server_sock)
        print "Accepted connection from ", client_info
        
        #get the socket number as an integer
        socketNumber = client_sock.fileno()
        print "Socket = ",socketNumber
       

        # now that we connected, switch to C because it's better:
        argStr = "./BTServer %i" % socketNumber
        print "arg string = " + argStr
        os.system(argStr)

    except IOError:
        pass

    ##close the sockets because this script is one-shot
    client_sock.close()
    server_sock.close()

    print "Server going down"
       

main()
