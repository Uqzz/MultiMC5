/* Copyright 2013-2015 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "BaseInstance.h"

#include <QFileInfo>
#include <QDir>
#include <QDebug>

#include "settings/INISettingsObject.h"
#include "settings/Setting.h"
#include "settings/OverrideSetting.h"

#include "minecraft/MinecraftVersionList.h"
#include "FileSystem.h"
#include "Commandline.h"

BaseInstance::BaseInstance(SettingsObjectPtr globalSettings, SettingsObjectPtr settings, const QString &rootDir)
	: QObject()
{
	m_settings = settings;
	m_rootDir = rootDir;

	m_settings->registerSetting("name", "Unnamed Instance");
	m_settings->registerSetting("iconKey", "default");
	m_settings->registerSetting("notes", "");
	m_settings->registerSetting("lastLaunchTime", 0);
	m_settings->registerSetting("totalTimePlayed", 0);

	// Custom Commands
	auto commandSetting = m_settings->registerSetting({"OverrideCommands","OverrideLaunchCmd"}, false);
	m_settings->registerOverride(globalSettings->getSetting("PreLaunchCommand"), commandSetting);
	m_settings->registerOverride(globalSettings->getSetting("WrapperCommand"), commandSetting);
	m_settings->registerOverride(globalSettings->getSetting("PostExitCommand"), commandSetting);

	// Console
	auto consoleSetting = m_settings->registerSetting("OverrideConsole", false);
	m_settings->registerOverride(globalSettings->getSetting("ShowConsole"), consoleSetting);
	m_settings->registerOverride(globalSettings->getSetting("AutoCloseConsole"), consoleSetting);
	m_settings->registerOverride(globalSettings->getSetting("LogPrePostOutput"), consoleSetting);
}

QString BaseInstance::getPreLaunchCommand()
{
	return settings()->get("PreLaunchCommand").toString();
}

QString BaseInstance::getWrapperCommand()
{
	return settings()->get("WrapperCommand").toString();
}

QString BaseInstance::getPostExitCommand()
{
	return settings()->get("PostExitCommand").toString();
}

void BaseInstance::iconUpdated(QString key)
{
	if(iconKey() == key)
	{
		emit propertiesChanged(this);
	}
}

void BaseInstance::invalidate()
{
	// TODO: actually do something here.
	qDebug() << "Instance" << id() << "has been invalidated.";
}

void BaseInstance::nuke()
{
	FS::deletePath(instanceRoot());
	emit nuked(this);
}

QString BaseInstance::id() const
{
	return QFileInfo(instanceRoot()).fileName();
}

bool BaseInstance::isRunning() const
{
	return m_isRunning;
}

void BaseInstance::setRunning(bool running)
{
	if(running == m_isRunning)
		return;

	if(running)
	{
		m_timeStarted = QDateTime::currentDateTime();
	}
	else
	{
		qint64 current = settings()->get("totalTimePlayed").toLongLong();
		QDateTime timeEnded = QDateTime::currentDateTime();
		settings()->set("totalTimePlayed", current + m_timeStarted.secsTo(timeEnded));
		emit propertiesChanged(this);
	}
	m_isRunning = running;

	emit runningStatusChanged(running);
}

int64_t BaseInstance::totalTimePlayed() const
{
	qint64 current = settings()->get("totalTimePlayed").toLongLong();
	if(m_isRunning)
	{
		QDateTime timeNow = QDateTime::currentDateTime();
		return current + m_timeStarted.secsTo(timeNow);
	}
	return current;
}

void BaseInstance::resetTimePlayed()
{
	settings()->reset("totalTimePlayed");
}

QString BaseInstance::instanceType() const
{
	return m_settings->get("InstanceType").toString();
}

QString BaseInstance::instanceRoot() const
{
	return m_rootDir;
}

InstancePtr BaseInstance::getSharedPtr()
{
	return shared_from_this();
}

SettingsObjectPtr BaseInstance::settings() const
{
	return m_settings;
}

BaseInstance::InstanceFlags BaseInstance::flags() const
{
	return m_flags;
}

void BaseInstance::setFlags(const InstanceFlags &flags)
{
	if (flags != m_flags)
	{
		m_flags = flags;
		emit flagsChanged();
		emit propertiesChanged(this);
	}
}

void BaseInstance::setFlag(const BaseInstance::InstanceFlag flag)
{
	// nothing to set?
	if(flag & m_flags)
		return;
	m_flags |= flag;
	emit flagsChanged();
	emit propertiesChanged(this);
}

void BaseInstance::unsetFlag(const BaseInstance::InstanceFlag flag)
{
	// nothing to unset?
	if(!(flag & m_flags))
		return;
	m_flags &= ~flag;
	emit flagsChanged();
	emit propertiesChanged(this);
}

bool BaseInstance::canLaunch() const
{
	return (!(flags() & VersionBrokenFlag)) && (!isRunning());
}

bool BaseInstance::reload()
{
	return m_settings->reload();
}

qint64 BaseInstance::lastLaunch() const
{
	return m_settings->get("lastLaunchTime").value<qint64>();
}

void BaseInstance::setLastLaunch(qint64 val)
{
	//FIXME: if no change, do not set. setting involves saving a file.
	m_settings->set("lastLaunchTime", val);
	emit propertiesChanged(this);
}

void BaseInstance::setGroupInitial(QString val)
{
	if(m_group == val)
	{
		return;
	}
	m_group = val;
	emit propertiesChanged(this);
}

void BaseInstance::setGroupPost(QString val)
{
	if(m_group == val)
	{
		return;
	}
	setGroupInitial(val);
	emit groupChanged();
}

QString BaseInstance::group() const
{
	return m_group;
}

void BaseInstance::setNotes(QString val)
{
	//FIXME: if no change, do not set. setting involves saving a file.
	m_settings->set("notes", val);
}

QString BaseInstance::notes() const
{
	return m_settings->get("notes").toString();
}

void BaseInstance::setIconKey(QString val)
{
	//FIXME: if no change, do not set. setting involves saving a file.
	m_settings->set("iconKey", val);
	emit propertiesChanged(this);
}

QString BaseInstance::iconKey() const
{
	return m_settings->get("iconKey").toString();
}

void BaseInstance::setName(QString val)
{
	//FIXME: if no change, do not set. setting involves saving a file.
	m_settings->set("name", val);
	emit propertiesChanged(this);
}

QString BaseInstance::name() const
{
	return m_settings->get("name").toString();
}

QString BaseInstance::windowTitle() const
{
	return "MultiMC: " + name();
}

QStringList BaseInstance::extraArguments() const
{
	return Commandline::splitArgs(settings()->get("JvmArgs").toString());
}

std::shared_ptr<LaunchTask> BaseInstance::getLaunchTask()
{
	return m_launchProcess;
}

void BaseInstance::setProvider(BaseInstanceProvider* provider)
{
	// only once.
	assert(!m_provider);
	if(m_provider)
	{
		qWarning() << "Provider set more than once for instance" << id();
	}
	m_provider = provider;
}

BaseInstanceProvider* BaseInstance::provider() const
{
	return m_provider;
}
