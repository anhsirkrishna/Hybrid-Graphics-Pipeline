#pragma once
#include <chrono>

class TimerWrap
{
public:
	TimerWrap() noexcept;
	void Reset() noexcept;
	float Mark() noexcept;
	float Peek() const noexcept;
private:
	std::chrono::steady_clock::time_point last;
	std::chrono::steady_clock::time_point start;
};