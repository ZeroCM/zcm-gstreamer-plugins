CFLAGS=`pkg-config --cflags gstreamer-1.0 gstreamer-video-1.0 zcm` -I build
LIBS=`pkg-config --libs gstreamer-1.0 gstreamer-video-1.0 zcm`

test: all
	gst-inspect-1.0 ./build/gstzcmimagesink.so

all: build zcmtypes
	gcc -Wall -Werror -fPIC $(CFLAGS) -c -o build/gstzcmimagesink.o src/zcmimagesink/gstzcmimagesink.c
	gcc -shared -o build/gstzcmimagesink.so build/gstzcmimagesink.o build/zcm_gstreamer_plugins/zcm_gstreamer_plugins_image_t.o $(LIBS)

debug: build zcmtypes
	gcc -Wall -Werror -fPIC -g $(CFLAGS) -c -o build/gstzcmimagesink.o src/zcmimagesink/gstzcmimagesink.c
	gcc -shared -o build/gstzcmimagesink.so -g build/gstzcmimagesink.o build/zcm_gstreamer_plugins/zcm_gstreamer_plugins_image_t.o $(LIBS)

zcmtypes: build
	zcm-gen -c src/image_t.zcm --c-cpath build --c-hpath build --c-typeinfo
	gcc -Wall -Werror -fPIC -g $(CFLAGS) -c -o build/zcm_gstreamer_plugins/zcm_gstreamer_plugins_image_t.o build/zcm_gstreamer_plugins/zcm_gstreamer_plugins_image_t.c
	gcc -shared -o build/libzcmtypes.so -g build/zcm_gstreamer_plugins/zcm_gstreamer_plugins_image_t.o $(LIBS)

build:
	mkdir -p build

clean:
	rm -rf ./build/*
