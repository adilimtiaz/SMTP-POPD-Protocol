CC=gcc
CFLAGS=-g -Wall -std=gnu99

all: mysmtpd mypopd

mysmtpd: mysmtpd.o netbuffer.o mailuser.o server.o smtpsession.o helpers.o
mypopd: mypopd.o netbuffer.o mailuser.o server.o popsession.o helpers.o

mysmtpd.o: mysmtpd.c netbuffer.h mailuser.h server.h smtpsession.h helpers.h
mypopd.o: mypopd.c netbuffer.h mailuser.h server.h popsession.h helpers.h

netbuffer.o: netbuffer.c netbuffer.h
mailuser.o: mailuser.c mailuser.h
server.o: server.c server.h
helpers.o: helpers.c helpers.h
smtpsession.o: smtpsession.c smtpsession.h
popsession.o: popsession.c popsession.h

clean:
	-rm -rf mysmtpd mypopd mysmtpd.o mypopd.o netbuffer.o mailuser.o server.o
cleanall: clean
	-rm -rf *~
