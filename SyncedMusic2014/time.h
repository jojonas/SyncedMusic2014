#ifndef TIME_H
#define TIME_H

#include <Windows.h>

#define TIMER_POINT_COUNT 50

typedef double timer_t;

typedef struct {
	timer_t lowPrecisionTimePoints[TIMER_POINT_COUNT];
	timer_t highPrecisionTimePoints[TIMER_POINT_COUNT];
	int nextPoint;
	int dirty; // needs new linear regression if != 0
	int full; // has been written once?

	timer_t slope; // from linear regression: y=slope*x + offset
	timer_t offset;

	HANDLE mutex;
} TimerState;

TimerState* createTimer();
timer_t getTime(TimerState* const timerState);
void updateTimer(TimerState* const timerState, const timer_t lowPrecisionTime);
timer_t getHighPrecisionTime();
#endif // TIME_H
