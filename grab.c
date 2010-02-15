#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include "grabber.h"
#include "yuvrgb.h"

static int ErrorExit(int code)
{
  fprintf(stderr, "Fatal error code %d\n", code);
  return code;
}

static int saveBmp(const char* filename)
{
	FILE* fd2 = fopen(filename, "w");
	if (fd2 == NULL)
	{
		fprintf(stderr, "Failed to write %s\n", filename);
		return 1;
	}
	unsigned char* video = malloc(luma.width * luma.height * 3); 

	YUVtoRGB(video, &luma, &chroma);

                // write bmp
                unsigned char hdr[14 + 40];
                int i = 0;
#define PUT32(x) hdr[i++] = ((x)&0xFF); hdr[i++] = (((x)>>8)&0xFF); hdr[i++] = (((x)>>16)&0xFF); hdr[i++] = (((x)>>24)&0xFF);
#define PUT16(x) hdr[i++] = ((x)&0xFF); hdr[i++] = (((x)>>8)&0xFF);
#define PUT8(x) hdr[i++] = ((x)&0xFF);
                PUT8('B'); PUT8('M');
                PUT32((((luma.width * luma.height) * 3 + 3) &~ 3) + 14 + 40);
                PUT16(0); PUT16(0); PUT32(14 + 40);
                PUT32(40); PUT32(luma.width); PUT32(luma.height);
                PUT16(1);
                PUT16(3*8); // bits
                PUT32(0); PUT32(0); PUT32(0); PUT32(0); PUT32(0); PUT32(0);
#undef PUT32
#undef PUT16
#undef PUT8
                fwrite(hdr, 1, i, fd2);
                int y;
                for (y = luma.height-1; y != 0; y -= 1) {
                        fwrite(video+(y*luma.width*3), luma.width*3, 1, fd2);
                }

	fclose(fd2);
	free(video);
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

static void showhist(unsigned int* hist)
{
    int base, i;
    for (base = 0; base < 256; base += 16)
    {
	for (i = 0; i < 16; ++i)
	{
		printf("%4d", hist[base + i]);
	}
	printf("\n");
    }
}

void printRGB(int y, int u, int v)
{
   int r, g, b;
   YUV2RGB(y,u,v, &r,&g,&b);
   printf("(y=%d u=%d v=%d) -> (r=%d, g=%d, b=%d)", y, u, v, r, g, b);
}


int main(int argc, char** argv)
{
    int r;

    r = grabber_initialize();
    if (r != 0) return ErrorExit(r);

    r = grabber_begin();
    if (r != 0) return ErrorExit(r);

    int i;
    int x2 = 0;
    unsigned int hist[256];
    for (i = 1; i <= 5; ++i) // 5 regions from left to right
    {
	int x1 = x2;
	x2 = ((i * luma.width) / 5) & 0xFFFFFFFE; // even pixels only (for chroma)
        //printf("Luma size: %dx%d (stride: %d)\n", luma.width, luma.height, luma.stride);
    	int avgY = avg(luma.data, x1, x2, luma.stride, luma.height);

    	memset(hist, 0, sizeof(hist));
    	histogram2(chroma.data, 0, chroma.width, chroma.stride, chroma.height, hist);
    	int mU = median(hist, chroma.width*chroma.height/2);
    	memset(hist, 0, sizeof(hist));
    	histogram2(chroma.data+1, 0, chroma.width, chroma.stride, chroma.height, hist);
    	int mV = median(hist, chroma.width*chroma.height/2);

        printf("Region (%3d..%3d) avgY=%d mU=%d mV=%d ", x1, x2, avgY, mU, mV);
    	printRGB(avgY, mU, mV);
	printf("\n");
    }
/*
    printf("\nLeftTop pixel: ");
    printRGB(luma.data[0], chroma.data[0], chroma.data[1]);
    printf("\npixel at 100,100: ");
    printRGB(luma.data[100*luma.stride+100], chroma.data[50*chroma.stride+100], chroma.data[50*chroma.stride+101]);
*/

    saveBmp("/tmp/tmp.bmp");

    r = grabber_end();
    if (r != 0) return ErrorExit(r);

    r = grabber_destroy();
    return r;
}
