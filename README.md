# haxor

# README: 
Make now forces -Werror!
There is still a pragma -Wchar-subscript override in the code that needs to be fixed.
Binary file for harness has been removed from the repo (Whoops).
Harness now supports valgrind.

## OpenMP:
To build with OpenMP use: make omp
"make all" doesn't use omp yet because the results are incorrect

## Valgrind support has been added. (Need to check if all pipes work properly through it)
#### Instructions
* g++ datasets/harness.cpp -o datasets/harness -O3 (recompiles harness)
* sudo apt-get install valgrind
* ./test.sh -v

Valgrind is slow.

## Makefile modes:
* make all
* make dbg (force use openmp, don't use -O3)
* make omp (force use openmp, use -O3)
* make noflags (don't use any flags)
