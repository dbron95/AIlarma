

#include <QMainWindow>
class VideoThread;

class MainWindow : public QMainWindow
{
	Q_OBJECT
public:
	MainWindow(QWidget* parent = nullptr);
	~MainWindow();
private:
	void initUI();
	VideoThread *m_videoThread;
private Q_SLOTS:
	void receiveImage();
	void videoAvailableChanged(bool available);
protected:
	void closeEvent(QCloseEvent* event) override;
	void paintEvent(QPaintEvent* event) override;
};