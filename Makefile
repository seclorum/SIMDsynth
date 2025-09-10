simdsynth:	simdsynth.cpp
	clang++ -O3 -std=c++17 simdsynth.cpp -o simdsynth

test:	simdsynth
	./simdsynth | play -t raw -r 48000 -e floating-point -b 32 -c 1 -

clean:
	rm -rf *.o *.?~ simdsynth
