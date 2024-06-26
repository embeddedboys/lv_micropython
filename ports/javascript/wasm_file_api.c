#define _POSIX_C_SOURCE
#include <stdio.h>
#include <string.h>
#include "py/mpconfig.h"

#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#endif

// VERY BAD
int
wasm_file_open(const char *url) {
    //fprintf(stderr,"10:wasm_file_open[%s]\n", url);

    /* Skip same host lookups, we don't use them at LVGL */
    /*
    if (strlen(url)>1 && url[0]==':') {
        fprintf(stderr,"wasm_file_open -> same host[%s]\n", url);
        int fidx = EM_ASM_INT({return window.wasm_file_open(UTF8ToString($0)); }, url );

        // TODO rebuild local path in ramdisk relative to getcwd()
        char fname[MICROPY_ALLOC_PATH_MAX];

        snprintf(fname, sizeof(fname), "cache_%d", fidx);
        return fileno( fopen(fname,"r") );
    }
    */
    /* treat everything else as a remote host */
    /*if ( (strlen(url)>6) && (url[4]==':' || url[5]==':') ) {*/
    fprintf(stderr,"wasm_file_open -> remote host[%s]\n", url);
    int fidx = EM_ASM_INT({return window.wasm_file_open(UTF8ToString($0)); }, url );
    char fname[20];
    snprintf(fname, sizeof(fname), "cache_%d", fidx);
    return fileno( fopen(fname,"r") );
    /*}*/
    //return 0;
}

