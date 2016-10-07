#pragma once

#include <QObject>
#include <QString>
#include "BaseInstance.h"
#include "settings/SettingsObject.h"

using InstanceId = QString;
using InstanceLocator = std::pair<InstancePtr, int>;

class BaseInstanceProvider : public QObject
{
	Q_OBJECT
public:
	BaseInstanceProvider(SettingsObjectPtr settings) : m_globalSettings(settings)
	{
		// nil
	}
public:
	virtual QList<InstanceId> discoverInstances() = 0;
	virtual InstancePtr loadInstance(const InstanceId &id) = 0;
	virtual void loadGroupList() = 0;
	virtual void saveGroupList() = 0;
signals:
	void instancesChanged();
	void groupsChanged(QSet<QString> groups);

protected:
	SettingsObjectPtr m_globalSettings;
};

