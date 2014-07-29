#include "time.h"

#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>

// === PRIVATE ===

void updateLinearRegression(TimerState* const timerState) {
	const int n = timerState->full ? TIMER_POINT_COUNT : timerState->nextPoint;
	const timer_t* const x = timerState->highPrecisionTimePoints;
	const timer_t* const y = timerState->lowPrecisionTimePoints;
	
	timer_t xy_sum = 0, x_sum = 0, y_sum = 0, x_sq_sum = 0;
	for (int i = 0; i < n; i++) {
		xy_sum += x[i] * y[i];
		x_sum += x[i];
		y_sum += y[i];
		x_sq_sum += x[i] * x[i];
	}

	const timer_t denominator = (n*x_sq_sum - x_sum*x_sum);

	timerState->slope = (n*xy_sum - x_sum*y_sum) / denominator;
	timerState->offset = (x_sq_sum*y_sum - x_sum*xy_sum) / denominator;
}

// === PUBLIC ===

timer_t getHighPrecisionTime() {
	LARGE_INTEGER performanceCounter;
	LARGE_INTEGER frequency;
	if (QueryPerformanceCounter(&performanceCounter) 
		&& QueryPerformanceFrequency(&frequency)) {
		LONGLONG integer_part = performanceCounter.QuadPart / frequency.QuadPart;
		LONGLONG real_part = performanceCounter.QuadPart % frequency.QuadPart;
		return integer_part + (timer_t)real_part / frequency.QuadPart;
	}
	else {
		return -1;
	}
}


TimerState* createTimer()
{
	TimerState* const timerState = malloc(sizeof(TimerState));
	if (!timerState) {
		puts("could not allocate memory for timer state");
		return NULL;
	}
	timerState->nextPoint = 0;
	timerState->dirty = 1;
	timerState->full = 0;
	return timerState;
}

timer_t getTime(TimerState* const timerState)
{
	if (timerState->dirty) {
		updateLinearRegression(timerState);
		timerState->dirty = 0;
	}

	const timer_t highPrecisionTime = getHighPrecisionTime();
	return timerState->slope * highPrecisionTime + timerState->offset;
}

void updateTimer(TimerState* const timerState, const timer_t lowPrecisionTime)
{
	const timer_t highPrecisionTime = getHighPrecisionTime();
	timerState->lowPrecisionTimePoints[timerState->nextPoint] = lowPrecisionTime;
	timerState->highPrecisionTimePoints[timerState->nextPoint] = highPrecisionTime;
	timerState->nextPoint = (timerState->nextPoint + 1) % TIMER_POINT_COUNT;
	if (timerState->nextPoint == 0) 
		timerState->full = 1;
	
	timerState->dirty = 1;
}


