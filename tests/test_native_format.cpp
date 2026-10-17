#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "golden.h"
#include "ps/image_io.h"
#include "ps/native_format.h"
#include "ps/selection.h"
#include "state_checks.h"

using namespace ps;

namespace {

constexpr int kT = kTileSize;
const Color kRed{255, 0, 0, 255};

std::string temp_path(const std::string& name) {
  return std::string(PS_TEST_OUTPUT_DIR) + "/" + name;
}

// A document using everything the format has to carry.
Document busy() {
  Document doc(make_test_pattern(3 * kT + 10, 2 * kT + 5));
  Layer moved;
  moved.name = "Moved, with \"quotes\" and ünïcödé";
  moved.pixels.fill({-kT - 3, -5, 40, 30}, kRed);  // negative tiles
  moved.offset = {kT + 7, 20};
  moved.opacity = 0.37f;
  moved.blend = BlendMode::SoftLight;
  moved.mask = MaskGrid(255);
  moved.mask->fill({-kT, 0, 20, 10}, 30);
  doc.add_layer(1, moved);

  Layer hidden;
  hidden.name = "Hidden";
  hidden.pixels.fill({0, 0, 5000, 3}, {1, 2, 3, 4});  // far past the canvas
  hidden.visible = false;
  hidden.blend = BlendMode::ColorBurn;
  hidden.mask = MaskGrid(0);  // hide-all, no tiles
  doc.add_layer(2, hidden);

  Layer empty;
  empty.name = "";
  doc.add_layer(3, empty);
  return doc;
}

std::vector<std::uint8_t> read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST_CASE("Saving and loading gives back the same document", "[native]") {
  const Document original = busy();
  const Document loaded = decode_native(encode_native(original));
  CHECK(same_state(loaded.snapshot(), original.snapshot()));
  CHECK(loaded.flatten() == original.flatten());
  CHECK(loaded.layers().at(1).name == original.layers().at(1).name);
  CHECK(loaded.layers().at(2).mask->background() == 0);
  CHECK(loaded.layers().at(3).pixels.empty());
}

TEST_CASE("Every blend mode survives a round trip", "[native]") {
  Document doc(8, 8);
  for (BlendMode mode : kBlendModes) {
    Layer layer;
    layer.blend = mode;
    doc.add_layer(doc.layers().size(), layer);
  }
  CHECK(same_state(decode_native(encode_native(doc)).snapshot(), doc.snapshot()));
}

TEST_CASE("The selection is not saved", "[native]") {
  Document doc = busy();
  doc.set_selection(rect_mask({1, 1, 5, 5}));
  CHECK_FALSE(decode_native(encode_native(doc)).selection());
}

TEST_CASE("Saving to a file and loading it back", "[native]") {
  const Document original = busy();
  const std::string path = temp_path("busy.mps");
  save_native(original, path);
  CHECK(read_file(path + ".saving").empty());  // the temporary file is gone
  CHECK(same_state(load_native(path).snapshot(), original.snapshot()));

  SECTION("saving again replaces the file") {
    save_native(Document(10, 10), path);
    CHECK(load_native(path).width() == 10);
  }
  SECTION("a failed save leaves nothing behind") {
    CHECK_THROWS_AS(save_native(original, "/nonexistent/dir/x.mps"), std::runtime_error);
  }
}

TEST_CASE("The archive holds a manifest and per-layer tile blobs", "[native]") {
  // Entry names are stored as plain text in the zip's directory.
  const std::vector<std::uint8_t> bytes = encode_native(busy());
  const std::string text(bytes.begin(), bytes.end());
  CHECK(text.find("manifest.json") != std::string::npos);
  CHECK(text.find("layers/1/pixels.zst") != std::string::npos);
  CHECK(text.find("layers/1/mask.zst") != std::string::npos);
}

TEST_CASE("Bad files are rejected with a clear error", "[native]") {
  CHECK_THROWS_AS(decode_native({}), std::runtime_error);
  CHECK_THROWS_AS(decode_native({'n', 'o', 'p', 'e'}), std::runtime_error);
  CHECK_THROWS_AS(load_native("/nonexistent/file.mps"), std::runtime_error);

  // A truncated archive.
  std::vector<std::uint8_t> bytes = encode_native(busy());
  bytes.resize(bytes.size() / 2);
  CHECK_THROWS_AS(decode_native(bytes), std::runtime_error);

  // A PNG with the wrong extension.
  const std::string png = temp_path("not_native.png");
  save_png(make_test_pattern(8, 8), png);
  CHECK_THROWS_AS(load_native(png), std::runtime_error);
}

TEST_CASE("Native file names", "[native]") {
  CHECK(is_native_path("a.mps"));
  CHECK(is_native_path("/x/y/Picture.MPS"));
  CHECK_FALSE(is_native_path("a.png"));
  CHECK_FALSE(is_native_path("mps"));
  CHECK_FALSE(is_native_path("a.mps.png"));
}

TEST_CASE("Exporting PNG and JPEG", "[native][io]") {
  Image picture = make_test_pattern(40, 30);
  picture.fill({0, 0, 10, 10}, kTransparent);

  const std::string png = temp_path("export.png");
  save_image(picture, png);
  CHECK(load_image(png) == picture);  // lossless, alpha kept

  const std::string jpg = temp_path("export.JPG");
  save_image(picture, jpg);
  const Image back = load_image(jpg);
  CHECK(back.width() == 40);
  CHECK(back.height() == 30);
  CHECK(back.pixel(5, 5).a == 255);  // transparency flattened...
  CHECK(back.pixel(5, 5).r > 245);   // ...onto white
  CHECK(back.pixel(5, 5).g > 245);
  const Color mid = back.pixel(25, 20), want = picture.pixel(25, 20);
  CHECK(std::abs(mid.g - want.g) < 24);  // close, not exact: JPEG is lossy

  CHECK_THROWS_AS(save_image(picture, temp_path("export.tiff")), std::runtime_error);
}
