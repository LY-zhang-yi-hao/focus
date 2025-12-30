#include "UIAnimation.h"
#include <cmath>

// ============================================================
// AnimatedValue Implementation
// ============================================================

AnimatedValue::AnimatedValue(float initialValue)
    : current_(initialValue),
      target_(initialValue),
      speed_(1.0f),
      threshold_(0.5f),
      easing_(EasingType::UNLINEAR),
      integral_(0.0f),
      lastError_(0.0f)
{
}

void AnimatedValue::setTarget(float target) {
    target_ = target;
}

void AnimatedValue::snapTo(float value) {
    current_ = value;
    target_ = value;
    integral_ = 0.0f;
    lastError_ = 0.0f;
}

void AnimatedValue::update(unsigned long deltaMs) {
    if (!isAnimating()) return;

    float error = target_ - current_;

    // When close to target, snap to it (avoid infinite approach)
    if (fabs(error) < threshold_) {
        current_ = target_;
        integral_ = 0.0f;
        lastError_ = 0.0f;
        return;
    }

    // Time factor (normalized to ~16ms baseline for 60fps)
    float timeFactor = deltaMs / 16.0f;
    if (timeFactor < 0.1f) timeFactor = 0.1f;  // Prevent division issues
    if (timeFactor > 4.0f) timeFactor = 4.0f;  // Cap for stability

    switch (easing_) {
        case EasingType::UNLINEAR: {
            // OLED_UI style non-linear easing
            // CurrentNum += 0.02f * Speed * Error
            // Using higher coefficient for faster response (~150ms)
            float delta = 0.08f * speed_ * error * timeFactor;
            current_ += delta;
            break;
        }

        case EasingType::PID: {
            // PID controller for smoother motion
            float Kp = 0.08f * speed_;
            float Ki = 0.02f * speed_;
            float Kd = 0.005f;

            integral_ += error * timeFactor;
            // Integral windup prevention
            float maxIntegral = 50.0f / Ki;
            integral_ = constrain(integral_, -maxIntegral, maxIntegral);

            float derivative = (error - lastError_) / timeFactor;
            lastError_ = error;

            float output = Kp * error + Ki * integral_ + Kd * derivative;
            current_ += output * timeFactor;
            break;
        }

        case EasingType::LINEAR: {
            // Linear interpolation
            float maxDelta = speed_ * 4.0f * timeFactor;
            if (fabs(error) <= maxDelta) {
                current_ = target_;
            } else {
                current_ += (error > 0 ? maxDelta : -maxDelta);
            }
            break;
        }
    }
}

float AnimatedValue::getValue() const {
    return current_;
}

float AnimatedValue::getTarget() const {
    return target_;
}

bool AnimatedValue::isAnimating() const {
    return fabs(target_ - current_) >= threshold_;
}

void AnimatedValue::setSpeed(float speed) {
    speed_ = speed;
}

void AnimatedValue::setEasing(EasingType easing) {
    easing_ = easing;
}

void AnimatedValue::setThreshold(float threshold) {
    threshold_ = threshold;
}

// ============================================================
// AnimatedRect Implementation
// ============================================================

AnimatedRect::AnimatedRect(float ix, float iy, float iw, float ih)
    : x(ix), y(iy), w(iw), h(ih)
{
}

void AnimatedRect::setTarget(float tx, float ty, float tw, float th) {
    x.setTarget(tx);
    y.setTarget(ty);
    w.setTarget(tw);
    h.setTarget(th);
}

void AnimatedRect::snapTo(float sx, float sy, float sw, float sh) {
    x.snapTo(sx);
    y.snapTo(sy);
    w.snapTo(sw);
    h.snapTo(sh);
}

void AnimatedRect::update(unsigned long deltaMs) {
    x.update(deltaMs);
    y.update(deltaMs);
    w.update(deltaMs);
    h.update(deltaMs);
}

bool AnimatedRect::isAnimating() const {
    return x.isAnimating() || y.isAnimating() ||
           w.isAnimating() || h.isAnimating();
}

void AnimatedRect::setSpeed(float speed) {
    x.setSpeed(speed);
    y.setSpeed(speed);
    w.setSpeed(speed);
    h.setSpeed(speed);
}

void AnimatedRect::setEasing(EasingType easing) {
    x.setEasing(easing);
    y.setEasing(easing);
    w.setEasing(easing);
    h.setEasing(easing);
}

// ============================================================
// TaskListAnimationState Implementation
// ============================================================

// Layout constants (must match DisplayController)
static const int LIST_TOP = 16;
static const int CARD_H = 16;
static const int CARD_GAP = 4;
static const int SEG_X = 25;
static const int SEG_HALF_W = 39;  // 78 / 2
static const int SCROLLBAR_INNER_Y = 17;

TaskListAnimationState::TaskListAnimationState()
    : cursorY(LIST_TOP),                    // First card position
      scrollOffset(0.0f),
      segmentHighlightX(SEG_X + 1),         // "Pending" position
      scrollbarY(SCROLLBAR_INNER_Y)
{
    // Configure animation speeds (fast style, ~150ms)
    cursorY.setSpeed(2.5f);
    cursorY.setThreshold(0.3f);
    cursorY.setEasing(EasingType::UNLINEAR);

    scrollOffset.setSpeed(2.0f);
    scrollOffset.setThreshold(0.3f);
    scrollOffset.setEasing(EasingType::UNLINEAR);

    segmentHighlightX.setSpeed(3.0f);
    segmentHighlightX.setThreshold(0.5f);
    segmentHighlightX.setEasing(EasingType::UNLINEAR);

    scrollbarY.setSpeed(2.5f);
    scrollbarY.setThreshold(0.3f);
    scrollbarY.setEasing(EasingType::UNLINEAR);
}

void TaskListAnimationState::updateAll(unsigned long deltaMs) {
    cursorY.update(deltaMs);
    scrollOffset.update(deltaMs);
    segmentHighlightX.update(deltaMs);
    scrollbarY.update(deltaMs);
}

bool TaskListAnimationState::isAnyAnimating() const {
    return cursorY.isAnimating() ||
           scrollOffset.isAnimating() ||
           segmentHighlightX.isAnimating() ||
           scrollbarY.isAnimating();
}

void TaskListAnimationState::reset() {
    cursorY.snapTo(LIST_TOP);
    scrollOffset.snapTo(0.0f);
    segmentHighlightX.snapTo(SEG_X + 1);
    scrollbarY.snapTo(SCROLLBAR_INNER_Y);
}

void TaskListAnimationState::snapAllToTargets() {
    cursorY.snapTo(cursorY.getTarget());
    scrollOffset.snapTo(scrollOffset.getTarget());
    segmentHighlightX.snapTo(segmentHighlightX.getTarget());
    scrollbarY.snapTo(scrollbarY.getTarget());
}

// ============================================================
// FrameRateController Implementation
// ============================================================

FrameRateController::FrameRateController(uint8_t fps)
    : targetFps_(fps),
      frameInterval_(1000 / fps),
      lastFrameTime_(0),
      actualFps_(0.0f)
{
}

bool FrameRateController::shouldRender(unsigned long& deltaMs) {
    unsigned long now = millis();
    deltaMs = now - lastFrameTime_;

    if (deltaMs >= frameInterval_) {
        // Calculate actual frame rate
        if (deltaMs > 0) {
            actualFps_ = 1000.0f / deltaMs;
        }
        lastFrameTime_ = now;
        return true;
    }
    return false;
}

float FrameRateController::getActualFps() const {
    return actualFps_;
}

void FrameRateController::reset() {
    lastFrameTime_ = millis();
    actualFps_ = 0.0f;
}
