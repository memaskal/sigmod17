
# Run our executable with small input
if [ -z ${1+x} ]; then # No shell input. Normal execute
	cd datasets
	./harness.out small.init small.work small.result ../source.out
else # Forward $1 to harness
	if [ $1 = "-v" ]; then
		cd datasets
		./harness.out small.init small.work small.result ../source.out -v
	elif [ $1 = "-l" ] && [ -e large_small.init ]; then
		cd datasets
		./harness.out large_small.init large_small.work large_small.result ../source.out
	elif [ $1 = "-prepare" ]; then
		echo "Making harness..."
		make
		echo "Making source.cpp..."
		cd ../
		make
		cd ./datasets
		echo "Unpacking large.tar.bz2..."
		tar -xvf large.tar.bz2
		echo "DONE. Make sure the source.cpp defines are correct and recompile it if you change anything."
	elif [ $1 = "-tar" ]; then
		echo "Making tar for submission..."
		cd submission-tools/
		./create_tar.sh
		cd ..
	else
		echo "Invalid option or missing files. Valid options are -v, -l, -prepare"
	fi
fi

