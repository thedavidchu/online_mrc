#include <stdio.h>
#include <glib.h>

int main(void) {
    printf("Hello, World!\n");
    #ifdef G_OS_UNIX
        printf("UNIX!\n");
    #endif
    return 0;
}