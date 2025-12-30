#ifndef UIANIMATION_H
#define UIANIMATION_H

#include <Arduino.h>

// ============================================================
// UI Animation Engine - Inspired by OLED_UI
// Provides smooth animations for UI elements
// ============================================================

// Animation easing types
enum class EasingType {
    UNLINEAR,    // Non-linear easing (OLED_UI style) - smooth deceleration
    PID,         // PID curve animation - smoother acceleration/deceleration
    LINEAR       // Linear interpolation
};

// ============================================================
// AnimatedValue - Single value animation object
// Used for: cursor Y position, scrollbar position, etc.
// ============================================================
class AnimatedValue {
public:
    AnimatedValue(float initialValue = 0.0f);

    // Set target value (starts animation)
    void setTarget(float target);

    // Immediately jump to value (no animation)
    void snapTo(float value);

    // Update animation (call every frame)
    // deltaMs: milliseconds since last update
    void update(unsigned long deltaMs);

    // Get current animated value
    float getValue() const;

    // Get target value
    float getTarget() const;

    // Check if animation is in progress
    bool isAnimating() const;

    // Configuration
    void setSpeed(float speed);           // Animation speed (default 1.0)
    void setEasing(EasingType easing);    // Easing type
    void setThreshold(float threshold);   // Arrival threshold (default 0.5)

private:
    float current_;
    float target_;
    float speed_;
    float threshold_;
    EasingType easing_;

    // PID controller state
    float integral_;
    float lastError_;
};

// ============================================================
// AnimatedRect - Rectangle area animation object
// Used for: selection highlight box, segment control fill
// ============================================================
struct AnimatedRect {
    AnimatedValue x;
    AnimatedValue y;
    AnimatedValue w;
    AnimatedValue h;

    AnimatedRect(float x = 0, float y = 0, float w = 0, float h = 0);

    void setTarget(float tx, float ty, float tw, float th);
    void snapTo(float sx, float sy, float sw, float sh);
    void update(unsigned long deltaMs);
    bool isAnimating() const;

    // Configure speed for all dimensions
    void setSpeed(float speed);
    void setEasing(EasingType easing);
};

// ============================================================
// TaskListAnimationState - Task list animation state
// Encapsulates all animation states needed for task list page
// ============================================================
struct TaskListAnimationState {
    // Cursor/selection card animation
    AnimatedValue cursorY;           // Selected card Y position

    // List scroll animation
    AnimatedValue scrollOffset;      // List scroll offset (pixel level)

    // Segment control animation
    AnimatedValue segmentHighlightX; // Segment control highlight X position

    // Scrollbar animation
    AnimatedValue scrollbarY;        // Scrollbar knob Y position

    // Constructor - initializes with default values
    TaskListAnimationState();

    // Update all animations
    void updateAll(unsigned long deltaMs);

    // Check if any animation is in progress
    bool isAnyAnimating() const;

    // Reset all animations to initial state
    void reset();

    // Snap all animations to their targets (no animation)
    void snapAllToTargets();
};

// ============================================================
// FrameRateController - Frame rate limiter
// Ensures consistent frame rate and provides delta time
// ============================================================
class FrameRateController {
public:
    FrameRateController(uint8_t targetFps = 30);

    // Check if should render new frame
    // Returns true and sets deltaMs when it's time to render
    bool shouldRender(unsigned long& deltaMs);

    // Get actual frame rate
    float getActualFps() const;

    // Reset timing (call when entering state)
    void reset();

private:
    uint8_t targetFps_;
    unsigned long frameInterval_;
    unsigned long lastFrameTime_;
    float actualFps_;
};

#endif // UIANIMATION_H
