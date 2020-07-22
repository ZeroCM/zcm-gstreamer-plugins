CFLAGS=`pkg-config --cflags gstreamer-1.0 gstreamer-video-1.0 zcm`
LIBS=`pkg-config --libs gstreamer-1.0 gstreamer-video-1.0 zcm`

all: zcmtypes
	gcc -Wall -Werror -fPIC $(CFLAGS) -c -o build/gstzcmsink.o gstzcmsink.c
	gcc -Wall -Werror -fPIC $(CFLAGS) -c -o build/image_t.o image_t.c
	gcc -shared -o build/gstzcmsink.so build/gstzcmsink.o build/image_t.o $(LIBS)

debug:
	gcc -Wall -Werror -fPIC -g $(CFLAGS) -c -o build/gstzcmsink.o gstzcmsink.c
	gcc -Wall -Werror -fPIC -g $(CFLAGS) -c -o build/image_t.o image_t.c
	gcc -shared -o build/gstzcmsink.so -g build/gstzcmsink.o build/image_t.o $(LIBS)

zcmtypes:
	zcm-gen -c image_t.zcm
	gcc -shared -o build/libzcmtypes.so -g build/image_t.o $(LIBS)

test: all
	gst-inspect-1.0 ./gstzcmsink.so

clean:
	rm -rf ./build/*
