#include <stddef.h>

typedef struct CReading { int temperature; int load; } CReading;

size_t status_reading_size(void) { return sizeof(CReading); }

int status_checksum(int temperature, int load)
{
    return temperature * 31 + load;
}
