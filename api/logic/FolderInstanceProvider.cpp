#include "FolderInstanceProvider.h"
#include "settings/INISettingsObject.h"
#include "FileSystem.h"
#include "minecraft/onesix/OneSixInstance.h"
#include "minecraft/legacy/LegacyInstance.h"
#include "NullInstance.h"

#include <QDir>
#include <QDirIterator>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

const static int GROUP_FILE_FORMAT_VERSION = 1;

struct WatchLock
{
	WatchLock(QFileSystemWatcher * watcher, const QString& instDir)
		: m_watcher(watcher), m_instDir(instDir)
	{
		m_watcher->removePath(m_instDir);
	}
	~WatchLock()
	{
		m_watcher->addPath(m_instDir);
	}
	QFileSystemWatcher * m_watcher;
	QString m_instDir;
};

FolderInstanceProvider::FolderInstanceProvider(SettingsObjectPtr settings, const QString& instDir)
	: BaseInstanceProvider(settings)
{
	m_instDir = instDir;
	if (!QDir::current().exists(m_instDir))
	{
		QDir::current().mkpath(m_instDir);
	}
	m_watcher = new QFileSystemWatcher(this);
	connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &FolderInstanceProvider::instanceDirContentsChanged);
	m_watcher->addPath(m_instDir);
}

QList< InstanceId > FolderInstanceProvider::discoverInstances()
{
	QList<InstanceId> out;
	QDirIterator iter(m_instDir, QDir::Dirs | QDir::NoDot | QDir::NoDotDot | QDir::Readable, QDirIterator::FollowSymlinks);
	while (iter.hasNext())
	{
		QString subDir = iter.next();
		QFileInfo dirInfo(subDir);
		if (!QFileInfo(FS::PathCombine(subDir, "instance.cfg")).exists())
			continue;
		// if it is a symlink, ignore it if it goes to the instance folder
		if(dirInfo.isSymLink())
		{
			QFileInfo targetInfo(dirInfo.symLinkTarget());
			QFileInfo instDirInfo(m_instDir);
			if(targetInfo.canonicalPath() == instDirInfo.canonicalFilePath())
			{
				qDebug() << "Ignoring symlink" << subDir << "that leads into the instances folder";
				continue;
			}
		}
		auto id = dirInfo.fileName();
		out.append(id);
		qDebug() << "Found instance ID" << id;
	}
	return out;
}

InstancePtr FolderInstanceProvider::loadInstance(const InstanceId& id)
{
	if(!m_groupsLoaded)
	{
		loadGroupList();
	}
	auto instanceRoot = FS::PathCombine(m_instDir, id);
	auto instanceSettings = std::make_shared<INISettingsObject>(FS::PathCombine(instanceRoot, "instance.cfg"));
	InstancePtr inst;

	instanceSettings->registerSetting("InstanceType", "Legacy");

	QString inst_type = instanceSettings->get("InstanceType").toString();

	if (inst_type == "OneSix" || inst_type == "Nostalgia")
	{
		inst.reset(new OneSixInstance(m_globalSettings, instanceSettings, instanceRoot));
	}
	else if (inst_type == "Legacy")
	{
		inst.reset(new LegacyInstance(m_globalSettings, instanceSettings, instanceRoot));
	}
	else
	{
		inst.reset(new NullInstance(m_globalSettings, instanceSettings, instanceRoot));
	}
	inst->init();
	inst->setProvider(this);
	auto iter = groupMap.find(id);
	if (iter != groupMap.end())
	{
		inst->setGroupInitial((*iter));
	}
	connect(inst.get(), &BaseInstance::groupChanged, this, &FolderInstanceProvider::groupChanged);
	qDebug() << "Loaded instance " << inst->name() << " from " << inst->instanceRoot();
	return inst;
}

