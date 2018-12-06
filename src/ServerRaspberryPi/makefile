# Makefile for Home automation project

CC=g++
CFLAGS=-I. -I/usr/local/include/RF24 -L/usr/local/lib -lrf24
DEPS = CommonAutomation.h
OBJ=PiTest_HelloWorld.o
PCOBJ=PiTest_HelloWorld_FROM_PC.o
TOBJ=CodeFragments.c
SOBJ=Server_HomeAutomation.o
EXACUTABLES=pitest

all:
				@echo "You need to select the right file"
				@ls -l

%.o: %.c $(DEPS)
				$(CC) -c -o $@ $< $(CFLAGS)

pitest: $(TOBJ)
				$(CC) -Wall -o $@ $^ $(CFLAGS)

pipc:		$(PCOBJ)
				$(CC) -Wall -o $@ $^ $(CFLAGS)

pimain: $(SOBJ)
				$(CC) -Wall -o $@ $^ $(CFLAGS)

clean:
				$(RM) *.o *~ $(OBJ) $(PCOBJ) $(EXACUTABLES)
