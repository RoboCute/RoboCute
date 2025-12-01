#include "RBCEditor/components/AnimationPlayer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QCheckBox>
#include <QTimer>

namespace rbc {

AnimationPlayer::AnimationPlayer(QWidget *parent)
    : QWidget(parent),
      totalFrames_(0),
      fps_(30.0f),
      currentFrame_(0),
      isPlaying_(false),
      isLooping_(true) {
    isAutoUpdate_.store(false);
    setupUi();

    // Setup playback timer
    playbackTimer_ = new QTimer(this);
    connect(playbackTimer_, &QTimer::timeout, this, &AnimationPlayer::onTimerTick);
}

void AnimationPlayer::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);

    // Animation name label
    animNameLabel_ = new QLabel("No animation loaded", this);
    QFont nameFont = animNameLabel_->font();
    nameFont.setBold(true);
    animNameLabel_->setFont(nameFont);
    mainLayout->addWidget(animNameLabel_);

    // Timeline slider
    timelineSlider_ = new QSlider(Qt::Horizontal, this);
    timelineSlider_->setMinimum(0);
    timelineSlider_->setMaximum(0);
    timelineSlider_->setValue(0);
    timelineSlider_->setEnabled(false);
    mainLayout->addWidget(timelineSlider_);

    // Controls layout
    auto *controlsLayout = new QHBoxLayout();

    // Play/Pause button
    playPauseButton_ = new QPushButton("Play", this);
    playPauseButton_->setEnabled(false);
    playPauseButton_->setFixedWidth(80);
    controlsLayout->addWidget(playPauseButton_);

    // Loop checkbox
    loopCheckBox_ = new QCheckBox("Loop", this);
    loopCheckBox_->setChecked(true);
    loopCheckBox_->setEnabled(false);
    controlsLayout->addWidget(loopCheckBox_);

    // Frame label
    frameLabel_ = new QLabel("Frame: 0 / 0", this);
    controlsLayout->addWidget(frameLabel_);

    controlsLayout->addStretch();

    mainLayout->addLayout(controlsLayout);

    // Connect signals
    connect(playPauseButton_, &QPushButton::clicked,
            this, &AnimationPlayer::onPlayPauseClicked);
    connect(timelineSlider_, &QSlider::valueChanged,
            this, &AnimationPlayer::onSliderValueChanged);
    connect(loopCheckBox_, &QCheckBox::toggled,
            this, &AnimationPlayer::setLoop);

    setLayout(mainLayout);
}

void AnimationPlayer::setAnimation(const QString &name, int totalFrames, float fps) {
    // Stop playback if active
    if (isPlaying_) {
        pause();
    }

    animationName_ = name;
    totalFrames_ = totalFrames;
    fps_ = fps;
    currentFrame_ = 0;

    // Update UI
    animNameLabel_->setText(QString("Animation: %1").arg(name));

    timelineSlider_->setMaximum(totalFrames);
    timelineSlider_->setValue(0);
    timelineSlider_->setEnabled(true);

    playPauseButton_->setEnabled(true);
    loopCheckBox_->setEnabled(true);

    updateFrameLabel();

    // Set timer interval based on FPS
    if (fps > 0) {
        int intervalMs = static_cast<int>(1000.0f / fps);
        playbackTimer_->setInterval(intervalMs);
    }
}

void AnimationPlayer::clear() {
    pause();

    animationName_ = "";
    totalFrames_ = 0;
    currentFrame_ = 0;

    animNameLabel_->setText("No animation loaded");

    timelineSlider_->setMaximum(0);
    timelineSlider_->setValue(0);
    timelineSlider_->setEnabled(false);

    playPauseButton_->setEnabled(false);
    loopCheckBox_->setEnabled(false);

    updateFrameLabel();
}

void AnimationPlayer::play() {
    if (totalFrames_ <= 0) return;

    isPlaying_ = true;
    playbackTimer_->start();
    updatePlayButton();

    emit playStateChanged(true);
}

void AnimationPlayer::pause() {
    isPlaying_ = false;
    playbackTimer_->stop();
    updatePlayButton();

    emit playStateChanged(false);
}

void AnimationPlayer::setFrame(int frame) {
    isAutoUpdate_.store(true);
    if (frame < 0) frame = 0;
    if (frame > totalFrames_) frame = totalFrames_;
    currentFrame_ = frame;
    timelineSlider_->setValue(frame);
    updateFrameLabel();
    isAutoUpdate_.store(false);

    emit frameChanged(frame);
}

int AnimationPlayer::currentFrame() const {
    return currentFrame_;
}

void AnimationPlayer::onPlayPauseClicked() {
    if (isPlaying_) {
        pause();
    } else {
        play();
    }
}

void AnimationPlayer::onSliderValueChanged(int value) {
    // Pause playback when user manually scrubs
    if (isPlaying_ && sender() == timelineSlider_ && !isAutoUpdate_.load()) {
        pause();
    }

    currentFrame_ = value;
    updateFrameLabel();

    emit frameChanged(value);
}

void AnimationPlayer::onTimerTick() {
    // Programmatic Update Frame
    if (!isPlaying_) return;

    currentFrame_++;

    // Handle end of animation
    if (currentFrame_ > totalFrames_) {
        if (isLooping_) {
            // Loop: reset to beginning
            currentFrame_ = 0;
        } else {
            // No loop: pause at last frame
            currentFrame_ = totalFrames_;
            pause();
        }
    }

    isAutoUpdate_.store(true);
    timelineSlider_->setValue(currentFrame_);
    isAutoUpdate_.store(false);

    updateFrameLabel();

    emit frameChanged(currentFrame_);
}

void AnimationPlayer::updatePlayButton() {
    if (isPlaying_) {
        playPauseButton_->setText("Pause");
    } else {
        playPauseButton_->setText("Play");
    }
}

void AnimationPlayer::updateFrameLabel() {
    frameLabel_->setText(QString("Frame: %1 / %2").arg(currentFrame_).arg(totalFrames_));
}

void AnimationPlayer::setLoop(bool loop) {
    isLooping_ = loop;
}

}// namespace rbc
