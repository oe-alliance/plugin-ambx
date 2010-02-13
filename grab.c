#include <stdlib.h>
#include <stdio.h>
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

/*
                int x, y;
		unsigned char* row = (unsigned char*)malloc(bmp->width * 3);
                for (y=bmp->height-1; y>=0 ; --y) {
			unsigned char* d = row;
			for (x=0; x<bmp->width; ++x)
                        {
				unsigned char pix = bmp->data[bmp->stride*y + x];
				*d++ = pix;
				*d++ = pix;
				*d++ = pix;
			}
			fwrite(row, bmp->width * 3, 1, fd2);
                }
		free(row);
*/
	fclose(fd2);
	free(video);
}

int main(int argc, char** argv)
{
    int r;

    r = grabber_initialize();
    if (r != 0) return ErrorExit(r);

    r = grabber_begin();
    if (r != 0) return ErrorExit(r);

    printf("Luma size: %dx%d (stride: %d)\n", luma.width, luma.height, luma.stride);
    printf("Avg luma: %d\n", avg(luma.data, 0, luma.width, luma.stride, luma.height));
    printf("Chroma size: %dx%d (stride: %d)\n", chroma.width, chroma.height, chroma.stride);
    printf("Avg ch-U: %d\n", avg2(chroma.data, 0, chroma.width, chroma.stride, chroma.height));
    printf("Avg ch-V: %d\n", avg2(chroma.data + 1, 0, chroma.width, chroma.stride, chroma.height));

    saveBmp("/tmp/tmp.bmp");

    r = grabber_end();
    if (r != 0) return ErrorExit(r);

    r = grabber_destroy();
    return r;
}
