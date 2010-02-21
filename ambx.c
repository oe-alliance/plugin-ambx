#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <sys/time.h>
#include "grabber.h"
#include "yuvrgb.h"

static int ErrorExit(int code)
{
  fprintf(stderr, "Fatal error code %d\n", code);
  return code;
}

static unsigned int median(const unsigned int* hist, unsigned int count)
{
	count /= 2;
	const unsigned int* p = hist;
	unsigned int sum = *p;
	while (sum < count)
	{
		++p;
		sum += *p;
	}
	return (p - hist);
}

int main(int argc, char** argv)
{
    int r;

    if (ambx_init() < 0)
    {
        printf("ambx_init failed.\n");
        return -1;
    }

    int id = 0;
    if (ambx_open(id) < 0)
    {
        printf("ambx_open(%d) failed.\n", id);
        return -1;
    }

    r = grabber_initialize();
    if (r != 0) return ErrorExit(r);

    int fpsCount = 0;
    suseconds_t elapsed = 0;
    struct timeval starttime;
    struct timeval now;
    gettimeofday(&starttime, NULL);
    while(r==0)
    {
        r = grabber_begin();
        if (r != 0) return ErrorExit(r);

        int i;
        int x2 = 0;
        unsigned int hist[256];
        for (i = 1; i <= 5; ++i) // 5 regions from left to right
        {
	    // speed up by processing only half the lines, gives "purple"!
#	    define SPEEDSHIFT 0
    	    int x1 = x2;
	    x2 = ((i * luma.width) / 5) & 0xFFFFFFFE; // even pixels only (for chroma)
            //printf("Luma size: %dx%d (stride: %d)\n", luma.width, luma.height, luma.stride);
    	    int avgY = avg(luma.data, x1, x2, luma.stride << SPEEDSHIFT, luma.height >> SPEEDSHIFT);

    	    memset(hist, 0, sizeof(hist));
    	    histogram2(chroma.data, 0, chroma.width, chroma.stride << SPEEDSHIFT, chroma.height >> SPEEDSHIFT, hist);
    	    int mU = median(hist, chroma.width*chroma.height/2);
    	    memset(hist, 0, sizeof(hist));
    	    histogram2(chroma.data+1, 0, chroma.width, chroma.stride << SPEEDSHIFT, chroma.height >> SPEEDSHIFT, hist);
    	    int mV = median(hist, chroma.width*chroma.height/2);

            //printf("Region (%3d..%3d) avgY=%d mU=%d mV=%d ", x1, x2, avgY, mU, mV);
    	    //printRGB(avgY, mU, mV);
	    //printf("\n");
		
	    static const region2light[5] = {0, 2, 3, 4, 1};
	    int color = YUV2RGB(avgY, mU, mV);
	    ambx_set_light(id, region2light[i], color);
	}
        r = grabber_end();
        if (r != 0) return ErrorExit(r);

  	gettimeofday(&now, NULL);
	elapsed += (now.tv_sec - starttime.tv_sec) * 1000000;
	elapsed += now.tv_usec;
	elapsed -= starttime.tv_usec;
	++fpsCount;
	if (elapsed > 1000000)
	{
		printf("FPS: %d/10 (%d/%d)\n", (fpsCount * 10000000) / elapsed, fpsCount, elapsed);
		starttime = now;
		fpsCount = 0;
 		elapsed = 0;
        }
    }

    r = grabber_destroy();
    return r;
}
