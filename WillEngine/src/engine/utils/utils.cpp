#include "utils.h"

RollingAverage::RollingAverage(size_t size) : buffer(size), maxSize(size), sum(0.0), index(0), count(0) {}

void RollingAverage::addValue(float value)
{
    if (count < maxSize) {
        buffer[index] = value;
        sum += value;
        count++;
    }
    else {
        sum -= buffer[index];
        buffer[index] = value;
        sum += value;
    }
    index = (index + 1) % maxSize;
}


float RollingAverage::getAverage() const {
    return (count > 0) ? sum / count : 0.0f;
}
