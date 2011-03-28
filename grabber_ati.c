#include <memory.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/types.h>
#include <linux/videodev.h>
#include <linux/fb.h>

#include "grabber.h"

static void createBitmap(Bitmap* bm, int width, int height)
{
    bm->width = width;
    bm->height = height;
    bm->stride = (width+127) & 0xFFFFFF80; // round to multiple of 128 
    //printf("createBitmap(%dx%d) ", width, height);
    height = ((height+63)/64) * 64; // Compensate for extra data.
    //printf("allocate (%dx%d) = %d\n", bm->stride, height, bm->stride*height);
    bm->data = malloc(bm->stride * height);
}

static void destroyBitmap(Bitmap* bm)
{
    if (bm->data)
    {
	free((void*)bm->data);
        bm->data = 0;
        bm->stride = -1;
    }
}

static void destroyBitmaps()
{
	destroyBitmap(&chroma);
	destroyBitmap(&luma);
}

static void createBitmaps(int xres, int yres)
{
    createBitmap(&luma, xres, yres);
    createBitmap(&chroma, xres, (yres+1) >> 1);
}

static int mem_fd;
// Called once when starting up.
int grabber_initialize()
{
    memset(&luma, 0, sizeof(luma));
    memset(&chroma, 0, sizeof(chroma));
    mem_fd = open("/dev/mem", O_RDONLY | O_SYNC);
    if (mem_fd < 0)
    {
        fprintf(stderr, "Can't open /dev/mem \n");
        return 1;
    }
    return 0;
}

