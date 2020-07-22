CFLAGS=`pkg-config --cflags gstreamer-1.0 gstreamer-video-1.0 zcm`
LIBS=`pkg-config --libs gstreamer-1.0 gstreamer-video-1.0 zcm`

all:
	gcc -Wall -Werror -fPIC $(CFLAGS) -c -o gstzcmsink.o gstzcmsink.c
	gcc -shared -o gstzcmsink.so gstzcmsink.o $(LIBS)

test: all
	gst-inspect-1.0 ./gstzcmsink.so
