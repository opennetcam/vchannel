# make all
VERSIONQT := $(shell expr `qmake -query QT_VERSION | cut -f1 -d.` )

all: 
	make -C src 
	make -C debian
clean: 
	make -C src clean
	make -C debian clean
qmake:
ifeq "$(VERSIONQT)" "4"
	qmake src/vchannel.pro -o src/Makefile
else
	echo incorrect Qt version
endif
