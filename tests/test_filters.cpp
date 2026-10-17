#include <cstdint>
#include <cstdlib>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/filter_commands.h"
#include "ps/selection.h"
#include "state_checks.h"

using namespace ps;

namespace {

const Color kRed{255, 0, 0, 255};

// A white dot on black, for watching filters spread it.
Image dot() {
  Image image(21, 21, kBlack);
  image.set_pixel(10, 10, kWhite);
  return image;
}

// A red square on transparency, plus the test pattern, for goldens.
Image filter_source() {
  Image image(96, 64);
  image.blit(make_test_pattern(48, 48), {4, 8});
  image.fill({62, 16, 24, 32}, kRed);
  return image;
}

}  // namespace

TEST_CASE("Filters have names and halos", "[filter]") {
  CHECK(std::string(filter_name(GaussianBlur{})) == "Gaussian Blur");
  CHECK(std::string(filter_name(UnsharpMask{})) == "Unsharp Mask");
  CHECK(std::string(filter_name(AddNoise{})) == "Add Noise");
  CHECK(std::string(filter_name(MotionBlur{})) == "Motion Blur");
  CHECK(FilterCommand(0, MotionBlur{}).name() == "Motion Blur");
  CHECK(filter_halo(GaussianBlur{2.0f}) == 7);
  CHECK(filter_halo(AddNoise{}) == 0);
  CHECK(filter_halo(MotionBlur{0, 10}) == 6);
}

TEST_CASE("Gaussian blur", "[filter]") {
  const Image flat(15, 15, {90, 120, 30, 255});
  CHECK(apply_filter(GaussianBlur{3.0f}, flat, {0, 0}) == flat);
  CHECK(apply_filter(GaussianBlur{0.0f}, dot(), {0, 0}) == dot());

  const Image blurred = apply_filter(GaussianBlur{1.5f}, dot(), {0, 0});
  CHECK(blurred.pixel(10, 10).r < 255);
  CHECK(blurred.pixel(10, 10).r > blurred.pixel(11, 10).r);
  CHECK(blurred.pixel(11, 10).r > blurred.pixel(12, 10).r);
  CHECK(blurred.pixel(12, 10).r > 0);
  CHECK(blurred.pixel(18, 10).r == 0);
  // Round, so the same in every direction.
  CHECK(blurred.pixel(12, 10) == blurred.pixel(8, 10));
  CHECK(blurred.pixel(12, 10) == blurred.pixel(10, 12));
  CHECK(blurred.pixel(11, 11) == blurred.pixel(9, 9));

  SECTION("transparent neighbours soften alpha but keep colour pure") {
    Image square(30, 30);
    square.fill({10, 10, 10, 10}, kRed);
    const Image soft = apply_filter(GaussianBlur{2.0f}, square, {0, 0});
    const Color edge = soft.pixel(9, 15);
    CHECK(edge.a > 0);
    CHECK(edge.a < 255);
    CHECK(edge.r == 255);  // no darkening from the transparent black around
    CHECK(edge.g == 0);
    CHECK(soft.pixel(15, 15).a > soft.pixel(10, 15).a);
  }
}

TEST_CASE("Unsharp mask", "[filter]") {
  const Image flat(15, 15, {90, 120, 30, 255});
  CHECK(apply_filter(UnsharpMask{1.0f, 2.0f, 0}, flat, {0, 0}) == flat);

  // A step from dark to light grey gets darker and lighter at the edge.
  Image step(20, 10, {80, 80, 80, 255});
  step.fill({10, 0, 10, 10}, {160, 160, 160, 255});
  const Image sharp = apply_filter(UnsharpMask{1.0f, 1.5f, 0}, step, {0, 0});
  CHECK(sharp.pixel(9, 5).r < 80);
  CHECK(sharp.pixel(10, 5).r > 160);
  CHECK(sharp.pixel(2, 5).r == 80);  // far from the edge: unchanged
  CHECK(sharp.pixel(17, 5).r == 160);

  CHECK(apply_filter(UnsharpMask{0.0f, 1.5f, 0}, step, {0, 0}) == step);
  CHECK(apply_filter(UnsharpMask{1.0f, 1.5f, 255}, step, {0, 0}) == step);
}

