#include "InstanceHandle.h"
#include "BaseInstance.h"

InstanceHandle::InstanceHandle(const QString& id, loader_type loader)
	: m_id(id), m_loader(loader)
{
}

InstanceHandle::~InstanceHandle()
{
}

void InstanceHandle::changeState(InstanceHandle::State newState)
{
	auto currentState = m_state;
	m_state = newState;
	emit stateChanged(currentState, newState);
}

void InstanceHandle::reload()
{
	auto result = m_loader(m_id, m_instance.get());
	if(result != m_instance.get())
	{
		m_instance.reset(result);
		changeState(m_instance ? State::Loaded : State::Destroyed);
	}
}

void InstanceHandle::unload()
{
	if(m_state == State::Loaded)
	{
		m_instance.reset();
		changeState(State::Unloaded);
	}
}

void InstanceHandle::destroy()
{
	if(m_state == State::Loaded)
	{
		m_instance->nuke();
		changeState(State::Destroyed);
	}
}
