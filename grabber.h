typedef struct _Bitmap
{
	unsigned char* data;
	int stride;
	int width;
	int height;
} Bitmap;
	
extern Bitmap luma;
extern Bitmap chroma;

// Called once when starting up.
int grabber_initialize();
// Called at each frame, before using luma and chroma
int grabber_begin();
// Called when done processing luma and chroma
int grabber_end();
// Called on program shutdown.
int grabber_destroy();

