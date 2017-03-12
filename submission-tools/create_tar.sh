#!/bin/bash

rm ./Makefile
cp ../Makefile ./Makefile
mkdir ./src/
rm ./src/source.cpp
cp ../src/source.cpp ./src/source.cpp
tar -cvf ~/submission.tar.gz README Makefile run.sh compile.sh ./src/source.cpp
