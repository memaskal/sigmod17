CC=gcc
CFLAGS=-Wall -Wextra -O3

EXECUTABLE=source
	
all :
	$(CC) $(CFLAGS) src/$(EXECUTABLE).c -o $(EXECUTABLE).out

clean:
	$(RM) $(EXECUTABLE).out
