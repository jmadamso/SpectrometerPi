all: BTServer specDriver.o
BTServer: BTServer.c specDriver.o exp.o
	gcc -W BTServer.c specDriver.o exp.o -o BTServer -lbluetooth -lseabreeze -lusb -lwiringPi
	make clean

specDriver.o: spectrometerDriver.c
	gcc -c spectrometerDriver.c -o specDriver.o 
	
exp.o: experimentFSM.c
	gcc -c experimentFSM.c -o exp.o

clean:
	rm *.o
