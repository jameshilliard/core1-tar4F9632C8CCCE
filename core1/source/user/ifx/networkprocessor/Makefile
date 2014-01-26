#all: tcpmessages
CC	= mips-linux-gcc
AR      = mips-linux-ar
CFLAGS	= ${IFX_CFLAGS}

all: tcpmessages

tcpmessages: Socket.o SocketCommunication.o 
	$(CC) -gcc -o tcpmessages Socket.o SocketCommunication.o 

#dependencies

Socket.o:  Socket.c SocketCommunication.h 
 
#
#       Transition rules (add -o to put object in right directory)
#
.c.o:
	$(CC) -c -o $@ $(DEBUG) $(CFLAGS) $(IFLAGS) $<

clean:
	rm -f *.o tcpmessages
