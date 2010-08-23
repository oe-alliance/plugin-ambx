#include <Python.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>

#include "Fader.h"
#include "grabber.h"
#include "yuvrgb.h"
#include "ambxlib.h"
#include "colorproc.h"

#define SHOW_FPS

static int faderSpeed = 250; // in ms
static unsigned int fps_frames = 0; // for FPS calculation
static unsigned int fps_usec = 0;

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
static int terminateGrabber = 0;
static int grabColors[5] = {0};
static int updateColors[5] = {0};
static int updateTrigger = 0; // informs updater that we sent a new value

static void grabDone(void)
{
	int index;
	pthread_mutex_lock(&mutex);
	for (index = 0; index < 5; ++index)
		updateColors[index] = grabColors[index];
	++updateTrigger;
	pthread_cond_broadcast(&cond);
	pthread_mutex_unlock(&mutex); 
}

static int grabLoop(void)
{
    nice(15); // don't hog the CPU
    int r;
#ifdef SHOW_FPS
    int fpsCount = 0;
    suseconds_t elapsed = 0;
    struct timeval starttime;
    struct timeval now;
    gettimeofday(&starttime, NULL);
#endif
    while(!terminateGrabber)
    {
        r = grabber_begin();
        if (r != 0) 
	{
		// grabbing failed, try again in half a second.
		usleep(500000);
		continue;
	}

        int i;
        int x2 = 0;
        unsigned int hist[256];
        for (i = 1; i <= 5; ++i) // 5 regions from left to right
        {
	    // speed up by processing less lines
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

#ifdef SHOW_FPS
  	gettimeofday(&now, NULL);
	elapsed += (now.tv_sec - starttime.tv_sec) * 1000000;
	elapsed += now.tv_usec;
	elapsed -= starttime.tv_usec;
	++fpsCount;
	if (elapsed > 2000000)
	{
		fps_frames = fpsCount;
		fps_usec = elapsed;
		starttime = now;
		fpsCount = 0;
 		elapsed = 0;
        }
#endif
    }

    return 0;
}

static void* startGrabLoop(void* arg)
{
    return (void*)grabLoop();
}

static unsigned int tick(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000 + (now.tv_usec / 1000);
}

static int terminateOutput = 0;
static int ambxId = 0;


static int run(void)
{
    Fader fader;
    fader_init(&fader, 5*3);
    int i;
    for (i = 0; i < 5*3; ++i)
    {
    	fader.target[i] = 0;
    }
    int currentTrigger = updateTrigger-1;
    int currentColors[5] = {0};
    unsigned int now = tick();
    fader_commit(&fader, now, now); 
    while (!terminateOutput)
    {
	struct timespec abstime;
        clock_gettime(CLOCK_REALTIME, &abstime);
        abstime.tv_nsec += 25000000;
	if (abstime.tv_nsec > 1000000000)
	{
		abstime.tv_sec += 1;
		abstime.tv_nsec-= 1000000000;
	}
	static const int region2light[5] = {0, 2, 3, 4, 1};
	for (i=0; i<5; ++i)
	{
		int color = 
			((int)fader.current[i*3  ] << 16) |
			((int)fader.current[i*3+1] << 8 ) |
			((int)fader.current[i*3+2]      );
		if (currentColors[i] != color)
		{
			ambx_set_light(ambxId, region2light[i], color);
			currentColors[i] = color;
		}
	}
	pthread_mutex_lock(&mutex);
	pthread_cond_timedwait(&cond, &mutex, &abstime);
	now = tick();
	if (currentTrigger != updateTrigger)
	{
		currentTrigger = updateTrigger;
		//printf("Data!\n");
		for (i = 0; i < 5; ++i)
		{
			fader.target[3*i    ] = (byte)((updateColors[i] >> 16) & 0xFF);
			fader.target[3*i + 1] = (byte)((updateColors[i] >> 8) & 0xFF);
			fader.target[3*i + 2] = (byte)(updateColors[i] & 0xFF);
		}
		fader_commit(&fader, now, now + faderSpeed);
	}	
	pthread_mutex_unlock(&mutex); 
	fader_update(&fader, now);
    }

    // turn off the lights when shutting down.
    for (i = 0; i < 5; ++i)
    {
	ambx_set_light(ambxId, i, 0);
    }
    return 0;
}


static void* startRun(void* arg)
{
    return (void*)run();
}


static pthread_t outputThreadId;
static pthread_t grabberThreadId;


static PyObject *startOutput(PyObject *self, PyObject *args)
{
    terminateOutput = 0;
    if (ambx_open(ambxId) < 0)
    {
        PyErr_SetString(PyExc_RuntimeError, "ambx_open failed, probably no kit connected");
        return NULL;
    }
 
    int r = pthread_create(&outputThreadId, NULL, startRun, NULL);
    if (r != 0)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create output thread");
	return NULL;
    }
	
    return Py_BuildValue("i", outputThreadId);
}

