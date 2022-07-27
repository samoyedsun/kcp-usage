g++ -c ikcp.c -o ikcp.o
g++ -c main.cpp -o main.o
g++ main.o ikcp.o -o bin
