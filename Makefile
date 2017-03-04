CC=gcc
CFLAGS=-Wall -Wextra -Wchar-subscripts -Werror
OMP=-fopenmp
NOMP=-Wno-unknown-pragmas
OPTIM_FLAGS=-O3


EXECUTABLE=source
	
all:
	$(CC) $(CFLAGS) $(OPTIM_FLAGS) $(NOMP) src/$(EXECUTABLE).c -o $(EXECUTABLE).out
dbg:
	$(CC) $(CFLAGS) $(OMP) src/$(EXECUTABLE).c -o $(EXECUTABLE).out
omp:
	$(CC) $(CFLAGS) $(OPTIM_FLAGS) $(OMP) src/$(EXECUTABLE).c -o $(EXECUTABLE).out
noflags:
	$(CC) src/$(EXECUTABLE).c -o $(EXECUTABLE).out

clean:
	$(RM) $(EXECUTABLE).out
