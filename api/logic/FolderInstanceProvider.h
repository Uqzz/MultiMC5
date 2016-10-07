#pragma once

#include "BaseInstanceProvider.h"
#include <QMap>

class QFileSystemWatcher;

class FolderInstanceProvider : public BaseInstanceProvider
{
	Q_OBJECT
public:
	FolderInstanceProvider(SettingsObjectPtr settings, const QString & instDir);

public:
	QList<InstanceId> discoverInstances() override;
	InstancePtr loadInstance(const InstanceId& id) override;
	void loadGroupList() override;
	void saveGroupList() override;

private slots:
	void instanceDirContentsChanged(const QString &path);
	void on_InstFolderChanged(const Setting &setting, QVariant value);
	void groupChanged();

private:
	QString m_instDir;
	QFileSystemWatcher * m_watcher;
	QSet<QString> m_knownIds;
	QMap<QString, QString> groupMap;
	bool m_groupsLoaded = false;
};


