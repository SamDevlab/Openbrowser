#include "icon_system.h"

#include "aura_design_tokens.h"

#include "include/views/cef_label_button.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

namespace openbrowser::desktop {
namespace {

constexpr int kIconDip = 20;
constexpr float kStroke = 1.8F;

struct Point {
    float x;
    float y;
};

struct Pixel {
    std::uint8_t blue{0};
    std::uint8_t green{0};
    std::uint8_t red{0};
    std::uint8_t alpha{0};
};

class Raster final {
public:
    explicit Raster(const int scale)
        : scale_(scale), pixels_(static_cast<std::size_t>(kIconDip * scale) *
                                 static_cast<std::size_t>(kIconDip * scale)) {}

    [[nodiscard]] int Size() const noexcept { return kIconDip * scale_; }
    [[nodiscard]] const std::vector<Pixel>& Pixels() const noexcept { return pixels_; }

    void FillCircle(const Point center, const float radius) {
        for (int y = 0; y < Size(); ++y) {
            for (int x = 0; x < Size(); ++x) {
                const float dx = static_cast<float>(x) / scale_ + 0.5F / scale_ - center.x;
                const float dy = static_cast<float>(y) / scale_ + 0.5F / scale_ - center.y;
                if (dx * dx + dy * dy <= radius * radius) {
                    Put(x, y);
                }
            }
        }
    }

    void Circle(const Point center, const float radius, const float thickness = kStroke) {
        for (int y = 0; y < Size(); ++y) {
            for (int x = 0; x < Size(); ++x) {
                const float dx = static_cast<float>(x) / scale_ + 0.5F / scale_ - center.x;
                const float dy = static_cast<float>(y) / scale_ + 0.5F / scale_ - center.y;
                const float distance = std::sqrt(dx * dx + dy * dy);
                if (std::abs(distance - radius) <= thickness / 2.0F) {
                    Put(x, y);
                }
            }
        }
    }

    void Line(const Point start, const Point end, const float thickness = kStroke) {
        const float min_x = std::max(0.0F, std::min(start.x, end.x) - thickness);
        const float max_x = std::min(static_cast<float>(kIconDip), std::max(start.x, end.x) + thickness);
        const float min_y = std::max(0.0F, std::min(start.y, end.y) - thickness);
        const float max_y = std::min(static_cast<float>(kIconDip), std::max(start.y, end.y) + thickness);
        const float dx = end.x - start.x;
        const float dy = end.y - start.y;
        const float length_squared = dx * dx + dy * dy;

        for (int y = static_cast<int>(min_y * scale_); y <= static_cast<int>(max_y * scale_); ++y) {
            for (int x = static_cast<int>(min_x * scale_); x <= static_cast<int>(max_x * scale_); ++x) {
                const float px = static_cast<float>(x) / scale_ + 0.5F / scale_;
                const float py = static_cast<float>(y) / scale_ + 0.5F / scale_;
                const float projection = length_squared > 0.0F
                    ? std::clamp(((px - start.x) * dx + (py - start.y) * dy) / length_squared, 0.0F, 1.0F)
                    : 0.0F;
                const float nearest_x = start.x + projection * dx;
                const float nearest_y = start.y + projection * dy;
                const float distance_x = px - nearest_x;
                const float distance_y = py - nearest_y;
                if (distance_x * distance_x + distance_y * distance_y <=
                    (thickness * thickness) / 4.0F) {
                    Put(x, y);
                }
            }
        }
    }

    void Polyline(const std::initializer_list<Point> points, const bool close = false) {
        if (points.size() < 2) {
            return;
        }
        auto previous = points.begin();
        for (auto current = previous + 1; current != points.end(); ++current) {
            Line(*previous, *current);
            previous = current;
        }
        if (close) {
            Line(*previous, *points.begin());
        }
    }

    void Rect(const Point top_left, const Point bottom_right, const float thickness = kStroke) {
        Line(top_left, {bottom_right.x, top_left.y}, thickness);
        Line({bottom_right.x, top_left.y}, bottom_right, thickness);
        Line(bottom_right, {top_left.x, bottom_right.y}, thickness);
        Line({top_left.x, bottom_right.y}, top_left, thickness);
    }

private:
    void Put(const int x, const int y) {
        if (x < 0 || y < 0 || x >= Size() || y >= Size()) {
            return;
        }
        auto& pixel = pixels_[static_cast<std::size_t>(y * Size() + x)];
        pixel.blue = 0xF0;
        pixel.green = 0xE8;
        pixel.red = 0xE1;
        pixel.alpha = 0xFF;
    }

