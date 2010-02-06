#include "colorproc.h"

unsigned char avg(const unsigned char* data, int x1, int x2, int stride, int yres)
{
	unsigned int v = 0;
        int x;
        int y;
        const unsigned char* line;
        for (y = 0; y < yres; ++y)
        {
                line = data + x1 + (y * stride);
                for (x = x1; x < x2; ++x)
                {
                        v += *line++;
                }
        }
        v /= (x2-x1) * yres;
        return v;
}

int avgcolor(const unsigned char* data, int x1, int x2, int stride, int yres)
{
        unsigned int r = 0;
        unsigned int g = 0;
        unsigned int b = 0;
        unsigned int count = 0;
        int x;
        int y;
        const unsigned char* line;
        for (y = 0; y<yres; ++y)
        {
                line = data + (x1*3) + (y*stride);
                for (x = x1; x < x2; ++x)
                {
                        r += *line++;
                        g += *line++;
                        b += *line++;
                }
        }
        count = (x2-x1) * yres;
        r /= count;
        g /= count;
        b /= count;
        return (r << 16) | (g << 8) | b;
}

void getcolors(int* output, const unsigned char* data, int xres, int yres)
{
        int x;
        for (x = 0; x < 5; ++x)
        {
                output[x] = avgcolor(data, (x * xres) / 5, ((x+1) * xres) / 5, xres*3, yres);
        }
}

