# vchannel
video channel surveillance module

vchannel is a GUI application that does the following:-
	- retrieves a single video stream from an IP network camera using RTSP/RTP
	- buffers the stream in memory and (optionally) perodically saves it to disk
	- performs motion detection on the stream
	- displays the stream image in a window
	- converts the stream images into JPEG so they can be retrieved using REST/HTTP

To build and run the application you will need Qt 4.8. environment.  

This application is only supported on Linux, but it may also work quite easily on Mac and Windows.
This has been tested on Ubuntu (14.04), Debian (Wheezy, Jessie), and Raspbian

Dependencies
------------

Qt4.8: libqt4-network libqtcore4 libqtgui4

For MJPEG:
    libjpeg8 or libjpeg62-turbo
    
For H264:
    libavformat libavcodec libswscale libavutil

Building the application
------------------------

For Debian: Install the following packages
    qt4-dev-tools
    gcc g++ make autoconf 
    libjpeg62-turbo-dev (or libjpeg8-devlibavformat-dev libavutil-dev libswscale-dev libavcodec-dev equivs


Download the source code using: 
    > git clone https://github.com/opennetcam/vchannel.git

Change into the vchannel directory

    > cd vchannel-master

	> make qmake
	
or, to get a debug build:
	> src/qmake vchannel-d.pro

	> make clean all

If you are running on a Debian platform, you can install 
from the opennetcam_1.0.deb package:

    > sudo dpkg -i debian/opennetcam_1.0_all.deb

and then to load dependencies:
    > apt-get install -f

