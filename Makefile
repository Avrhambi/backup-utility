CC = gcc
CFLAGS = -Wall -Wextra -g

OBJS = main.o backup.o

all: backup

backup: $(OBJS)
	$(CC) $(CFLAGS) -o backup $(OBJS)

main.o: main.c backup.h
	$(CC) $(CFLAGS) -c main.c

backup.o: backup.c backup.h
	$(CC) $(CFLAGS) -c backup.c

clean:
	rm -f backup $(OBJS)
