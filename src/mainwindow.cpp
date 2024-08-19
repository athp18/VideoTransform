#include "mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFormLayout>
#include <QFuture>
#include <QtConcurrent>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), currentFile("")
{
    setupUi();
    connectSlots();
    ffmpegWrapper = new FFmpegWrapper(this);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    setWindowTitle("Video Transform");

    player = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    player->setAudioOutput(audioOutput);
    videoWidget = new QVideoWidget(this);
    player->setVideoOutput(videoWidget);

    openButton = new QPushButton("Open", this);
    playButton = new QPushButton("Play", this);
    playButton->setEnabled(false);
    positionSlider = new QSlider(Qt::Horizontal, this);

    processButton = new QPushButton("Process Video", this);
    processButton->setEnabled(false);

    cropXInput = new QLineEdit(this);
    cropYInput = new QLineEdit(this);
    cropWidthInput = new QLineEdit(this);
    cropHeightInput = new QLineEdit(this);
    rescaleWidthInput = new QLineEdit(this);
    rescaleHeightInput = new QLineEdit(this);
    trimStartInput = new QLineEdit(this);
    trimEndInput = new QLineEdit(this);
    outputFormatInput = new QLineEdit(this);

    processProgressBar = new QProgressBar(this);
    processProgressBar->setRange(0, 100);
    processProgressBar->setValue(0);

    statusLabel = new QLabel("Ready", this);

    QFormLayout *formLayout = new QFormLayout;
    formLayout->addRow("Crop X:", cropXInput);
    formLayout->addRow("Crop Y:", cropYInput);
    formLayout->addRow("Crop Width:", cropWidthInput);
    formLayout->addRow("Crop Height:", cropHeightInput);
    formLayout->addRow("Rescale Width:", rescaleWidthInput);
    formLayout->addRow("Rescale Height:", rescaleHeightInput);
    formLayout->addRow("Trim Start (s):", trimStartInput);
    formLayout->addRow("Trim End (s):", trimEndInput);
    formLayout->addRow("Output Format:", outputFormatInput);

    controlLayout = new QHBoxLayout;
    controlLayout->addWidget(openButton);
    controlLayout->addWidget(playButton);
    controlLayout->addWidget(positionSlider);

    mainLayout = new QVBoxLayout;
    mainLayout->addWidget(videoWidget);
    mainLayout->addLayout(controlLayout);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(processButton);
    mainLayout->addWidget(processProgressBar);
    mainLayout->addWidget(statusLabel);

    QWidget *centralWidget = new QWidget(this);
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);

    resize(800, 600);
}

void MainWindow::connectSlots()
{
    connect(openButton, &QPushButton::clicked, this, &MainWindow::openFile);
    connect(playButton, &QPushButton::clicked, this, &MainWindow::playPause);
    connect(positionSlider, &QSlider::sliderMoved, this, &MainWindow::setPosition);
    connect(player, &QMediaPlayer::durationChanged, this, &MainWindow::updateDuration);
    connect(player, &QMediaPlayer::positionChanged, this, &MainWindow::updatePosition);
    connect(processButton, &QPushButton::clicked, this, &MainWindow::startVideoProcessing);
    connect(ffmpegWrapper, &FFmpegWrapper::progressUpdated, this, &MainWindow::updateProgress);
    connect(ffmpegWrapper, &FFmpegWrapper::errorOccurred, this, &MainWindow::displayError);
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open Video File", "", "Video Files (*.mp4 *.avi *.mkv)");
    if (!fileName.isEmpty()) {
        currentFile = fileName;
        player->setSource(QUrl::fromLocalFile(fileName));
        playButton->setEnabled(true);
        processButton->setEnabled(true);
        statusLabel->setText("File loaded: " + fileName);
    }
}

void MainWindow::playPause()
{
    if (player->playbackState() == QMediaPlayer::PlayingState) {
        player->pause();
        playButton->setText("Play");
    } else {
        player->play();
        playButton->setText("Pause");
    }
}

void MainWindow::setPosition(int position)
{
    player->setPosition(position);
}

void MainWindow::updateDuration(qint64 duration)
{
    positionSlider->setRange(0, duration);
}

void MainWindow::updatePosition(qint64 position)
{
    positionSlider->setValue(position);
}

void MainWindow::startVideoProcessing()
{
    if (currentFile.isEmpty()) {
        QMessageBox::warning(this, "Error", "No video file loaded.");
        return;
    }

    QString outputFile = QFileDialog::getSaveFileName(this, "Save Processed Video", "", "Video Files (*.mp4 *.avi *.mkv)");
    if (outputFile.isEmpty()) {
        return;
    }

    if (!ffmpegWrapper->openInputFile(currentFile)) {
        return;
    }

    if (!ffmpegWrapper->setupOutputFile(outputFile)) {
        return;
    }

    // Set transformation parameters
    ffmpegWrapper->cropVideo(cropXInput->text().toInt(), cropYInput->text().toInt(),
                             cropWidthInput->text().toInt(), cropHeightInput->text().toInt());
    ffmpegWrapper->rescaleVideo(rescaleWidthInput->text().toInt(), rescaleHeightInput->text().toInt());
    ffmpegWrapper->trimVideo(trimStartInput->text().toDouble(), trimEndInput->text().toDouble());
    ffmpegWrapper->convertFormat(outputFormatInput->text());

    statusLabel->setText("Processing video...");
    processProgressBar->setValue(0);

    // Start processing in a separate thread to keep UI responsive
    QFuture<void> future = QtConcurrent::run([this]() {
        if (ffmpegWrapper->processVideo()) {
            QMetaObject::invokeMethod(statusLabel, "setText", Q_ARG(QString, "Processing complete"));
        }
    });
}

void MainWindow::updateProgress(int progress)
{
    processProgressBar->setValue(progress);
}

void MainWindow::displayError(const QString& error)
{
    QMessageBox::critical(this, "Error", error);
    statusLabel->setText("Error: " + error);
}