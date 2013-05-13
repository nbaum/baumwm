CXX=clang++ -std=c++11
LD=clang++ -std=c++11

all: bwm

bwm: main.o
	$(LD) $(LDFLAGS) $(shell pkg-config --libs x11) -o$@ $^

%.o: %.cpp
	$(CXX) -c -o$@ $(CXXFLAGS) $<

test: bwm
	DISPLAY=:1 ./bwm

clean:
	$(RM) bwm main.o

depend: main.cpp
	$(CXX) $(CXXFLAGS) -M main.cpp > Makefile.depends

include Makefile.depends
