#pragma once
#include <QObject>
#include <memory>

class BaseInstance;

class InstanceHandle : public QObject
{
Q_OBJECT
public: /* types */
	using loader_type = std::function<BaseInstance *(const QString & id, BaseInstance * original)>;
	enum State
	{
		NotLoaded, ///< instance has not been loaded yet
		Loaded, ///< instance has been successfully loaded
		Unloaded, ///< instance has been unloaded
		Destroyed ///< instance has been removed entirely from persistent storage
	};

public: /* methods */
	InstanceHandle(const QString &id, loader_type loader);
	virtual ~InstanceHandle();

	BaseInstance * operator*()
	{
		return m_instance.get();
	}
	void reload();
	void unload();
	void destroy();

	Q_SIGNAL void stateChanged(State oldState, State newState);

private: /* methods */
	void changeState(State newState);

private: /* data */
	QString m_id;
	State m_state = NotLoaded;
	std::unique_ptr<BaseInstance> m_instance;
	loader_type m_loader;
};
