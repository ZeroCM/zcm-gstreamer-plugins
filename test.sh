#!/bin/bash

handler() {
    kill $(jobs -rp) &>/dev/null
    wait
    exit 1
}
trap handler SIGINT SIGTERM

WINDOWS=

jpeg_test() {
    gst-launch-1.0 videotestsrc pattern=ball ! videoconvert ! jpegenc ! zcmimagesink channel=JPEG_TEST &
    gst-launch-1.0 zcmimagesrc channel=JPEG_TEST ! jpegdec ! videoconvert ! autovideosink &
    WINDOWS="$WINDOWS $!"
}

rgb_test() {
    gst-launch-1.0 videotestsrc pattern=ball ! videoconvert ! 'video/x-raw,format=RGB' ! zcmimagesink channel=RGB_TEST &
    gst-launch-1.0 zcmimagesrc channel=RGB_TEST ! videoconvert ! autovideosink &
    WINDOWS="$WINDOWS $!"
}

jpeg_test
rgb_test

wait $WINDOWS
kill $(jobs -rp)
wait
