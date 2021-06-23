CFLAGS=`pkg-config --cflags gstreamer-1.0 gstreamer-video-1.0 zcm` -I build
LIBS=`pkg-config --libs gstreamer-1.0 gstreamer-video-1.0 zcm`
TYPESLIB=-L build/zcmtypes -l zcmtypes

ZCMGEN=zcm-gen -c --c-cpath build/zcmtypes --c-hpath build/zcmtypes --c-include zcmtypes --c-typeinfo

$(shell mkdir -p build/imagesink build/imagesrc build/snap build/multifilesink build/zcmtypes)

test: all
	@LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:./build/zcmtypes/ \
		gst-inspect-1.0 ./build/imagesink/gstzcmimagesink.so
	@LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:./build/zcmtypes/ \
		gst-inspect-1.0 ./build/imagesrc/gstzcmimagesrc.so
	@LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:./build/zcmtypes/ \
		gst-inspect-1.0 ./build/snap/gstzcmsnap.so
	@LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:./build/zcmtypes/ \
		gst-inspect-1.0 ./build/multifilesink/gstzcmmultifilesink.so

all: examples zcmtypes core

core: zcmtypes
	@gcc -Wall -Werror -fPIC $(CFLAGS) -c \
		-o build/imagesink/gstzcmimagesink.o src/imagesink/gstzcmimagesink.c
	@gcc -shared -o build/imagesink/gstzcmimagesink.so \
		build/imagesink/gstzcmimagesink.o $(TYPESLIB) $(LIBS)
	@gcc -Wall -Werror -fPIC $(CFLAGS) -c \
		-o build/imagesrc/gstzcmimagesrc.o src/imagesrc/gstzcmimagesrc.c
	@gcc -shared -o build/imagesrc/gstzcmimagesrc.so \
		build/imagesrc/gstzcmimagesrc.o $(TYPESLIB) $(LIBS)
	@gcc -Wall -Werror -fPIC $(CFLAGS) -c \
		-o build/snap/gstzcmsnap.o src/snap/gstzcmsnap.c
	@gcc -shared -o build/snap/gstzcmsnap.so \
		build/snap/gstzcmsnap.o $(TYPESLIB) $(LIBS)
	@gcc -Wall -Werror -fPIC $(CFLAGS) -c \
		-o build/multifilesink/gstzcmmultifilesink.o src/multifilesink/gstzcmmultifilesink.c
	@gcc -shared -o build/multifilesink/gstzcmmultifilesink.so \
		build/multifilesink/gstzcmmultifilesink.o $(TYPESLIB) $(LIBS)


debug: zcmtypes
	@gcc -Wall -Werror -fPIC -g $(CFLAGS) -c \
		-o build/imagesink/gstzcmimagesink.o src/imagesink/gstzcmimagesink.c
	@gcc -shared -g -o build/imagesink/gstzcmimagesink.so \
		build/imagesink/gstzcmimagesink.o $(TYPESLIB) $(LIBS)
	@gcc -Wall -Werror -fPIC -g $(CFLAGS) -c \
		-o build/imagesrc/gstzcmimagesrc.o src/imagesrc/gstzcmimagesrc.c
	@gcc -shared -g -o build/imagesrc/gstzcmimagesrc.so \
		build/imagesrc/gstzcmimagesrc.o $(TYPESLIB) $(LIBS)
	@gcc -Wall -Werror -fPIC -g $(CFLAGS) -c \
		-o build/snap/gstzcmsnap.o src/snap/gstzcmsnap.c
	@gcc -shared -g -o build/snap/gstzcmsnap.so \
		build/snap/gstzcmsnap.o $(TYPESLIB) $(LIBS)
	@gcc -Wall -Werror -fPIC -g $(CFLAGS) -c \
		-o build/multifilesink/gstzcmmultifilesink.o src/multifilesink/gstzcmmultifilesink.c
	@gcc -shared -g -o build/multifilesink/gstzcmmultifilesink.so \
		build/multifilesink/gstzcmmultifilesink.o $(TYPESLIB) $(LIBS)


zcmtypes:
	@$(ZCMGEN) src/zcmtypes/image_t.zcm
	@$(ZCMGEN) src/zcmtypes/snap_t.zcm
	@$(ZCMGEN) src/zcmtypes/photo_t.zcm
	@gcc -Wall -Werror -fPIC $(CFLAGS) -c \
		-o build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_image_t.o \
		build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_image_t.c
	@gcc -Wall -Werror -fPIC $(CFLAGS) -c \
		-o build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_snap_t.o \
		build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_snap_t.c
	@gcc -Wall -Werror -fPIC $(CFLAGS) -c \
		-o build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_photo_t.o \
		build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_photo_t.c
	@gcc -shared -o build/zcmtypes/libzcmtypes.so \
		build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_image_t.o \
	  	build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_snap_t.o \
	    build/zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_photo_t.o $(LIBS)
examples: zcmtypes
	@gcc -o build/snap/example-pub $(CFLAGS) \
		src/snap/example_pub.c $(LIBS) -L build/zcmtypes -l zcmtypes

clean:
	@rm -rf ./build/*
