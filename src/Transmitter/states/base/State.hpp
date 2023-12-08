#pragma once

#include <memory>
#include <mutex>
#include <functional>

#include <helpers.hpp>
#include <CircularBuffer.hpp>

namespace txr { class Transmitter; }

namespace states
{

enum AppState : size_t {
	STATE_1,
	STATE_2,
	STATE_3,
};

template<class T>
class State
{
public:
	virtual void run() = 0;
	virtual void on_enter() = 0;
	virtual void on_exit() = 0;
	virtual bool can_transition_to(AppState state) = 0;
private:
	T* _parent {};
};

class State1 : public State<txr::Transmitter>
{
public:
	virtual void run() override;
	virtual void on_enter() override;
	virtual void on_exit() override;
	virtual bool can_transition_to(AppState state) override;

private:
};

class State2 : public State<txr::Transmitter>
{
public:
	virtual void run() override;
	virtual void on_enter() override;
	virtual void on_exit() override;
	virtual bool can_transition_to(AppState state) override;

private:
	enum class SubAppState {
		SUB_STATE_1,
		SUB_STATE_2,
		SUB_STATE_3,
	};
	SubAppState _sub_state {};
};


class State3 : public State<txr::Transmitter>
{
public:
	virtual void run() override;
	virtual void on_enter() override;
	virtual void on_exit() override;
	virtual bool can_transition_to(AppState state) override;

private:
	enum class SubAppState {
		SUB_STATE_1,
		SUB_STATE_2,
		SUB_STATE_3,
	};
	SubAppState _sub_state {};
};

} // end namespace states
