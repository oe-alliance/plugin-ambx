unsigned char avg(const unsigned char* data, int x1, int x2, int stride, int yres);
unsigned char avg2(const unsigned char* data, int x1, int x2, int stride, int yres);
void histogram2(const unsigned char* data, int x1, int x2, int stride, int yres, unsigned int* result);
int avgcolor(const unsigned char* data, int x1, int x2, int stride, int yres);
void getcolors(int* output, const unsigned char* data, int xres, int yres);

