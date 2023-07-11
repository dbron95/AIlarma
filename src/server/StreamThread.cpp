#include "StreamThread.h"

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QVector>

StreamThread::StreamThread(QObject* parent /*= nullptr*/)
{

}

StreamThread::~StreamThread()
{

}

void StreamThread::run()
{
	bool error = false;
	while (!this->isInterruptionRequested()) {
		m_mutex.lock();
		while (m_video_frames.isEmpty() && !this->isInterruptionRequested()) {
			m_WaitAnyItem.wait(&m_mutex);
		}
		if (this->isInterruptionRequested()) {
			m_mutex.unlock();
			break;
		}
		if (!m_video_frames.isEmpty()) {
			//do stuff
		}
		m_mutex.unlock();
	}
	m_WaitAnyItem.wakeAll();
}

void StreamThread::addVideoFrame(const BufferRGB8& videoFrame)
{
	m_mutex.lock();
	bool empty = m_video_frames.isEmpty();
	m_video_frames.append(videoFrame);
	if (empty)
		m_WaitAnyItem.wakeOne();
	m_mutex.unlock();
}

