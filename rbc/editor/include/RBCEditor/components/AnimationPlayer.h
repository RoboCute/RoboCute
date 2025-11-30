#pragma once

#include <QWidget>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QString>

namespace rbc {

/**
 * Animation Player Widget
 * 
 * Provides timeline controls for animation playback:
 * - Timeline slider for frame scrubbing
 * - Play/Pause button
 * - Frame counter display
 */
class AnimationPlayer : public QWidget {
    Q_OBJECT

public:
    explicit AnimationPlayer(QWidget *parent = nullptr);
    
    // Set animation parameters
    void setAnimation(const QString &name, int totalFrames, float fps);
    
    // Clear animation
    void clear();
    
    // Playback control
    void play();
    void pause();
    bool isPlaying() const { return isPlaying_; }
    
    // Frame control
    void setFrame(int frame);
    int currentFrame() const;

signals:
    // Emitted when frame changes (either by slider or playback)
    void frameChanged(int frame);
    
    // Emitted when play state changes
    void playStateChanged(bool playing);

private slots:
    void onPlayPauseClicked();
    void onSliderValueChanged(int value);
    void onTimerTick();

private:
    void setupUi();
    void updatePlayButton();
    void updateFrameLabel();

    QSlider *timelineSlider_;
    QPushButton *playPauseButton_;
    QLabel *frameLabel_;
    QLabel *animNameLabel_;
    
    QString animationName_;
    int totalFrames_;
    float fps_;
    int currentFrame_;
    bool isPlaying_;
    
    QTimer *playbackTimer_;
};

}  // namespace rbc

