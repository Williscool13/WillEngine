#pragma once
#include <vector>

class RollingAverage {
public:
    RollingAverage(size_t size);
    void addValue(float value);
    float getAverage() const;
private:
    std::vector<float> buffer;
    size_t maxSize;
    float sum;
    size_t index;
    size_t count;
};