#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>

#include "Fader.h"
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

// Thread control and communication
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static int terminate = 0;
static int grabColors[5] = {0};
static int updateColors[5] = {0};

static int grabDone()
{
	int index;
	pthread_mutex_lock(&mutex);
	for (index = 0; index < 5; ++index)
		updateColors[index] = grabColors[index];
	pthread_cond_broadcast(&cond);
	pthread_mutex_unlock(&mutex); 
}

static int grabLoop()
{
    int r;
    r = grabber_initialize();
    if (r != 0) return ErrorExit(r);

    int fpsCount = 0;
    suseconds_t elapsed = 0;
    struct timeval starttime;
    struct timeval now;
    gettimeofday(&starttime, NULL);
    while(!terminate)
    {
        r = grabber_begin();
        if (r != 0) return ErrorExit(r);

        int i;
        int x2 = 0;
        unsigned int hist[256];
        for (i = 1; i <= 5; ++i) // 5 regions from left to right
        {
	    // speed up by processing only half the lines, gives "purple"!
#	    define SPEEDSHIFT_LUMA 4
#	    define SPEEDSHIFT_CHROMA 2

    	    int x1 = x2;
	    x2 = ((i * luma.width) / 5) & 0xFFFFFFFE; // even pixels only (for chroma)
            //printf("Luma size: %dx%d (stride: %d)\n", luma.width, luma.height, luma.stride);
    	    int avgY = avg(luma.data, x1, x2, luma.stride << SPEEDSHIFT_LUMA, luma.height >> SPEEDSHIFT_LUMA);

    	    memset(hist, 0, sizeof(hist));
	    int chromacount = (chroma.width*chroma.height/2) >> SPEEDSHIFT_CHROMA;
    	    histogram2(chroma.data, 0, chroma.width, chroma.stride << SPEEDSHIFT_CHROMA, chroma.height >> SPEEDSHIFT_CHROMA, hist);
    	    int mU = median(hist, chromacount);
    	    memset(hist, 0, sizeof(hist));
    	    histogram2(chroma.data+1, 0, chroma.width, chroma.stride << SPEEDSHIFT_CHROMA, chroma.height >> SPEEDSHIFT_CHROMA, hist);
    	    int mV = median(hist, chromacount);

            // printf("Region (%3d..%3d) avgY=%d mU=%d mV=%d color=#%06x\n", x1, x2, avgY, mU, mV, YUV2RGB(avgY, mU, mV));
		
	    grabColors[i-1] = YUV2RGB(avgY, mU, mV);
	}
        r = grabber_end();
	grabDone(); // Broadcast that we have finished
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
}

static void* startGrabLoop(void* arg)
{
    return (void*)grabLoop();
}

static unsigned int tick()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000 + (now.tv_usec / 1000);
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
 
    pthread_t grabberThreadId;
    r = pthread_create(&grabberThreadId, NULL, startGrabLoop, NULL);
    if (r != 0)
    {
	fprintf(stderr, "pthread_create failed to create grab thread, code: %d\n", r);
	return 2;
    }

    Fader fader;
    fader_init(&fader, 5*3); // 5 RGB triplets
    int i;
    for (i = 0; i < 5*3; ++i)
    {
    	fader.target[i] = 128; // fade to gray
    }
    unsigned int now = tick();
    fader_commit(&fader, now, now + 5000); // in 5 seconds...
    for (;;)
    {
	static const region2light[5] = {0, 2, 3, 4, 1};
	for (i=0; i<5; ++i)
	{
		int color = 
			((int)fader.current[i*3  ] << 16) |
			((int)fader.current[i*3+1] << 8 ) |
			((int)fader.current[i*3+2]      );
		ambx_set_light(id, region2light[i], color);
		// printf("#%02x%02x%02x", fader.current[i*3  ], fader.current[i*3+1], fader.current[i*3+2]);
	}
	//printf("\n");
	struct timespec abstime;
        clock_gettime(CLOCK_REALTIME, &abstime);
        abstime.tv_nsec += 20000000;
	if (abstime.tv_nsec > 1000000000)
	{
		abstime.tv_sec += 1;
		abstime.tv_nsec-= 1000000000;
	}
	pthread_mutex_lock(&mutex);
	int ret = pthread_cond_timedwait(&cond, &mutex, &abstime);
	now = tick();
	if (ret == ETIMEDOUT)
	{
		// no new data (probably)
		//printf(".\n");
	}
	else
	{
		//printf("Data!\n");
		for (i = 0; i < 5; ++i)
		{
			fader.target[3*i    ] = (byte)((updateColors[i] >> 16) & 0xFF);
			fader.target[3*i + 1] = (byte)((updateColors[i] >> 8) & 0xFF);
			fader.target[3*i + 2] = (byte)(updateColors[i] & 0xFF);
			fader_commit(&fader, now, now + 500);
		}
	}	
	pthread_mutex_unlock(&mutex); 
	fader_update(&fader, now);
    }

    // Terminate grabber
    terminate = 1;
    pthread_join(grabberThreadId, (void**)&r);

    return r;
}
