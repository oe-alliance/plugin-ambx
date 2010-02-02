#include "grabber.h"
#include "xilleon.h"

int main(int argc, char** argv)
{
    Grabber* grabber = new Xilleon();

    grabber->grab();
    // do something useful
    grabber->done();

    delete grabber;
    return 0;
}
