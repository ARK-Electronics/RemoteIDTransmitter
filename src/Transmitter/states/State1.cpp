#include <states/base/State.hpp>

#include <chrono>
#include <thread>

namespace states
{

bool State1::can_transition_to(AppState state)
{
	switch (state) {
	case AppState::STATE_1:
	case AppState::STATE_2:
	case AppState::STATE_3:
		return true;

	default:
		return false;
	}
}

void State1::on_enter() {}

void State1::on_exit() {}

void State1::run()
{
	LOG("State1: run()");
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

} // end namespace states
