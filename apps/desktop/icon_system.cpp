#include "icon_system.h"

#include "include/views/cef_label_button.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace openbrowser::desktop {
namespace {

constexpr int kSamplesPerAxis = 4;
constexpr int kSamplesPerPixel = kSamplesPerAxis * kSamplesPerAxis;

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

enum class PrimitiveKind {
    Segment,
    Ring,
    Disc,
};

struct Primitive {
    PrimitiveKind kind;
    Point first;
    Point second;
    float radius;
    float thickness;
};

class Raster final {
public:
    Raster(const IconRasterVariant variant, const IconGeometrySpec geometry)
        : scale_(variant.scale_factor),
          size_(variant.pixel_size),
          geometry_(geometry),
          pixels_(static_cast<std::size_t>(size_) * static_cast<std::size_t>(size_)) {}

    [[nodiscard]] int Size() const noexcept { return size_; }
    [[nodiscard]] const std::vector<Pixel>& Pixels() const noexcept { return pixels_; }

    void FillCircle(const Point center, const float radius) {
        primitives_.push_back({PrimitiveKind::Disc, Offset(center), {}, radius, 0.0F});
    }

    void Circle(const Point center, const float radius, const float thickness = 0.0F) {
        primitives_.push_back({
            PrimitiveKind::Ring,
            Offset(center),
            {},
            radius,
            thickness > 0.0F ? thickness : geometry_.stroke_dip,
        });
    }

