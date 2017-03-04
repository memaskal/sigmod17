cd datasets

# Run our executable with small input
if [ -z ${1+x} ]; then # No shell input. Normal execute
	./harness.out small.init small.work small.result ../source.out
else # Forward $1 to harness
	./harness.out small.init small.work small.result ../source.out "$1"
fi

