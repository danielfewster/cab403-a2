CC = gcc
CFLAGS = -Wall -Wextra -Werror 
LDFLAGS = -lrt -lpthread

all: simulator manager fire_alarm

simulator.o manager.o: common.h
manager.o: manager.h
simulator.o: simulator.h
fire_alarm.o: core.h

simulator: simulator.o
manager: manager.o
fire_alarm: fire_alarm.o

clean: 
	rm -f simulator
	rm -f manager
	rm -f fire_alarm
	rm -f *.o

.PHONY: all clean
