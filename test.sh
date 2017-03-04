cd datasets

# Run our executable with small input
if [ -z ${1+x} ]; then # No shell input. Normal execute
	./harness small.init small.work small.result ../source.out
else # Forward $1 to harness
	./harness small.init small.work small.result ../source.out "$1"
fi

