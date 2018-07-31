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





# Main loop
def main():
    
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
    socketNumber = None
    try:
        client_sock = None

        # This will block until we get a new connection
        client_sock, client_info = server_sock.accept()
        #and, if we have accepted a connection, go back invisible:
        os.system("hciconfig hci0 noscan")
        stop_advertising(server_sock)
        print "Accepted connection from ", client_info
        
        #write the socket to a file 
        socketNumber = client_sock.fileno()
        print "Socket = ",socketNumber
        socketFile = "./socket.txt"
        f = open(socketFile,'w')
        f.write(str(socketNumber))
        f.close()


    except IOError:
        pass
    client_sock.send("e" + str(777));
    
	#os._exit(0);

main()
