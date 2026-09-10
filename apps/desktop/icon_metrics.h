#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace openbrowser::desktop {

// Openbrowser owns the complete icon vocabulary. The enum is deliberately
// independent from CEF so the geometry contract can be tested by Core CI.
enum class IconId : std::uint8_t {
    AppMark,
    Back,
    Forward,
    Reload,
    Security,
    SecurityWarning,
    InternalPage,
    Bookmark,
    Download,
    Profile,
    PrivateProfile,
    More,
    Sidebar,
    SidebarCollapse,
    Focus,
    Library,
    Settings,
    Commands,
    Network,
    WorkspaceHome,
    WorkspaceWork,
    WorkspacePersonal,
    WorkspaceGeneric,
    Close,
    Add,
    Previous,
    Next,
    Pause,
    Play,
    Clear,
    Search,
    Count,
};

inline constexpr std::size_t kIconCount = static_cast<std::size_t>(IconId::Count);

// All glyphs share a 20 DIP square. The glyph itself is optically kept inside
// that square; button hit targets are intentionally larger than the artwork.
inline constexpr int kIconCanvasDip = 20;
inline constexpr float kToolbarGlyphDip = 18.0F;
inline constexpr float kCompactGlyphDip = 16.0F;
inline constexpr int kIconHitTargetDip = 32;
inline constexpr float kDefaultIconStrokeDip = 1.75F;

struct IconRasterVariant {
    float scale_factor;
    int pixel_size;
};

// CEF selects the closest representation for the device scale factor. Keeping
// every representation square avoids a layout-driven non-uniform stretch.
inline constexpr std::array<IconRasterVariant, 4> kIconRasterVariants{{
    {1.0F, 20},
    {1.25F, 25},
    {1.5F, 30},
    {2.0F, 40},
}};

struct IconBounds {
    float left;
    float top;
    float right;
    float bottom;
};

struct IconGeometrySpec {
    IconBounds bounds;
    float stroke_dip;
    float optical_offset_x;
    float optical_offset_y;
};

inline constexpr std::array<IconId, kIconCount> kAllIconIds{{
    IconId::AppMark,
    IconId::Back,
    IconId::Forward,
    IconId::Reload,
    IconId::Security,
    IconId::SecurityWarning,
    IconId::InternalPage,
    IconId::Bookmark,
    IconId::Download,
    IconId::Profile,
    IconId::PrivateProfile,
    IconId::More,
    IconId::Sidebar,
    IconId::SidebarCollapse,
    IconId::Focus,
    IconId::Library,
    IconId::Settings,
    IconId::Commands,
    IconId::Network,
    IconId::WorkspaceHome,
    IconId::WorkspaceWork,
    IconId::WorkspacePersonal,
    IconId::WorkspaceGeneric,
    IconId::Close,
    IconId::Add,
    IconId::Previous,
    IconId::Next,
    IconId::Pause,
    IconId::Play,
    IconId::Clear,
    IconId::Search,
}};

inline constexpr std::array<IconGeometrySpec, kIconCount> kIconGeometry{{
    {{{3.0F, 3.0F, 17.0F, 17.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{6.0F, 3.0F, 18.0F, 17.0F}, kDefaultIconStrokeDip, 0.35F, 0.0F}},
    {{{2.0F, 3.0F, 14.0F, 17.0F}, kDefaultIconStrokeDip, -0.35F, 0.0F}},
    {{{3.0F, 3.0F, 18.0F, 17.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{3.0F, 1.0F, 17.0F, 19.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{1.0F, 1.0F, 19.0F, 18.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{2.0F, 2.0F, 18.0F, 18.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{4.0F, 2.0F, 16.0F, 18.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{3.0F, 2.0F, 17.0F, 18.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{3.0F, 3.0F, 17.0F, 18.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{3.0F, 5.0F, 17.0F, 18.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{3.0F, 8.0F, 17.0F, 12.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{2.0F, 2.0F, 18.0F, 18.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{2.0F, 2.0F, 18.0F, 18.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{0.0F, 0.0F, 20.0F, 20.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{2.0F, 3.0F, 18.0F, 17.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{0.0F, 0.0F, 20.0F, 20.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{3.0F, 4.0F, 18.0F, 16.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{2.0F, 3.0F, 18.0F, 17.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{2.0F, 2.0F, 18.0F, 18.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{2.0F, 3.0F, 18.0F, 17.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{4.0F, 3.0F, 16.0F, 17.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{3.0F, 3.0F, 17.0F, 17.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{4.0F, 4.0F, 16.0F, 16.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{3.0F, 3.0F, 17.0F, 17.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{6.0F, 3.0F, 14.0F, 17.0F}, kDefaultIconStrokeDip, 0.35F, 0.0F}},
    {{{6.0F, 3.0F, 14.0F, 17.0F}, kDefaultIconStrokeDip, -0.35F, 0.0F}},
    {{{4.0F, 3.0F, 16.0F, 17.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{6.0F, 3.0F, 17.0F, 17.0F}, kDefaultIconStrokeDip, -0.25F, 0.0F}},
    {{{4.0F, 2.0F, 16.0F, 18.0F}, kDefaultIconStrokeDip, 0.0F, 0.0F}},
    {{{2.0F, 2.0F, 18.0F, 18.0F}, kDefaultIconStrokeDip, -0.15F, -0.15F}},
}};

[[nodiscard]] constexpr bool IsValidIconId(const IconId id) noexcept {
    return static_cast<std::size_t>(id) < kIconCount;
}

[[nodiscard]] constexpr IconGeometrySpec IconGeometryFor(const IconId id) noexcept {
    return IsValidIconId(id) ? kIconGeometry[static_cast<std::size_t>(id)]
                             : IconGeometrySpec{{0.0F, 0.0F, 0.0F, 0.0F}, 0.0F, 0.0F, 0.0F};
}

}  // namespace openbrowser::desktop