    void Line(const Point start, const Point end, const float thickness = 0.0F) {
        primitives_.push_back({
            PrimitiveKind::Segment,
            Offset(start),
            Offset(end),
            0.0F,
            thickness > 0.0F ? thickness : geometry_.stroke_dip,
        });
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

    void Rect(const Point top_left, const Point bottom_right, const float thickness = 0.0F) {
        Line(top_left, {bottom_right.x, top_left.y}, thickness);
        Line({bottom_right.x, top_left.y}, bottom_right, thickness);
        Line(bottom_right, {top_left.x, bottom_right.y}, thickness);
        Line({top_left.x, bottom_right.y}, top_left, thickness);
    }

    void Render() {
        for (int y = 0; y < size_; ++y) {
            for (int x = 0; x < size_; ++x) {
                int covered_samples = 0;
                for (int sample_y = 0; sample_y < kSamplesPerAxis; ++sample_y) {
                    const float dip_y =
                        (static_cast<float>(y) + (static_cast<float>(sample_y) + 0.5F) /
                                                     static_cast<float>(kSamplesPerAxis)) /
                        scale_;
                    for (int sample_x = 0; sample_x < kSamplesPerAxis; ++sample_x) {
                        const float dip_x =
                            (static_cast<float>(x) + (static_cast<float>(sample_x) + 0.5F) /
                                                         static_cast<float>(kSamplesPerAxis)) /
                            scale_;
                        if (Contains({dip_x, dip_y})) {
                            ++covered_samples;
                        }
                    }
                }
                Put(x, y, covered_samples);
            }
        }
    }

private:
    [[nodiscard]] Point Offset(const Point point) const noexcept {
        return {
            point.x + geometry_.optical_offset_x,
            point.y + geometry_.optical_offset_y,
        };
    }

    [[nodiscard]] static float DistanceSquared(const Point point, const Point start, const Point end) {
        const float dx = end.x - start.x;
        const float dy = end.y - start.y;
        const float length_squared = dx * dx + dy * dy;
        const float projection = length_squared > 0.0F
            ? std::clamp(
                  ((point.x - start.x) * dx + (point.y - start.y) * dy) / length_squared,
                  0.0F,
                  1.0F)
            : 0.0F;
        const float nearest_x = start.x + projection * dx;
        const float nearest_y = start.y + projection * dy;
        const float distance_x = point.x - nearest_x;
        const float distance_y = point.y - nearest_y;
        return distance_x * distance_x + distance_y * distance_y;
    }

    [[nodiscard]] bool Contains(const Point point) const {
        for (const auto& primitive : primitives_) {
            switch (primitive.kind) {
                case PrimitiveKind::Segment:
                    if (DistanceSquared(point, primitive.first, primitive.second) <=
                        (primitive.thickness * primitive.thickness) / 4.0F) {
                        return true;
                    }
                    break;
                case PrimitiveKind::Ring: {
                    const float dx = point.x - primitive.first.x;
                    const float dy = point.y - primitive.first.y;
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    if (std::abs(distance - primitive.radius) <= primitive.thickness / 2.0F) {
                        return true;
                    }
                    break;
                }
                case PrimitiveKind::Disc: {
                    const float dx = point.x - primitive.first.x;
                    const float dy = point.y - primitive.first.y;
                    if (dx * dx + dy * dy <= primitive.radius * primitive.radius) {
                        return true;
                    }
                    break;
                }
            }
        }
        return false;
    }

    void Put(const int x, const int y, const int covered_samples) {
        if (covered_samples <= 0 || x < 0 || y < 0 || x >= size_ || y >= size_) {
            return;
        }

        const int alpha = (covered_samples * 255 + kSamplesPerPixel / 2) / kSamplesPerPixel;
        auto& pixel = pixels_[static_cast<std::size_t>(y * size_ + x)];

        // CEF_ALPHA_TYPE_PREMULTIPLIED requires transparent RGB to stay zero.
        // This avoids dark fringes when CEF composites the antialiased edge.
        pixel.blue = static_cast<std::uint8_t>((0xE1 * alpha + 127) / 255);
        pixel.green = static_cast<std::uint8_t>((0xE8 * alpha + 127) / 255);
        pixel.red = static_cast<std::uint8_t>((0xF0 * alpha + 127) / 255);
        pixel.alpha = static_cast<std::uint8_t>(alpha);
    }

    float scale_;
    int size_;
    IconGeometrySpec geometry_;
    std::vector<Primitive> primitives_;
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
            raster.Polyline({{10, 2}, {16, 5}, {15, 11}, {10, 18}, {5, 11}, {4, 5}, {10, 2}});
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
            raster.Polyline({{4, 17}, {5, 14}, {8, 12}, {12, 12}, {15, 14}, {16, 17}});
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
            raster.Polyline({{3, 9}, {10, 3}, {17, 9}});
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
            raster.Polyline({{5, 16}, {6, 12}, {10, 10}, {14, 12}, {15, 16}});
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
            raster.Polyline({{7, 4}, {16, 10}, {7, 16}, {7, 4}});
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

using IconCache = std::array<CefRefPtr<CefImage>, kIconCount>;

IconCache& Cache() {
    // Intentionally leaked until process exit. CEF images must not outlive the
    // CEF UI thread during normal shutdown, and this avoids static teardown
    // ordering across CEF's global shutdown sequence.
    static auto* cache = new IconCache();
    return *cache;
}

CefRefPtr<CefImage> BuildIcon(const IconId id) {
    auto image = CefImage::CreateImage();
    const auto geometry = IconGeometryFor(id);
    for (const auto variant : kIconRasterVariants) {
        Raster raster(variant, geometry);
        DrawIcon(raster, id);
        raster.Render();
        image->AddBitmap(
            variant.scale_factor,
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
    if (!IsValidIconId(id)) {
        return nullptr;
    }
    auto& cache = Cache();
    auto& image = cache[static_cast<std::size_t>(id)];
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
    button->SetImage(CEF_BUTTON_STATE_DISABLED, image);
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

    // The image is always a square 20 DIP canvas. The larger hit target is
    // shared by toolbar and compact-sidebar controls, while text buttons keep
    // their natural width and cannot distort the glyph canvas.
    button->SetMinimumSize(CefSize(kIconHitTargetDip, kIconHitTargetDip));
    if (group_id != 0) {
        button->SetGroupID(group_id);
    }
}

}  // namespace openbrowser::desktop
