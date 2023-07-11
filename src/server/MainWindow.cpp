
#include "MainWindow.h"
#include "VideoThread.h"

//Qt
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

MainWindow::MainWindow(QWidget* parent /*= nullptr*/)
{
	initUI();

	m_videoThread = new VideoThread();
	m_videoThread->setVideoPath("");
	m_videoThread->start();
	connect(m_videoThread, &VideoThread::videoAvailable, this, &MainWindow::videoAvailableChanged);
}

MainWindow::~MainWindow()
{
	delete m_videoThread;
}

void MainWindow::initUI()
{
	QHBoxLayout* hlayout = new QHBoxLayout;
	QLabel* infoLabel = new QLabel;
	QLineEdit* videoPathLineEdit = new QLineEdit;
	QPushButton* confirmBtn = new QPushButton;
	QWidget* paintWidget = new QWidget;

	infoLabel->setText("Input stream");
	connect(confirmBtn, &QPushButton::clicked, this, [=] {
		m_videoThread->setVideoPath(videoPathLineEdit->text());
		});
	hlayout->addWidget(infoLabel);
	hlayout->addWidget(videoPathLineEdit);
	hlayout->addWidget(confirmBtn);


	QGridLayout* gridLayout = new QGridLayout;
	gridLayout->addLayout(hlayout, 0, 0, 0, 0);
	gridLayout->addWidget(paintWidget, 1, 0, 0, 0);

	this->setLayout(gridLayout);
}

void MainWindow::receiveImage()
{

}

void MainWindow::videoAvailableChanged(bool available)
{
	this->repaint();
}

void MainWindow::closeEvent(QCloseEvent* event)
{

}

void MainWindow::paintEvent(QPaintEvent* event)
{

}

