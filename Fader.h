#pragma once

typedef unsigned char byte;

typedef struct _Fader
{
	// Timestamp units are undefined, but must be incremental
	unsigned int tsBegin;
	unsigned int tsEnd;
	unsigned int size;
	byte* current;
	byte* target;
	byte* start;
} Fader;

void fader_init(Fader* self, unsigned int size);
void fader_free(Fader* self);
void fader_update(Fader* self, unsigned int tsNow); // Calculates current based on timestamp
void fader_commit(Fader* self, unsigned int tsNow, unsigned int tsEnd); // Sets new targets based on timestamp

