#!/bin/bash

cp ../Makefile ./Makefile
mkdir ./src/
cp ../src/source.cpp ./src/source.cpp
tar -cvf ~/submission.tar.gz README Makefile run.sh compile.sh ./src/source.cpp