void FolderInstanceProvider::saveGroupList()
{
	WatchLock foo(m_watcher, m_instDir);
	QString groupFileName = m_instDir + "/instgroups.json";
	QMap<QString, QSet<QString>> reverseGroupMap;
	for (auto iter = groupMap.begin(); iter != groupMap.end(); iter++)
	{
		QString id = iter.key();
		QString group = iter.value();
		if (group.isEmpty())
			continue;

		if (!reverseGroupMap.count(group))
		{
			QSet<QString> set;
			set.insert(id);
			reverseGroupMap[group] = set;
		}
		else
		{
			QSet<QString> &set = reverseGroupMap[group];
			set.insert(id);
		}
	}
	QJsonObject toplevel;
	toplevel.insert("formatVersion", QJsonValue(QString("1")));
	QJsonObject groupsArr;
	for (auto iter = reverseGroupMap.begin(); iter != reverseGroupMap.end(); iter++)
	{
		auto list = iter.value();
		auto name = iter.key();
		QJsonObject groupObj;
		QJsonArray instanceArr;
		groupObj.insert("hidden", QJsonValue(QString("false")));
		for (auto item : list)
		{
			instanceArr.append(QJsonValue(item));
		}
		groupObj.insert("instances", instanceArr);
		groupsArr.insert(name, groupObj);
	}
	toplevel.insert("groups", groupsArr);
	QJsonDocument doc(toplevel);
	try
	{
		FS::write(groupFileName, doc.toJson());
	}
	catch(FS::FileSystemException & e)
	{
		qCritical() << "Failed to write instance group file :" << e.cause();
	}
}

void FolderInstanceProvider::loadGroupList()
{
	QSet<QString> groupSet;
	QString groupFileName = m_instDir + "/instgroups.json";

	// if there's no group file, fail
	if (!QFileInfo(groupFileName).exists())
		return;

	QByteArray jsonData;
	try
	{
		jsonData = FS::read(groupFileName);
	}
	catch (FS::FileSystemException & e)
	{
		qCritical() << "Failed to read instance group file :" << e.cause();
		return;
	}

	QJsonParseError error;
	QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &error);

	// if the json was bad, fail
	if (error.error != QJsonParseError::NoError)
	{
		qCritical() << QString("Failed to parse instance group file: %1 at offset %2")
							.arg(error.errorString(), QString::number(error.offset))
							.toUtf8();
		return;
	}

	// if the root of the json wasn't an object, fail
	if (!jsonDoc.isObject())
	{
		qWarning() << "Invalid group file. Root entry should be an object.";
		return;
	}

	QJsonObject rootObj = jsonDoc.object();

	// Make sure the format version matches, otherwise fail.
	if (rootObj.value("formatVersion").toVariant().toInt() != GROUP_FILE_FORMAT_VERSION)
		return;

	// Get the groups. if it's not an object, fail
	if (!rootObj.value("groups").isObject())
	{
		qWarning() << "Invalid group list JSON: 'groups' should be an object.";
		return;
	}

	// Iterate through all the groups.
	QJsonObject groupMapping = rootObj.value("groups").toObject();
	for (QJsonObject::iterator iter = groupMapping.begin(); iter != groupMapping.end(); iter++)
	{
		QString groupName = iter.key();

		// If not an object, complain and skip to the next one.
		if (!iter.value().isObject())
		{
			qWarning() << QString("Group '%1' in the group list should "
								   "be an object.")
							   .arg(groupName)
							   .toUtf8();
			continue;
		}

		QJsonObject groupObj = iter.value().toObject();
		if (!groupObj.value("instances").isArray())
		{
			qWarning() << QString("Group '%1' in the group list is invalid. "
								   "It should contain an array "
								   "called 'instances'.")
							   .arg(groupName)
							   .toUtf8();
			continue;
		}

		// keep a list/set of groups for choosing
		groupSet.insert(groupName);

		// Iterate through the list of instances in the group.
		QJsonArray instancesArray = groupObj.value("instances").toArray();

		for (QJsonArray::iterator iter2 = instancesArray.begin(); iter2 != instancesArray.end();
			 iter2++)
		{
			groupMap[(*iter2).toString()] = groupName;
		}
	}
	emit groupsChanged(groupSet);
}

void FolderInstanceProvider::groupChanged()
{
	// save the groups. save all of them.
	auto instance = (BaseInstance *) QObject::sender();
	auto id = instance->id();
	groupMap[id] = instance->group();
	emit groupsChanged({instance->group()});
	saveGroupList();
}


void FolderInstanceProvider::instanceDirContentsChanged(const QString& path)
{
	Q_UNUSED(path);
	emit instancesChanged();
}

void FolderInstanceProvider::on_InstFolderChanged(const Setting &setting, QVariant value)
{
	QString newInstDir = value.toString();
	if(newInstDir != m_instDir)
	{
		m_instDir = newInstDir;
		emit instancesChanged();
	}
}
