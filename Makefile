CC=gcc
CFLAGS=-Wall -Wextra -Wchar-subscripts -Werror
OMP=-fopenmp
NOMP=-Wno-unknown-pragmas
OPTIM_FLAGS=-O3

SOURCES=src/source.c src/fast_read.c
EXECUTABLE=source.out
	
all:
	$(CC) $(CFLAGS) $(OPTIM_FLAGS) $(NOMP) $(SOURCES) -o $(EXECUTABLE)
dbg:
	$(CC) $(CFLAGS) $(OMP) $(SOURCES) -o $(EXECUTABLE)
omp:
	$(CC) $(CFLAGS) $(OPTIM_FLAGS) $(OMP) $(SOURCES) -o $(EXECUTABLE)
noflags:
	$(CC) $(SOURCES) -o $(EXECUTABLE)

clean:
	$(RM) $(EXECUTABLE)