static PyObject *startGrabber(PyObject *self, PyObject *args)
{
    terminateGrabber = 0;

    grabber_flags |= FLAG_COARSE; // no need to be precise
    int r = grabber_initialize();
    if (r != 0)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to initialize grabber");
	return NULL;
    }
    r = pthread_create(&grabberThreadId, NULL, startGrabLoop, NULL);
    if (r != 0)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create grabber thread");
        return NULL;
    }

    return Py_BuildValue("i",grabberThreadId);
}


static void stopThread(pthread_t threadId)
{
    int r;
    pthread_join(threadId, (void**)&r);
}

static PyObject *stopOutput(PyObject *self, PyObject *args)
{
    terminateOutput = 1;
    stopThread(outputThreadId);
    ambx_close(ambxId);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *stopGrabber(PyObject *self, PyObject *args)
{
    terminateGrabber = 1;
    stopThread(grabberThreadId);
    int r = grabber_destroy();
    if (r != 0)
    {
        PyErr_SetString(PyExc_RuntimeError, "grabber_destroy returned error");
	return NULL;
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* setLight(PyObject *self, PyObject *args)
{
	int i;
	int color;
	if (PyArg_ParseTuple(args, "i", &color))
	{
		for (i = 0; i < 5; ++i)
		{
			grabColors[i] = color;
		}
	}
	else if (PyArg_ParseTuple(args, "iiiii",  grabColors+0, grabColors+1, grabColors+2, grabColors+3, grabColors+4))
	{
		// set all colors...
	}
	else
	{
		return NULL;
	}
	grabDone();
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* getFadeTime(PyObject *self, PyObject *args)
{
	return Py_BuildValue("i", faderSpeed);
}


static PyObject* setFadeTime(PyObject *self, PyObject *args)
{
        int fadeTime;
        if (!PyArg_ParseTuple(args, "i", &fadeTime))
                return NULL;
	faderSpeed = fadeTime;
        Py_INCREF(Py_None);
        return Py_None;
}

static PyObject* getFpsStats(PyObject *self, PyObject *args)
{
	return Py_BuildValue("ii", fps_frames, fps_usec);
}

static PyMethodDef MyMethods[] = {
    {"startOutput", startOutput, METH_VARARGS, "Show nifty effects, return ID."},
    {"stopOutput", stopOutput, METH_VARARGS, "Show nifty effects."},
    {"startGrabber", startGrabber, METH_VARARGS, "Start grabbing to show lights."},
    {"stopGrabber", stopGrabber, METH_VARARGS, "Stop grabbing."},
    {"setLight", setLight, METH_VARARGS, "Set color (when not fading)."},
    {"setFadeTime", setFadeTime, METH_VARARGS, "Set fader delay in ms."},
    {"getFadeTime", getFadeTime, METH_VARARGS, "Get fader delay in ms."},
    {"getFpsStats", getFpsStats, METH_VARARGS, "Get FPS stats, returns (frames,usec)."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC initpyambx(void)
{
    (void) Py_InitModule("pyambx", MyMethods);
    if (ambx_init() < 0)
    {
        fprintf(stderr, "ambx_init failed");
    }
}

