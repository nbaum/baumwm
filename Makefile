CXX=clang++ -std=c++11 -g
LD=clang++ -std=c++11

all: admiral.bin

admiral.bin: main.o
	$(LD) $(LDFLAGS) $(shell pkg-config --libs x11 xinerama) -o$@ $^

%.o: %.cpp
	$(CXX) -c -o$@ $(CXXFLAGS) $<

test: all
	DISPLAY=:1 ./bwm

clean:
	$(RM) admiral.bin main.o

main.cpp: util.h

