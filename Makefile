CFLAGS=`pkg-config --cflags gstreamer-1.0 gstreamer-video-1.0 zcm` -I build
LIBS=`pkg-config --libs gstreamer-1.0 gstreamer-video-1.0 zcm`

all: build zcmtypes
	gcc -Wall -Werror -fPIC $(CFLAGS) -c -o build/gstzcmsink.o gstzcmsink.c
	gcc -shared -o build/gstzcmsink.so build/gstzcmsink.o build/image_t.o $(LIBS)

debug: build zcmtypes
	gcc -Wall -Werror -fPIC -g $(CFLAGS) -c -o build/gstzcmsink.o gstzcmsink.c
	gcc -shared -o build/gstzcmsink.so -g build/gstzcmsink.o build/image_t.o $(LIBS)

zcmtypes: build
	zcm-gen -c image_t.zcm --c-cpath build --c-hpath build --c-typeinfo
	gcc -Wall -Werror -fPIC -g $(CFLAGS) -c -o build/image_t.o build/image_t.c
	gcc -shared -o build/libzcmtypes.so -g build/image_t.o $(LIBS)

test: all
	gst-inspect-1.0 ./gstzcmsink.so

build:
	mkdir -p build

clean:
	rm -rf ./build/*
