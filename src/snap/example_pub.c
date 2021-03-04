#include <unistd.h>

#include <zcm/zcm.h>
#include "zcmtypes/zcm_gstreamer_plugins/zcm_gstreamer_plugins_snap_t.h"

int main(int argc, char *argv[])
{
    zcm_t *zcm = zcm_create("");
    if (!zcm)
        return 1;

    zcm_gstreamer_plugins_snap_t snap = {};
    if (argc > 1) snap.debounce = atoi(argv[1]);

    int j;
    for (j = 0; j < 10; ++j) {
        int i;
        for (i = 0; i < 10; ++i) {
            snap.utime++;
            zcm_gstreamer_plugins_snap_t_publish(zcm, "GSTREAMER_SNAP", &snap);
            usleep(100 * 1000);
        }
        snap.debounce++;
    }

    zcm_destroy(zcm);
    return 0;
}
