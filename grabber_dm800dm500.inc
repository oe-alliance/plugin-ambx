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

Bitmap luma;
Bitmap chroma;


static void createBitmap(Bitmap* bm, int width, int height)
{
    bm->width = width;
    bm->height = height;
    bm->stride = width; 
    //printf("createBitmap(%dx%d) ", width, height);
    height = height;
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

// Called once when starting up.
int grabber_initialize()
{
    memset(&luma, 0, sizeof(luma));
    memset(&chroma, 0, sizeof(chroma));
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
    if ((yres != luma.height) || (xres != luma.width))
    {
	destroyBitmaps();
	createBitmaps(xres,yres);
    }

    int mem_fd = open("/dev/mem", O_RDWR|O_SYNC);
    if (mem_fd < 0)
    {
        fprintf(stderr, "Can't open /dev/mem \n");
        return 1;
    }
                // grab brcm7401 pic from decoder memory
                const unsigned char* data = (unsigned char*)mmap(0, 100, PROT_READ, MAP_SHARED, mem_fd, 0x10100000);
                if(!data)
                {
                        fprintf(stderr, "Mainmemory: <Memmapping failed>\n");
                        return 1;
                }

                //vert_start=data[0x1B]<<8|data[0x1A];
                //vert_end=data[0x19]<<8|data[0x18];
                int stride=data[0x15]<<8|data[0x14];
                int ofs=(data[0x28]<<8|data[0x27])>>4; // luma lines
                int ofs2=(data[0x2c]<<8|data[0x2b])>>4;// chroma lines
                int adr=(data[0x1f]<<24|data[0x1e]<<16|data[0x1d]<<8|data[0x1c])&0xFFFFFF00; // start of  videomem
                int adr2=(data[0x23]<<24|data[0x22]<<16|data[0x21]<<8|data[0x20])&0xFFFFFF00;
                int offset=adr2-adr;

                munmap((void*)data, 100);

		// Check that obtained values are sane and prevent segfaults.
		if ((adr == 0) || (adr2 == 0) || (adr2 <= adr))
		{
			fprintf(stderr, "Got or invalid 'adr' offsets, aborting (%x,%x)\n", adr, adr2);
			close(mem_fd);
			return 1;
		}
		if (stride != xres)
		{
			fprintf(stderr, "stride != xres (%d != %d), aborting\n", stride, xres);
			close(mem_fd);
			return 1;
		}
		if (ofs < yres)
		{
			fprintf(stderr, "luma lines < yres (%d < %d), aborting\n", ofs, yres);
			close(mem_fd);
			return 1;
		}

		unsigned char* memory_tmp;	
                if(!(memory_tmp = (unsigned char*)mmap(0, offset + stride*(ofs2+64), PROT_READ, MAP_SHARED, mem_fd, adr)))
                {
                                fprintf(stderr, "Mainmemory: <Memmapping failed> video buffer\n");
                                return 1;
                }
                int xsub=chr_luma_stride;
                // decode luma & chroma plane or lets say sort it
		int xtmp;
		int ytmp;
		int t = 0;
		int t2 = 0;
                for (xtmp=0; xtmp < stride; xtmp += chr_luma_stride)
                {
                        if ((stride-xtmp) <= chr_luma_stride)
                                xsub=stride-xtmp;

                        int dat1=xtmp;
                        for (ytmp = 0; ytmp < yres; ytmp++)
                        {
                                memcpy(luma.data+dat1,memory_tmp+t,xsub); // luma
                                t+=chr_luma_stride;
                                dat1+=stride;
                        }
			// Skip the invisible lines
			t += (ofs - yres) * chr_luma_stride;
                }
		xsub=chr_luma_stride;
		int yres2 = yres >> 1; // ofs is larger than yres
                for (xtmp=0; xtmp < stride; xtmp += chr_luma_stride)
                {
                        if ((stride-xtmp) <= chr_luma_stride)
                                xsub=stride-xtmp;

                        int dat1=xtmp;
                        for (ytmp = 0; ytmp < yres2; ytmp++) 
                        {
                                memcpy(chroma.data+dat1,memory_tmp+offset+t2,xsub); // chroma
                                t2+=chr_luma_stride;
                                dat1+=stride;
                        }
                        // Skip the invisible lines
                        t += (ofs2 - yres2) * chr_luma_stride;
                }
		munmap(memory_tmp, offset + stride*(ofs2+64));
                int count = (stride*yres) >> 2;
                unsigned char* p = luma.data;
                for (t=count; t != 0; --t)
                {
			unsigned char q;
			q = p[0]; p[0] = p[3]; p[3] = q;
			q = p[1]; p[1] = p[2]; p[2] = q;
                        p += 4;
                }
                count = (stride*yres2) >> 2;
                p = chroma.data;
                for (t=count; t != 0; --t)
                {
			unsigned char q;
			q = p[0]; p[0] = p[3]; p[3] = q;
			q = p[1]; p[1] = p[2]; p[2] = q;
                        p += 4;
                }

    close(mem_fd);
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
    return 0;
}

