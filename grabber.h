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
extern int grabber_initialize(void);
// Called at each frame, before using luma and chroma
extern int grabber_begin(void);
// Called when done processing luma and chroma
extern int grabber_end(void);
// Called on program shutdown.
extern int grabber_destroy(void);

