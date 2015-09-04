# vchannel
video channel surveillance module

vchannel is a GUI application that does the following:-
	- retrieves a single video stream from an IP network camera using RTSP/RTP
	- buffers the stream in memory and (optionally) perodically saves it to disk
	- performs motion detection on the stream
	- displays the stream image in a window
	- converts the stream images into JPEG so they can be retrieved using REST/HTTP

To build the application you will need Qt 4.8.  

This application is only supported on Linux, but it may also work quite easily on Mac and Windows.

Building the application
------------------------

Download the source code using 

Change into the vchannel directory
	> qmake vchannel.pro
	> make clean all


