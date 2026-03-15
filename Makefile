compile:
	clear
	g++ -O0 -c main.cpp -Wall
	g++ -O0 -c glad.c
	g++ main.o glad.o -lglfw -lassimp

run: compile
	./a.out

test: compile
	./a.out --benchmark

testthingy: compile
	./a.out --benchmark --thingy10k

CXXFLAGS_PERF = -O3 -march=native -mtune=native
performance:
	clear
	g++ -c main.cpp $(CXXFLAGS_PERF)
	g++ -c glad.c $(CXXFLAGS_PERF)
	g++ main.o glad.o -lglfw -lassimp $(CXXFLAGS_PERF)

clean:
	rm -f *.out
	rm -rf cache
	mkdir cache cache/batty cache/naive cache/jumpflood cache/raymap cache/octree

cachesize:
	@du -sh cache	
