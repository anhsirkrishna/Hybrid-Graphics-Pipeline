#include "TimerWrap.h"

using namespace std::chrono;

TimerWrap::TimerWrap() noexcept {
	last = steady_clock::now();
	start = steady_clock::now();
}

void TimerWrap::Reset() noexcept {
	last = steady_clock::now();
	start = steady_clock::now();
}

float TimerWrap::Mark() noexcept {
	const auto old = last;
	last = steady_clock::now();
	const duration<float> frameTime = last - old;

	return frameTime.count();
}

float TimerWrap::Peek() const noexcept {
	return duration<float>(steady_clock::now() - last).count();
}
