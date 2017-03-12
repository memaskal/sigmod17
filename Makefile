CC=g++
CFLAGS=-Wall -Wextra -Wchar-subscripts -Werror -std=c++11
OMP=-fopenmp
NOMP=-Wno-unknown-pragmas
OPTIM_FLAGS=-O3

SOURCES=src/source.cpp
EXECUTABLE=source.out

all:
	$(CC) $(CFLAGS) $(OPTIM_FLAGS) $(OMP) $(SOURCES) -o $(EXECUTABLE)
dbg:
	$(CC) $(CFLAGS) $(OMP) $(SOURCES) -o $(EXECUTABLE)
nomp:
	$(CC) $(CFLAGS) $(OPTIM_FLAGS) $(NOMP) $(SOURCES) -o $(EXECUTABLE)
noflags:
	$(CC) $(SOURCES) -o $(EXECUTABLE)

clean:
	$(RM) $(EXECUTABLE)