TEST_CASE("Add noise", "[filter]") {
  const Image grey(40, 30, {128, 128, 128, 255});
  const AddNoise noise{20, false, 7};
  const Image a = apply_filter(noise, grey, {0, 0});
  CHECK(a == apply_filter(noise, grey, {0, 0}));  // repeatable
  CHECK(a != apply_filter(AddNoise{20, false, 8}, grey, {0, 0}));
  CHECK(apply_filter(AddNoise{0, false, 7}, grey, {0, 0}) == grey);

  long total = 0;
  bool moved = false;
  for (int y = 0; y < 30; ++y) {
    for (int x = 0; x < 40; ++x) {
      const Color c = a.pixel(x, y);
      CHECK(std::abs(c.r - 128) <= 20);
      CHECK(std::abs(c.b - 128) <= 20);
      CHECK(c.a == 255);
      moved |= c.r != 128;
      total += c.r;
    }
  }
  CHECK(moved);
  CHECK(std::abs(total / 1200.0 - 128) < 2.0);  // centred on the original

  SECTION("monochrome moves all channels together") {
    const Image mono = apply_filter(AddNoise{20, true, 7}, grey, {0, 0});
    const Color c = mono.pixel(13, 17);
    CHECK(c.r == c.g);
    CHECK(c.g == c.b);
  }
  SECTION("the noise belongs to the document position, not the image") {
    const Image part = apply_filter(noise, Image(10, 10, {128, 128, 128, 255}), {15, 12});
    CHECK(part.pixel(0, 0) == a.pixel(15, 12));
    CHECK(part.pixel(9, 9) == a.pixel(24, 21));
  }
  SECTION("transparent pixels stay transparent") {
    const Image clear(5, 5);
    CHECK(apply_filter(noise, clear, {0, 0}) == clear);
  }
}

TEST_CASE("Motion blur", "[filter]") {
  CHECK(apply_filter(MotionBlur{30.0f, 1}, dot(), {0, 0}) == dot());

  const Image across = apply_filter(MotionBlur{0.0f, 5}, dot(), {0, 0});
  CHECK(across.pixel(8, 10) == across.pixel(12, 10));
  CHECK(across.pixel(12, 10).r == 51);  // one fifth of the white
  CHECK(across.pixel(13, 10).r == 0);
  CHECK(across.pixel(10, 9).r == 0);    // nothing vertically

  const Image down = apply_filter(MotionBlur{90.0f, 5}, dot(), {0, 0});
  CHECK(down.pixel(10, 12).r == 51);
  CHECK(down.pixel(12, 10).r == 0);

  const Image diagonal = apply_filter(MotionBlur{45.0f, 5}, dot(), {0, 0});
  CHECK(diagonal.pixel(11, 9).r > 0);   // up and to the right
  CHECK(diagonal.pixel(9, 11).r > 0);
  CHECK(diagonal.pixel(11, 11).r == 0);
}

TEST_CASE("FilterCommand", "[filter]") {
  Document doc(80, 80);
  Layer square;
  square.pixels.fill({20, 20, 20, 20}, kRed);
  doc.add_layer(1, square);

  SECTION("a blur spreads the layer beyond what it stored") {
    Layer edge;
    edge.pixels.fill({54, 10, 10, 10}, kRed);  // ends at x = 63, a tile edge
    doc.add_layer(2, edge);
    FilterCommand(2, GaussianBlur{2.0f}).execute(doc);
    CHECK(doc.layers().at(2).pixels.pixel(65, 15).a > 0);
  }
  SECTION("only the selection changes, but it reads its neighbours") {
    doc.set_selection(rect_mask({40, 0, 40, 80}));
    FilterCommand(1, GaussianBlur{2.0f}).execute(doc);
    CHECK(doc.layers().at(1).pixels.pixel(41, 30).a > 0);   // blurred into
    CHECK(doc.layers().at(1).pixels.pixel(38, 30) == kRed); // left alone
    CHECK(doc.layers().at(1).pixels.pixel(19, 30).a == 0);
  }
  SECTION("a moved layer is filtered where it is shown") {
    doc.set_layer_offset(1, {30, 0});
    FilterCommand(1, MotionBlur{0.0f, 9}).execute(doc);
    CHECK(doc.flatten().pixel(48, 30) != kWhite);  // smeared left of x = 50
    CHECK(doc.flatten().pixel(45, 30) == kWhite);
  }
  SECTION("pixels beyond the canvas are kept as they were") {
    Layer wide;
    wide.pixels.fill({-20, 60, 40, 10}, kRed);
    doc.add_layer(2, wide);
    FilterCommand(2, GaussianBlur{2.0f}).execute(doc);
    CHECK(doc.layers().at(2).pixels.pixel(-10, 65) == kRed);
  }
  SECTION("an empty layer is left empty") {
    doc.add_layer(2, Layer{});
    FilterCommand(2, GaussianBlur{3.0f}).execute(doc);
    CHECK(doc.layers().at(2).pixels.empty());
  }
  CHECK(cache_is_current(doc));
}

TEST_CASE("Filters match their golden images", "[filter][golden]") {
  const struct {
    const char* name;
    Filter filter;
  } cases[] = {
      {"filter_gaussian_blur", GaussianBlur{2.5f}},
      {"filter_unsharp_mask", UnsharpMask{1.5f, 2.0f, 4}},
      {"filter_add_noise", AddNoise{40, false, 3}},
      {"filter_motion_blur", MotionBlur{30.0f, 12}},
  };
  for (const auto& c : cases) {
    CAPTURE(c.name);
    Document doc(96, 64, {40, 40, 40, 255});
    Layer layer;
    layer.pixels.write(filter_source(), {0, 0});
    doc.add_layer(1, layer);
    FilterCommand(1, c.filter).execute(doc);
    CHECK(golden_mismatch(doc.flatten(), c.name) == "");
  }
}
