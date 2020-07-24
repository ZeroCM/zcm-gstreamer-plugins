CFLAGS=`pkg-config --cflags gstreamer-1.0 gstreamer-video-1.0 zcm` -I build
LIBS=`pkg-config --libs gstreamer-1.0 gstreamer-video-1.0 zcm`

$(shell mkdir -p build/imagesink build/zcmtypes)

test: all
	gst-inspect-1.0 ./build/imagesink/gstzcmimagesink.so

all: build zcmtypes
	gcc -Wall -Werror -fPIC $(CFLAGS) -c -o build/imagesink/gstzcmimagesink.o src/imagesink/gstzcmimagesink.c
	gcc -shared -o build/imagesink/gstzcmimagesink.so build/imagesink/gstzcmimagesink.o build/zcmtypes/libzcmtypes.so $(LIBS)

debug: build zcmtypes
	gcc -Wall -Werror -fPIC -g $(CFLAGS) -c -o build/imagesink/gstzcmimagesink.o src/imagesink/gstzcmimagesink.c
	gcc -shared -g -o build/imagesink/gstzcmimagesink.so build/imagesink/gstzcmimagesink.o build/zcmtypes/libzcmtypes.so $(LIBS)

zcmtypes: build
	zcm-gen -c --c-cpath build/zcmtypes --c-hpath build/zcmtypes --c-include zcmtypes --c-typeinfo src/zcmtypes/image_t.zcm
	gcc -Wall -Werror -fPIC $(CFLAGS) -c -o build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_image_t.o build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_image_t.c
	gcc -shared -o build/zcmtypes/libzcmtypes.so build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_image_t.o $(LIBS)

build:
	mkdir -p build

clean:
	rm -rf ./build/*
