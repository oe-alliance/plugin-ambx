#include <stdlib.h>
#include <memory.h>
#include "Fader.h"

void fader_init(Fader* self, unsigned int size)
{
	self->tsBegin = 0;
	self->tsEnd = 0;
	self->size = size;
	self->current = malloc(sizeof(byte) * size * 3);
	self->target = self->current + size;
	self->start = self->target + size;
}

void fader_free(Fader* self)
{
	free(self->current);
}

void fader_update(Fader* self, unsigned int tsNow)
{
	if (tsNow >= self->tsEnd)
	{
		// Target reached.
		memcpy(self->current, self->target, self->size * sizeof(byte));
		return;
	}
	unsigned int tsDelta = tsNow - self->tsBegin;
	unsigned int tsDenom = self->tsEnd - self->tsBegin;

	unsigned int i;
	for (i = 0; i < self->size; ++i)
	{
		self->current[i] = (byte)(
                    (((int)(self->start[i]) * (tsDenom-tsDelta)) + ((int)(self->target[i]) * (tsDelta))) / tsDenom
                );
	}
}

void fader_commit(Fader* self, unsigned int tsNow, unsigned int tsEnd)
{
	self->tsBegin = tsNow;
	self->tsEnd = tsEnd;
	memcpy(self->start, self->current, self->size * sizeof(byte));
}
