# make all

all: 
	make -C src 
	make -C debian
clean: 
	make -C src clean
	make -C debian clean
qmake:
	qmake src/vchannel.pro -o src/Makefile

