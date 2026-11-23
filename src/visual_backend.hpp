#pragma once
#include "dots.hpp"

namespace mc {

// Legacy compensation vectors, after the user's gain and direction settings.
// Preserve the existing source-to-overlay behavior until field mapping is introduced.
struct RegionalVisualMotion {
    std::array<Vec2, 9> velocity{};
    float elapsedSeconds{};
    bool dynamic{};
};

// Owned by the overlay thread. The returned view lasts until the next update
// or reset. Capture and analysis workers never access this state.
class LegacyVisualBackend {
    RegionalDotField regional_;
    EdgeDotField edge_;
    std::vector<DotVisual> fixed_;
    int style_ = 0;

public:
    bool selectStyle(int style) {
        if (style_ == style) return false;
        reset();
        style_ = style;
        return true;
    }

    void reset() {
        regional_.reset();
        edge_.reset();
        fixed_.clear();
    }

    const std::vector<DotVisual>& update(int width, int height,
                                         const VisualSettings& settings,
                                         const RegionalVisualMotion& motion) {
        if (style_ == 2) {
            fixed_ = staticDots(width, height, settings);
            return fixed_;
        }
        if (style_ == 1) {
            return edge_.update(motion.velocity, width, height, settings, motion.elapsedSeconds);
        }
        return regional_.update(motion.velocity, width, height, settings.spacing,
                                motion.elapsedSeconds, motion.dynamic, settings.enhancement,
                                settings.boundaryFade, settings.radius, settings.staggered);
    }

    size_t dotCount() const {
        if (style_ == 2) return 16;
        return style_ == 1 ? edge_.dots().size() : regional_.dots().size();
    }

    float phaseMax() const {
        return style_ == 1 ? edge_.phaseMax() : regional_.phaseMax();
    }
};

}
