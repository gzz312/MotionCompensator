#pragma once
#include <algorithm>

namespace mc::workbench {

enum class Page { Overview, Visuals, Games, Diagnostics, General };
enum class Section { Shared, Overview, VisualHeader, Dots, Reticle, Motion, Games, Diagnostics, General };

inline constexpr int width = 1280;
inline constexpr int preferredHeight = 736;
inline constexpr int compactHeight = 680;
inline constexpr int visualLeft = 160;
inline constexpr int visualRise = 112;
inline constexpr int paneTop = 312 - visualRise;

inline bool scrollable(Section section) {
    return section == Section::Dots || section == Section::Reticle || section == Section::Motion;
}

inline bool visible(Section section, Page page, bool reticle, bool dynamic) {
    switch (section) {
        case Section::Shared: return true;
        case Section::Overview: return page == Page::Overview;
        case Section::VisualHeader: return page == Page::Visuals;
        case Section::Dots: return page == Page::Visuals && !reticle;
        case Section::Motion: return page == Page::Visuals && !reticle && dynamic;
        case Section::Reticle: return page == Page::Visuals && reticle;
        case Section::Games: return page == Page::Games;
        case Section::Diagnostics: return page == Page::Diagnostics;
        case Section::General: return page == Page::General;
    }
    return false;
}

struct Layout {
    float scale;
    int x;
    int y;
    int height;
};

inline Layout fit(int clientWidth, int clientHeight, int caption, int dpi) {
    const int content = std::max(1, clientHeight - caption);
    const float scale = std::min({std::max(1, dpi) / 96.f, clientWidth / float(width),
                                  content / float(compactHeight)});
    const int height = std::clamp(int(content / scale), compactHeight, preferredHeight);
    return {scale, int((clientWidth - width * scale) / 2),
            caption + int((content - height * scale) / 2), height};
}

}
