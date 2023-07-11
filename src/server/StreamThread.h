class QThread;
class QMutex;
class QWaitCondition;

class StreamThread : public QThread
{
	Q_OBJECT
public:
	StreamThread(QObject* parent = nullptr);
	~StreamThread();
	void run() override;
	void addVideoFrame(const BufferRGB8& videoFrame);
private:
	QMutex m_mutex;
	QWaitCondition m_WaitAnyItem;
	QVector<BufferRGB8> m_video_frames;
};