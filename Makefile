CC = gcc
CFLAGS = -Wall -Wextra -g

backup: backup_util.c
	$(CC) $(CFLAGS) -o backup backup_util.c

clean:
	rm -f backup