// Called at each frame, before using luma and chroma
int grabber_begin()
{
    int yres = hexFromFile("/proc/stb/vmpeg/0/yres");
    if (yres <= 0)
    {
	fprintf(stderr, "Failed to read yres (%d)\n", yres);
	return 1;
    }
    int xres = hexFromFile("/proc/stb/vmpeg/0/xres");
    if (xres <= 0)
    {
	fprintf(stderr, "Failed to read xres (%d)\n", xres);
	return 1;
    }
    if (scale_lines == 0)
    {
        int height = ((yres+63)/64) * 64; // Compensate for extra data.
        if ((height != luma.height) || (xres != luma.width))
        {
	    destroyBitmaps();
	    createBitmaps(xres, height);
        }
    }
    else
    {
        if((scale_lines != luma.height) || (xres != luma.width))
        {
            destroyBitmaps();
            createBitmaps(xres, scale_lines);
        }
    }

    unsigned char* frame = (unsigned char*)mmap(0, 1920*1152*6, PROT_READ, MAP_SHARED, mem_fd, 0x6000000);
    if (!frame)
    {
	fprintf(stderr, "Failed to map video memory\n");
	return 1;
    }

	const unsigned char* frame_chroma = frame + 1920*1152*5;

	const unsigned char* frame_l;
        const int ypart=32;
        const int xpart=128;
        int ysubcount = (yres+31) / 32;
        int ysubchromacount = ysubcount/2;
	int xsubcount = (xres+127) / 128;
	if (scale_lines == 0)
	{
                int xtmp,ytmp,ysub,xsub;

                // "decode" luma/chroma, there are 128x32pixel blocks inside the decoder mem
                for (ysub = 0; ysub != ysubcount; ++ysub)
                {
			frame_l = frame + (ysub * 1920 * 32);
                        for (xsub = 0; xsub != xsubcount; ++xsub)
                        {
                                // Even lines
                                for (ytmp=0; ytmp<ypart; ytmp++)
                                {
                                        int extraoffset = (luma.stride*(ytmp+(ysub*ypart)));
                                        int destx = xsub * xpart;
                                        // We copy a bit too much...
                                        memcpy(luma.data + destx + extraoffset, frame_l, xpart);
                                        frame_l += xpart;
                                }
                        }
			// move pointer to next "line"
                        ++ysub; // dirty, assume even line count
			frame_l = frame + (ysub * 1920 * 32);
                        for (xsub=0; xsub<xsubcount; xsub++) // 1920/128=15
                        {
                                // Odd lines (reverts 64 byte block?)
                                // Only luminance
                                for (ytmp=0; ytmp<ypart; ytmp++)
                                {
                                        int extraoffset = (luma.stride*(ytmp+(ysub*ypart)));
                                        int destx = xsub*xpart;
                                        // We copy a bit too much...
                                        memcpy(luma.data + destx + extraoffset + 64, frame_l, 64);
                                        memcpy(luma.data + destx + extraoffset, frame_l + 64, 64);
                                        frame_l += xpart;
                                }
                        }
                }

                // Chrominance (half resolution)
                for (ysub=0; ysub < ysubchromacount; ysub++)
                {
			const unsigned char* frame_c = frame_chroma + (ysub * 1920 * 32);
                        for (xsub=0; xsub < xsubcount; xsub++)
                        {
                                // Even lines
                                for (ytmp=0; ytmp<ypart; ytmp++)
                                {
                                        int extraoffset = (chroma.stride*(ytmp+(ysub*ypart)));
                                        int destx = xsub*xpart;
                                        memcpy(chroma.data + destx + extraoffset, frame_c, xpart);
                                        frame_c += xpart;
                                }
                        }
                        ++ysub; // dirty...
			frame_c = frame_chroma + (ysub * 1920 * 32);
                        for (xsub=0; xsub<xsubcount; xsub++) // 1920/128=15
                        {
                                // Odd lines (reverts 64 byte block?)
                                // Only luminance
                                for (ytmp=0; ytmp<ypart; ytmp++)
                                {
                                        int extraoffset = (chroma.stride*(ytmp+(ysub*ypart)));
                                        int destx = xsub*xpart;
                                        // We copy a bit too much...
                                        memcpy(chroma.data + destx + extraoffset + 64, frame_c, 64);
                                        memcpy(chroma.data + destx + extraoffset, frame_c + 64, 64);
                                        frame_c += xpart;
                                }
                        }

		}
	}
	else
	{
		int y;
		const unsigned char* dest_line = luma.data;
		for (y = 0; y != scale_lines; ++y)
		{
			// which is the scanline we need
			int line = (y * yres) / scale_lines;
			int block = line >> 5;
			int innerline = line - ( block << 5);
			const unsigned char* first_pixel = frame + (block * (32*1920)) + innerline * 128;
			int xsub;
			int offset = 0;
			if (block & 1)
			{
				// swap 64-byte stuff
				for (xsub = 0; xsub != xsubcount; ++xsub)
				{
					memcpy(dest_line, first_pixel+64, 64); 
					memcpy(dest_line+64, first_pixel, 64); 
					dest_line += 128;
					first_pixel += (128*32);
				}
			}
			else
			{
				for (xsub = 0; xsub != xsubcount; ++xsub)
				{
					memcpy(dest_line, first_pixel, 128); 
					dest_line += 128;
					first_pixel += (128*32);
				}
			}
		}
		int chroma_lines = scale_lines >> 1;
		int chroma_yres = yres >> 1;
		dest_line = chroma.data;
		for (y = 0; y != chroma_lines; ++y)
		{
			int line = (y * chroma_yres) / chroma_lines;
			int block = line >> 5;
			int innerline = line - ( block << 5);
			const unsigned char* first_pixel = frame_chroma + (block * (32*1920)) + innerline * 128;
			int xsub;
			int offset = 0;
			if (block & 1)
			{
				// swap 64-byte stuff
				for (xsub = 0; xsub != xsubcount; ++xsub)
				{
					memcpy(dest_line, first_pixel+64, 64); 
					memcpy(dest_line+64, first_pixel, 64); 
					dest_line += 128;
					first_pixel += (128*32);
				}
			}
			else
			{
				for (xsub = 0; xsub != xsubcount; ++xsub)
				{
					memcpy(dest_line, first_pixel, 128); 
					dest_line += 128;
					first_pixel += (128*32);
				}
			}
		}
	}
    munmap(frame, 1920*1152*6);
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
    destroyBitmaps();
    if (mem_fd != -1)
    {
        close(mem_fd);
        mem_fd = -1;
    }
    return 0;
}

