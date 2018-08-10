all: BTServer specDriver.o exp.o
BTServer: BTServer.c specDriver.o exp.o 
	gcc -W BTServer.c specDriver.o exp.o -o BTServer -lbluetooth -lseabreeze -lusb -lwiringPi
	#make clean

specDriver.o: ./src/spectrometerDriver.c
	gcc -c ./src/spectrometerDriver.c -o specDriver.o 
	
exp.o: ./src/experimentFSM.c
	gcc -c ./src/experimentFSM.c -o exp.o

clean:
	rm *.o