    int scale_;
    std::vector<Pixel> pixels_;
};

void DrawIcon(Raster& raster, const IconId id) {
    switch (id) {
        case IconId::AppMark:
            raster.Polyline({{4, 15}, {10, 4}, {16, 15}}, true);
            raster.Line({7, 11}, {13, 11});
            raster.Line({8, 15}, {12, 15});
            break;
        case IconId::Back:
            raster.Line({15, 4}, {7, 10});
            raster.Line({7, 10}, {15, 16});
            raster.Line({7, 10}, {17, 10});
            break;
        case IconId::Forward:
            raster.Line({5, 4}, {13, 10});
            raster.Line({13, 10}, {5, 16});
            raster.Line({3, 10}, {13, 10});
            break;
        case IconId::Reload:
            raster.Circle({10, 10}, 6.0F);
            raster.Line({14, 4}, {17, 4});
            raster.Line({17, 4}, {17, 7});
            break;
        case IconId::Security:
            raster.Polyline({{10, 2}, {16, 5}, {15, 11}, {10, 18}, {5, 11}, {4, 5}, {10, 2}}, false);
            raster.Line({7, 10}, {9, 12});
            raster.Line({9, 12}, {13, 8});
            break;
        case IconId::SecurityWarning:
            raster.Polyline({{10, 2}, {18, 17}, {2, 17}, {10, 2}}, true);
            raster.Line({10, 7}, {10, 12});
            raster.FillCircle({10, 14.5F}, 0.8F);
            break;
        case IconId::InternalPage:
            raster.Rect({3, 3}, {17, 17});
            raster.Line({6, 10}, {14, 10});
            raster.Line({6, 13}, {12, 13});
            break;
        case IconId::Bookmark:
            raster.Polyline({{5, 3}, {15, 3}, {15, 17}, {10, 14}, {5, 17}, {5, 3}}, true);
            break;
        case IconId::Download:
            raster.Line({10, 3}, {10, 13});
            raster.Line({6, 10}, {10, 14});
            raster.Line({10, 14}, {14, 10});
            raster.Line({4, 17}, {16, 17});
            break;
        case IconId::Profile:
            raster.Circle({10, 6.5F}, 2.8F);
            raster.Polyline({{4, 17}, {5, 14}, {8, 12}, {12, 12}, {15, 14}, {16, 17}}, false);
            break;
        case IconId::PrivateProfile:
            raster.Circle({10, 7}, 2.5F);
            raster.Line({4, 13}, {16, 13});
            raster.Line({5, 13}, {7, 17});
            raster.Line({15, 13}, {13, 17});
            raster.Line({7, 17}, {13, 17});
            break;
        case IconId::More:
            raster.FillCircle({5, 10}, 1.2F);
            raster.FillCircle({10, 10}, 1.2F);
            raster.FillCircle({15, 10}, 1.2F);
            break;
        case IconId::Sidebar:
            raster.Rect({3, 3}, {17, 17});
            raster.Line({7, 3}, {7, 17});
            break;
        case IconId::SidebarCollapse:
            raster.Rect({3, 3}, {17, 17});
            raster.Line({7, 3}, {7, 17});
            raster.Line({12, 7}, {9, 10});
            raster.Line({9, 10}, {12, 13});
            break;
        case IconId::Focus:
            raster.Circle({10, 10}, 6.0F);
            raster.Circle({10, 10}, 2.0F);
            raster.Line({10, 1.5F}, {10, 4});
            raster.Line({10, 16}, {10, 18.5F});
            raster.Line({1.5F, 10}, {4, 10});
            raster.Line({16, 10}, {18.5F, 10});
            break;
        case IconId::Library:
            raster.Rect({3, 4}, {17, 16});
            raster.Line({6, 4}, {6, 16});
            raster.Line({10, 4}, {10, 16});
            raster.Line({13, 7}, {15, 7});
            raster.Line({13, 10}, {15, 10});
            raster.Line({13, 13}, {15, 13});
            break;
        case IconId::Settings:
            raster.Circle({10, 10}, 5.5F);
            raster.Circle({10, 10}, 2.0F);
            raster.Line({10, 1.5F}, {10, 4});
            raster.Line({10, 16}, {10, 18.5F});
            raster.Line({1.5F, 10}, {4, 10});
            raster.Line({16, 10}, {18.5F, 10});
            raster.Line({4, 4}, {5.8F, 5.8F});
            raster.Line({14.2F, 14.2F}, {16, 16});
            raster.Line({14.2F, 5.8F}, {16, 4});
            raster.Line({4, 16}, {5.8F, 14.2F});
            break;
        case IconId::Commands:
            raster.Line({4, 5}, {9, 10});
            raster.Line({9, 10}, {4, 15});
            raster.Line({11, 15}, {17, 15});
            break;
        case IconId::Network:
            raster.Line({5, 6}, {15, 5});
            raster.Line({5, 6}, {10, 15});
            raster.Line({15, 5}, {10, 15});
            raster.FillCircle({5, 6}, 2.0F);
            raster.FillCircle({15, 5}, 2.0F);
            raster.FillCircle({10, 15}, 2.0F);
            break;
        case IconId::WorkspaceHome:
            raster.Polyline({{3, 9}, {10, 3}, {17, 9}}, false);
            raster.Rect({5, 9}, {15, 17});
            raster.Rect({9, 12}, {11, 17});
            break;
        case IconId::WorkspaceWork:
            raster.Rect({3, 7}, {17, 16});
            raster.Rect({7, 4}, {13, 7});
            raster.Line({3, 10}, {17, 10});
            break;
        case IconId::WorkspacePersonal:
            raster.Circle({10, 6}, 2.5F);
            raster.Polyline({{5, 16}, {6, 12}, {10, 10}, {14, 12}, {15, 16}}, false);
            break;
        case IconId::WorkspaceGeneric:
            raster.Rect({4, 4}, {8, 8});
            raster.Rect({12, 4}, {16, 8});
            raster.Rect({4, 12}, {8, 16});
            raster.Rect({12, 12}, {16, 16});
            break;
        case IconId::Close:
            raster.Line({5, 5}, {15, 15});
            raster.Line({15, 5}, {5, 15});
            break;
        case IconId::Add:
            raster.Line({10, 4}, {10, 16});
            raster.Line({4, 10}, {16, 10});
            break;
        case IconId::Previous:
            raster.Line({13, 4}, {7, 10});
            raster.Line({7, 10}, {13, 16});
            break;
        case IconId::Next:
            raster.Line({7, 4}, {13, 10});
            raster.Line({13, 10}, {7, 16});
            break;
        case IconId::Pause:
            raster.Rect({5, 4}, {8, 16});
            raster.Rect({12, 4}, {15, 16});
            break;
        case IconId::Play:
            raster.Polyline({{7, 4}, {16, 10}, {7, 16}, {7, 4}}, false);
            break;
        case IconId::Clear:
            raster.Line({5, 6}, {15, 6});
            raster.Line({7, 6}, {8, 16});
            raster.Line({13, 6}, {12, 16});
            raster.Line({8, 16}, {12, 16});
            raster.Line({8, 3}, {12, 3});
            break;
        case IconId::Search:
            raster.Circle({8.5F, 8.5F}, 5.0F);
            raster.Line({12, 12}, {16.5F, 16.5F});
            break;
        case IconId::Count:
            break;
    }
}

using IconCache = std::array<
    CefRefPtr<CefImage>, static_cast<std::size_t>(IconId::Count)>;

IconCache& Cache() {
    static auto* cache = new IconCache();
    return *cache;
}

[[nodiscard]] std::size_t CacheIndex(const IconId id) {
    return static_cast<std::size_t>(id);
}

CefRefPtr<CefImage> BuildIcon(const IconId id) {
    auto image = CefImage::CreateImage();
    for (const int scale : {1, 2}) {
        Raster raster(scale);
        DrawIcon(raster, id);
        image->AddBitmap(
            static_cast<float>(scale),
            raster.Size(),
            raster.Size(),
            CEF_COLOR_TYPE_BGRA_8888,
            CEF_ALPHA_TYPE_PREMULTIPLIED,
            raster.Pixels().data(),
            raster.Pixels().size() * sizeof(Pixel));
    }
    return image;
}

}  // namespace

CefRefPtr<CefImage> GetIcon(const IconId id) {
    auto& cache = Cache();
    auto& image = cache[CacheIndex(id)];
    if (!image) {
        image = BuildIcon(id);
    }
    return image;
}

void SetIcon(const CefRefPtr<CefLabelButton>& button, const IconId id) {
    if (!button) {
        return;
    }
    const auto image = GetIcon(id);
    button->SetImage(CEF_BUTTON_STATE_NORMAL, image);
    button->SetImage(CEF_BUTTON_STATE_HOVERED, image);
    button->SetImage(CEF_BUTTON_STATE_PRESSED, image);
}

void ConfigureIconButton(
    const CefRefPtr<CefLabelButton>& button,
    const IconId id,
    const std::string_view text,
    const std::string_view accessible_name,
    const std::string_view tooltip,
    const int group_id) {
    if (!button) {
        return;
    }

    SetIcon(button, id);
    button->SetText(std::string(text));
    button->SetAccessibleName(std::string(accessible_name));
    button->SetTooltipText(std::string(tooltip));
    button->SetFocusable(true);
    button->SetMinimumSize(CefSize(
        text.empty() ? aura::kIconControlSize : 104,
        aura::kControlHeight));
    if (group_id != 0) {
        button->SetGroupID(group_id);
    }
}

}  // namespace openbrowser::desktop
