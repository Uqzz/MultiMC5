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

#pragma once

#include <QMainWindow>
#include "LaunchController.h"
#include <QObjectPtr.h>
#include <QSystemTrayIcon>
#include "launch/LaunchTask.h"
#include "pages/BasePageContainer.h"

class QPushButton;
class PageContainer;
class InstanceWindow : public QMainWindow, public BasePageContainer
{
	Q_OBJECT

public:
	explicit InstanceWindow(InstancePtr proc, QWidget *parent = 0);
	virtual ~InstanceWindow();

	bool selectPage(QString pageId) override;

	QString instanceId();

	void setQuitOnClose(bool shouldQuit = false)
	{
		m_shouldQuit = shouldQuit;
	}

signals:
	void isClosing();

private
slots:
	void on_closeButton_clicked();
	void on_btnKillMinecraft_clicked();

	void onSucceeded();
	void onFailed(QString reason);
	void onProgressRequested(Task *task);

	void on_InstanceLaunchTask_changed(std::shared_ptr<LaunchTask> proc);
	void on_RunningState_changed(bool running);

protected:
	void closeEvent(QCloseEvent *) override;

private:
	void setKillButton(bool kill);

private:
	std::shared_ptr<LaunchTask> m_proc;
	unique_qobject_ptr<LaunchController> m_launchController;
	InstancePtr m_instance;
	bool m_shouldQuit = false;
	PageContainer *m_container = nullptr;
	QPushButton *m_closeButton = nullptr;
	QPushButton *m_killButton = nullptr;
};
