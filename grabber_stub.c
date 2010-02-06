#include <memory.h>
#include "grabber.h"

Bitmap luma;
Bitmap chroma;

const int w = 30;
const int h = 10;
const int stride = 32;

static void createBitmap(Bitmap* bm, int stride, int width, int height)
{
    bm->data = malloc(stride * h);
    bm->width = w;
    bm->height = h;
    bm->stride = stride;
}

static void destroyBitmap(Bitmap* bm)
{
    free(bm->data);
    bm->data = 0;
    bm->stride = -1;
}

// Called once when starting up.
int grabber_initialize()
{
    createBitmap(&luma, stride, w, h);
    createBitmap(&chroma, stride, w, h/2);
    return 0;
}

// Called at each frame, before using luma and chroma
int grabber_begin()
{
    int x,y;
    unsigned char* d;
    for (y=0; y < h; ++y)
    {
	d = luma.data + (y*stride);
    	for (x=0; x < w; ++x)
	{
	    d[x] = (255*x)/(w-1);
    	}
    }
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
    destroyBitmap(&luma);
    destroyBitmap(&chroma);
    return 0;
}

