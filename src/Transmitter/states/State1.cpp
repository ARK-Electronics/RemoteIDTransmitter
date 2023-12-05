#include <states/base/State.hpp>

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
	switch (_sub_state) {
	case SubAppState::SUB_STATE_1:
	case SubAppState::SUB_STATE_2:
	case SubAppState::SUB_STATE_3:
		break;
	}
}

} // end namespace states
