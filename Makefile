CFLAGS=`pkg-config --cflags gstreamer-1.0 gstreamer-video-1.0 zcm` -I build
LIBS=`pkg-config --libs gstreamer-1.0 gstreamer-video-1.0 zcm`

ZCMGEN = zcm-gen -c --c-cpath build/zcmtypes --c-hpath build/zcmtypes --c-include zcmtypes --c-typeinfo

$(shell mkdir -p build/imagesink build/snap build/zcmtypes)

test: all
	@gst-inspect-1.0 ./build/imagesink/gstzcmimagesink.so
	@gst-inspect-1.0 ./build/snap/gstzcmsnap.so

all: examples zcmtypes core build

core: zcmtypes build
	@gcc -Wall -Werror -fPIC $(CFLAGS) -c -o build/imagesink/gstzcmimagesink.o src/imagesink/gstzcmimagesink.c
	@gcc -shared -o build/imagesink/gstzcmimagesink.so build/imagesink/gstzcmimagesink.o build/zcmtypes/libzcmtypes.so $(LIBS)
	@gcc -Wall -Werror -fPIC $(CFLAGS) -c -o build/snap/gstzcmsnap.o src/snap/gstzcmsnap.c
	@gcc -shared -o build/snap/gstzcmsnap.so build/snap/gstzcmsnap.o build/zcmtypes/libzcmtypes.so $(LIBS)

debug: build zcmtypes
	@gcc -Wall -Werror -fPIC -g $(CFLAGS) -c -o build/imagesink/gstzcmimagesink.o src/imagesink/gstzcmimagesink.c
	@gcc -shared -g -o build/imagesink/gstzcmimagesink.so build/imagesink/gstzcmimagesink.o build/zcmtypes/libzcmtypes.so $(LIBS)
	@gcc -Wall -Werror -fPIC -g $(CFLAGS) -c -o build/snap/gstzcmsnap.o src/snap/gstzcmsnap.c
	@gcc -shared -g -o build/snap/gstzcmsnap.so build/snap/gstzcmsnap.o build/zcmtypes/libzcmtypes.so $(LIBS)

zcmtypes: build
	@$(ZCMGEN) src/zcmtypes/image_t.zcm
	@$(ZCMGEN) src/zcmtypes/snap_t.zcm
	@$(ZCMGEN) src/zcmtypes/photo_t.zcm
	@gcc -Wall -Werror -fPIC $(CFLAGS) -c -o build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_image_t.o build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_image_t.c
	@gcc -Wall -Werror -fPIC $(CFLAGS) -c -o build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_snap_t.o build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_snap_t.c
	@gcc -Wall -Werror -fPIC $(CFLAGS) -c -o build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_photo_t.o build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_photo_t.c
	@gcc -shared -o build/zcmtypes/libzcmtypes.so build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_image_t.o \
												  build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_snap_t.o \
												  build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_photo_t.o $(LIBS)
examples: zcmtypes build
	@gcc -o build/snap/example-pub $(CFLAGS) src/snap/example_pub.c $(LIBS) -L build/zcmtypes -l zcmtypes

build:
	@mkdir -p build

clean:
	@rm -rf ./build/*
