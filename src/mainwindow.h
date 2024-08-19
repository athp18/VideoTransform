#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QProgressBar>
#include <QLabel>
#include "ffmpegwrapper.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openFile();
    void playPause();
    void setPosition(int position);
    void updateDuration(qint64 duration);
    void updatePosition(qint64 position);
    void startVideoProcessing();
    void updateProgress(int progress);
    void displayError(const QString& error);

private:
    QMediaPlayer *player;
    QAudioOutput *audioOutput;
    QVideoWidget *videoWidget;
    QPushButton *openButton;
    QPushButton *playButton;
    QSlider *positionSlider;
    QVBoxLayout *mainLayout;
    QHBoxLayout *controlLayout;

    // New UI elements for video processing
    QPushButton *processButton;
    QLineEdit *cropXInput, *cropYInput, *cropWidthInput, *cropHeightInput;
    QLineEdit *rescaleWidthInput, *rescaleHeightInput;
    QLineEdit *trimStartInput, *trimEndInput;
    QLineEdit *outputFormatInput;
    QProgressBar *processProgressBar;
    QLabel *statusLabel;

    FFmpegWrapper *ffmpegWrapper;
    QString currentFile;

    void setupUi();
    void connectSlots();
};

#endif // MAINWINDOW_H