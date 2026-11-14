#ifndef BUTTON_INPUT_H
#define BUTTON_INPUT_H

#include <cstdint>

namespace button_input {

struct Events {
    bool pressed = false;
    bool released = false;
    bool click = false;
    bool long_press = false;
    uint32_t duration_ms = 0;
};

// Shared debouncing and hold semantics for KEY0 and the encoder button.
class Button {
public:
    void reset(bool pressed, uint32_t now_ms, bool ignore_until_release = false) {
        raw_ = stable_ = pressed;
        changed_at_ = pressed_at_ = now_ms;
        active_ = pressed && !ignore_until_release;
        blocked_ = pressed && ignore_until_release;
        long_sent_ = false;
    }

    Events update(bool pressed, uint32_t now_ms, uint32_t hold_ms, uint32_t debounce_ms = 25) {
        Events events;
        if (pressed != raw_) {
            raw_ = pressed;
            changed_at_ = now_ms;
        }
        if (raw_ != stable_ && now_ms - changed_at_ >= debounce_ms) {
            stable_ = raw_;
            if (stable_ && !blocked_) {
                active_ = true;
                long_sent_ = false;
                pressed_at_ = now_ms;
                events.pressed = true;
            } else if (!stable_) {
                if (active_) {
                    events.released = true;
                    events.duration_ms = now_ms - pressed_at_;
                    events.long_press = !long_sent_ && events.duration_ms >= hold_ms;
                    events.click = !long_sent_ && !events.long_press;
                }
                active_ = false;
                blocked_ = false;
            }
        }
        if (stable_ && raw_ && active_ && !long_sent_ && now_ms - pressed_at_ >= hold_ms) {
            long_sent_ = true;
            events.long_press = true;
            events.duration_ms = now_ms - pressed_at_;
        }
        return events;
    }

    bool pressed() const { return stable_ && active_; }

private:
    bool raw_ = false;
    bool stable_ = false;
    bool active_ = false;
    bool blocked_ = false;
    bool long_sent_ = false;
    uint32_t changed_at_ = 0;
    uint32_t pressed_at_ = 0;
};

} // namespace button_input
#endif
