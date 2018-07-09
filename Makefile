all: BTServer specDriver.o
BTServer: BTServer.c specDriver.o
	gcc -W BTServer.c specDriver.o -o BTServer -lbluetooth -lseabreeze -lusb -lwiringPi

specDriver.o: spectrometerDriver.c
	gcc -c spectrometerDriver.c -o specDriver.o 

clean:
	rm *.o
