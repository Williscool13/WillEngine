#pragma once

#include <SDL2/SDL.h>


class TimeUtil
{
public:
	static TimeUtil& Get();
	void init();
	void update();
	float getDeltaTime();
private:
	uint64_t deltaTime = 0;
	uint64_t lastTime = 0;

	static TimeUtil instance;
};