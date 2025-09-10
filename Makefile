simdsynth:	simdsynth.cpp
	clang++ -O3 simdsynth.cpp -o simdsynth


clean:
	rm -rf *.o *.?~ simdsynth
