cd datasets

# Run our executable with small input
./harness small.init small.work myOut.result source.out && diff myOut.result small.result | head -10
