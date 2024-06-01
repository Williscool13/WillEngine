#include <time_util.h>


TimeUtil TimeUtil::instance;

TimeUtil& TimeUtil::Get()
{
	return instance;
}

void TimeUtil::init()
{
	lastTime = SDL_GetTicks64();
}

void TimeUtil::update()
{
	uint64_t last = SDL_GetTicks64();
	deltaTime = last - lastTime;
	lastTime = last;
}

float TimeUtil::getDeltaTime()
{
	return deltaTime / 1000.0f;
}