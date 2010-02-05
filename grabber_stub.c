#include "grabber.h"

Bitmap luma;
Bitmap chroma;

// Called once when starting up.
int grabber_initialize()
{
    return 0;
}

// Called at each frame, before using luma and chroma
int grabber_begin()
{
    return 0;
}

// Called when done processing luma and chroma
int grabber_end()
{
    return 0;
}

// Called on program shutdown.
int grabber_destroy()
{
    return 0;
}

