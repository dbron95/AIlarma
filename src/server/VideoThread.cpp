#include "VideoThread.h"
#include <QThread>
#include <QAtomicPointer>

VideoThread::VideoThread(QObject* parent /*= nullptr*/):
	QThread(parent)
{

}

VideoThread::~VideoThread()
{
	if (this->isRunning())
	{
		this->requestInterruption();
		while (this->isRunning())
		{
			QThread::msleep(10);
		}
	}
}

void VideoThread::run()
{
	Media_Video videoIn;
	while (!this->isInterruptionRequested() && videoIn.Open(m_videoPath.load()->toStdString()) != VE_Ok)
	{
		QThread::sleep(1);
	}
	if (this->isInterruptionRequested())
	{
		return;
	}
	Q_EMIT videoAvailable(true);
	int error = VE_Ok;
	while (!this->isInterruptionRequested() && error == VE_Ok)
	{

	}
	Q_EMIT videoAvailable(false);
}

void VideoThread::setVideoPath(const QString& videoPath)
{
	m_videoPath.store(videoPath);
	if (this->isRunning())
	{
		this->requestInterruption();
		while (this->isRunning())
		{
			QThread::msleep(10);
		}
		this->start();
	}
}
