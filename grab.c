#include <stdlib.h>
#include <stdio.h>
#include "grabber.h"

static int ErrorExit(int code)
{
  fprintf(stderr, "Fatal error code %d\n", code);
  return code;
}

int main(int argc, char** argv)
{
    int r;

    r = grabber_initialize();
    if (r != 0) return ErrorExit(r);

    r = grabber_begin();
    if (r != 0) return ErrorExit(r);

    r = grabber_end();
    if (r != 0) return ErrorExit(r);

    r = grabber_destroy();

    return r;
}
