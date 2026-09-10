#include "apps/desktop/icon_metrics.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Assertion failed: " << message << '\n';
        std::exit(1);
    }
}

void TestCanvasAndScaleContract() {
    using namespace openbrowser::desktop;

    Require(kIconCanvasDip == 20, "icons use the 20 DIP logical canvas");
    Require(kToolbarGlyphDip == 18.0F, "toolbar glyph size is explicit");
    Require(kCompactGlyphDip == 16.0F, "compact glyph size is explicit");
    Require(kIconHitTargetDip == 32, "icon hit target is explicit");
    Require(
        kIconRasterVariants.size() == static_cast<std::size_t>(4),
        "four DPI representations are declared");

    float previous_scale = 0.0F;
    for (const auto variant : kIconRasterVariants) {
        Require(variant.scale_factor > previous_scale, "scale factors are strictly increasing");
        Require(variant.pixel_size > 0, "raster dimensions are positive");
        Require(
            std::abs(static_cast<float>(variant.pixel_size) -
                     static_cast<float>(kIconCanvasDip) * variant.scale_factor) < 0.001F,
            "raster dimensions match the square DIP canvas");
        previous_scale = variant.scale_factor;
    }
}

void TestEveryIconHasValidGeometry() {
    using namespace openbrowser::desktop;

    for (std::size_t index = 0; index < kAllIconIds.size(); ++index) {
        const auto id = kAllIconIds[index];
        Require(IsValidIconId(id), "icon vocabulary contains only valid IDs");
        Require(static_cast<std::size_t>(id) == index, "icon vocabulary is contiguous");

        const auto geometry = IconGeometryFor(id);
        Require(geometry.stroke_dip > 0.0F, "every icon has a stroke width");
        Require(geometry.stroke_dip <= 2.25F, "stroke width stays within the family range");
        Require(geometry.bounds.left >= 0.0F, "icon bounds do not cross the left canvas edge");
        Require(geometry.bounds.top >= 0.0F, "icon bounds do not cross the top canvas edge");
        Require(geometry.bounds.right <= static_cast<float>(kIconCanvasDip), "right bound fits canvas");
        Require(geometry.bounds.bottom <= static_cast<float>(kIconCanvasDip), "bottom bound fits canvas");
        Require(geometry.bounds.right > geometry.bounds.left, "icon bounds have width");
        Require(geometry.bounds.bottom > geometry.bounds.top, "icon bounds have height");
        Require(std::abs(geometry.optical_offset_x) <= 0.5F, "horizontal optical correction is bounded");
        Require(std::abs(geometry.optical_offset_y) <= 0.5F, "vertical optical correction is bounded");
    }

    Require(!IsValidIconId(IconId::Count), "Count is not a drawable icon");
}

}  // namespace

int main() {
    TestCanvasAndScaleContract();
    TestEveryIconHasValidGeometry();
    std::cout << "All Openbrowser icon invariant tests passed successfully.\n";
    return 0;
}
