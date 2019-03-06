all: expression pong

expression: expression.cpp
	clear; g++ expression.cpp -Wall -Wextra -pedantic -oexpression
pong: pong.cpp
	clear; g++ pong.cpp -Wall -lrt -lX11 -lGL -o pong


clean:
	rm -f expression
	rm -f pong

