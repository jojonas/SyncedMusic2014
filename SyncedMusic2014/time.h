#ifndef TIME_H
#define TIME_H

#define TIMER_POINT_COUNT 50

typedef float timer_t;

typedef struct {
	timer_t lowPrecisionTimePoints[TIMER_POINT_COUNT];
	timer_t highPrecisionTimePoints[TIMER_POINT_COUNT];
	int nextPoint;
	int dirty; // needs new linear regression if != 0
	int full; // has been written once?

	float slope; // from linear regression: y=slope*x + offset
	float offset;
} TimerState;

TimerState* createTimer();
timer_t getTime(TimerState* const timerState);
void updateTimer(TimerState* const timerState, const timer_t lowPrecisionTime);

#endif // TIME_H
