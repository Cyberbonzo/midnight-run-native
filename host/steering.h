#pragma once

#include <algorithm>
#include <cmath>

class CabinetSteering {
    double position_ = 0.0;

public:
    // Called once per 60 Hz cabinet tick. The serial ADC uses 128 for straight ahead.
    int update(int raw) {
        double axis = raw / (raw < 0 ? 32768.0 : 32767.0);
        constexpr double deadzone = 0.05;
        double magnitude = std::clamp((std::abs(axis) - deadzone) / (1.0 - deadzone), 0.0, 1.0);
        double target = std::copysign(magnitude * (0.40 + 0.60 * magnitude * magnitude), axis);
        // Return to centre quickly when the driver releases or countersteers.
        bool returning = target * position_ < 0 || std::abs(target) < std::abs(position_);
        double max_step = returning ? 0.32 : 0.19;
        position_ += std::clamp(target - position_, -max_step, max_step);
        return static_cast<int>(std::lround(128 + position_ * (position_ < 0 ? 128 : 127)));
    }
};
