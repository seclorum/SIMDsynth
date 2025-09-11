# Detect architecture
ARCH := $(shell uname -m)

# Compiler and flags
CXX = clang++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra -Wpedantic
ifeq ($(ARCH), x86_64)
    CXXFLAGS += -msse -msse2 -msse4.1
else ifeq ($(ARCH), arm64)
    CXXFLAGS += -march=armv8-a+simd
endif

# Targets
simdsynth: simdsynth.cpp
	$(CXX) $(CXXFLAGS) simdsynth.cpp -o simdsynth

test: simdsynth
	./simdsynth | play -t raw -r 48000 -e floating-point -b 32 -c 1 -

clean:
	rm -rf *.o *~ simdsynth
