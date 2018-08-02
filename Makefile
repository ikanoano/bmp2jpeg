all: a.out
	./a.out ./splatoon-2.bmp
	ls -lah /tmp/out.jpg
	firefox /tmp/out.jpg

a.out: main.cpp consts.hpp
	g++ -std=c++17 -O3 main.cpp

