
#include <QThread>
#include <QAtomicPointer>

class VideoThread : public QThread
{
	Q_OBJECT
public:
	VideoThread(QObject *parent = nullptr);
	~VideoThread();
	void run() override;
	void setVideoPath(const QString& videoPath);
private:
	QAtomicPointer<QString> m_videoPath;
Q_SIGNALS:
	void videoAvailable(bool available);
};
