CFLAGS?=-std=c++11 -Wall -O2 -g
INC=-Isrc/wiringPi/wiringPi
LIB=-lcurl -ljansson -lwiringPi

default: all

all: bin bin/run

test: bin bin/test

bin:
	mkdir bin

bin/run: bin/i2c8Bit.o bin/mcp3008Spi.o src/main.cpp
	g++ $(CFLAGS) $(INC) $(LIB) bin/i2c8Bit.o bin/mcp3008Spi.o src/main.cpp -o bin/run

bin/mcp3008Spi.o: src/mcp3008Spi.cpp
	g++ $< $(CFLAGS) $(INC) -c -o bin/mcp3008Spi.o

bin/i2c8Bit.o: src/i2c8Bit.cpp
	g++ $< $(CFLAGS) $(INC) -c -o bin/i2c8Bit.o

bin/test: bin/mcp3008Spi.o src/test.cpp	
	g++ $(CFLAGS) $(INC)$(LIB)  bin/mcp3008Spi.o src/test.cpp -o bin/test 

clean:
	rm -r bin

