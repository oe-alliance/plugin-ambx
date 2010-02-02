#ifndef GRABBER_H
#define GRABBER_H

struct Bitmap
{
	const unsigned char* data;
	int stride;
	int width;
	int height;
};

class Grabber
{
public:
	virtual void grab() = 0;
	virtual void done() = 0;
	
	Bitmap luma;
	Bitmap chroma;
};

class XilleonGrabber: public Grabber
{
	virtual void grab();
	virtual void done();
};

#endif
