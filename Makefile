#
# Makefile for todoesverso's JACK client 
#

### Varaibles ###
CC	= gcc -Wall
CODE	= client.c
BIN 	= client
LDFLAGS	=-ljack -lcurses
CFLAGS	=-O2 -msse2 -fwhole-program -ffast-math -pipe -fsigned-char -D_THREAD_SAFE -D_REENTRANT 

### Targets ###
all: $(CODE) $(BIN)

$(BIN): $(CODE)
	@ echo "*** Compiling $? ... ***"
	$(CC) -o $(BIN) $(CFLAGS) $(LDFLAGS) $?

clean:
	rm -f $(BIN)


PHONY	= clean
