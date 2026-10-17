#include "ps/native_format.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include <miniz.h>
#include <nlohmann/json.hpp>
#include <zstd.h>

#include "ps/blend.h"

namespace ps {

namespace {

using json = nlohmann::json;
using Bytes = std::vector<std::uint8_t>;

constexpr const char* kFormatName = "mock-photoshop";
constexpr const char* kManifest = "manifest.json";
constexpr std::size_t kTilePixels = static_cast<std::size_t>(kTileSize) * kTileSize;
// Refuse blobs that claim to unpack to more than this.
constexpr unsigned long long kMaxBlob = 1ULL << 32;

[[noreturn]] void fail(const std::string& why) {
  throw std::runtime_error("Not a readable .mps file: " + why);
}

void put_int(Bytes& out, std::int32_t v) {
  const auto u = static_cast<std::uint32_t>(v);
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(u >> (8 * i)));
}

std::int32_t get_int(const std::uint8_t* p) {
  std::uint32_t u = 0;
  for (int i = 0; i < 4; ++i) u |= static_cast<std::uint32_t>(p[i]) << (8 * i);
  return static_cast<std::int32_t>(u);
}

Bytes compress(const Bytes& raw) {
  Bytes out(ZSTD_compressBound(raw.size()));
  const std::size_t n = ZSTD_compress(out.data(), out.size(), raw.data(), raw.size(), 3);
  if (ZSTD_isError(n)) throw std::runtime_error("zstd compression failed");
  out.resize(n);
  return out;
}

Bytes decompress(const Bytes& packed) {
  const unsigned long long size = ZSTD_getFrameContentSize(packed.data(), packed.size());
  if (size == ZSTD_CONTENTSIZE_ERROR || size == ZSTD_CONTENTSIZE_UNKNOWN || size > kMaxBlob) {
    fail("damaged tile data");
  }
  Bytes out(static_cast<std::size_t>(size));
  const std::size_t n = ZSTD_decompress(out.data(), out.size(), packed.data(), packed.size());
  if (ZSTD_isError(n) || n != out.size()) fail("damaged tile data");
  return out;
}

// Tiles as records: x, y (little-endian int32) then the tile's bytes.
template <typename Grid, typename Read>
Bytes encode_tiles(const Grid& grid, Read read) {
  Bytes raw;
  for (TileCoord c : grid.tile_coords()) {
    put_int(raw, c.x);
    put_int(raw, c.y);
    read(tile_rect(c), raw);
  }
  return compress(raw);
}

template <typename Write>
void decode_tiles(const Bytes& packed, std::size_t bytes_per_pixel, Write write) {
  const Bytes raw = decompress(packed);
  const std::size_t record = 8 + kTilePixels * bytes_per_pixel;
  if (raw.size() % record != 0) fail("tile data has the wrong length");
  for (std::size_t at = 0; at < raw.size(); at += record) {
    const TileCoord c{get_int(&raw[at]), get_int(&raw[at + 4])};
    write(tile_rect(c), &raw[at + 8]);
  }
}

Bytes pixel_blob(const TileGrid& pixels) {
  return encode_tiles(pixels, [&](Rect r, Bytes& out) {
    const Image tile = pixels.read(r);
    out.insert(out.end(), tile.data(), tile.data() + tile.size_bytes());
  });
}

Bytes mask_blob(const MaskGrid& mask) {
  return encode_tiles(mask, [&](Rect r, Bytes& out) {
    const std::vector<std::uint8_t> values = mask.read(r);
    out.insert(out.end(), values.begin(), values.end());
  });
}

BlendMode parse_blend(const std::string& name) {
  for (BlendMode mode : kBlendModes) {
    if (name == blend_mode_name(mode)) return mode;
  }
  fail("unknown blend mode '" + name + "'");
}

class ZipReader {
 public:
  explicit ZipReader(const Bytes& bytes) {
    std::memset(&zip_, 0, sizeof(zip_));
    if (!mz_zip_reader_init_mem(&zip_, bytes.data(), bytes.size(), 0)) fail("not a zip archive");
  }
  ~ZipReader() { mz_zip_reader_end(&zip_); }
  ZipReader(const ZipReader&) = delete;
  ZipReader& operator=(const ZipReader&) = delete;

  Bytes read(const std::string& name) {
    std::size_t size = 0;
    void* data = mz_zip_reader_extract_file_to_heap(&zip_, name.c_str(), &size, 0);
    if (!data) fail("missing '" + name + "'");
    Bytes out(static_cast<std::uint8_t*>(data), static_cast<std::uint8_t*>(data) + size);
    mz_free(data);
    return out;
  }

 private:
  mz_zip_archive zip_;
};

}  // namespace

bool is_native_path(const std::string& path) {
  const std::string ext = kNativeExtension;
  if (path.size() < ext.size()) return false;
  return std::equal(ext.begin(), ext.end(), path.end() - static_cast<long>(ext.size()),
                    [](char a, char b) {
                      return std::tolower(static_cast<unsigned char>(a)) ==
                             std::tolower(static_cast<unsigned char>(b));
                    });
}

std::vector<std::uint8_t> encode_native(const Document& doc) {
  mz_zip_archive zip;
  std::memset(&zip, 0, sizeof(zip));
  if (!mz_zip_writer_init_heap(&zip, 0, 0)) throw std::runtime_error("zip setup failed");
  const auto add = [&](const std::string& name, const Bytes& data, bool deflate) {
    if (!mz_zip_writer_add_mem(&zip, name.c_str(), data.data(), data.size(),
                               deflate ? MZ_DEFAULT_COMPRESSION : MZ_NO_COMPRESSION)) {
      mz_zip_writer_end(&zip);
      throw std::runtime_error("zip write failed");
    }
  };

  json manifest = {{"format", kFormatName},
                   {"version", kNativeFormatVersion},
                   {"width", doc.width()},
                   {"height", doc.height()},
                   {"layers", json::array()}};
  for (std::size_t i = 0; i < doc.layers().size(); ++i) {
    const Layer& layer = doc.layers().at(i);
    const std::string folder = "layers/" + std::to_string(i) + "/";
    json entry = {{"name", layer.name},
                  {"offset", {layer.offset.x, layer.offset.y}},
                  {"opacity", layer.opacity},
                  {"visible", layer.visible},
                  {"blend", blend_mode_name(layer.blend)},
                  {"pixels", folder + "pixels.zst"}};
    add(folder + "pixels.zst", pixel_blob(layer.pixels), false);
    if (layer.mask) {
      entry["mask"] = {{"background", layer.mask->background()},
                       {"tiles", folder + "mask.zst"}};
      add(folder + "mask.zst", mask_blob(*layer.mask), false);
    }
    manifest["layers"].push_back(entry);
  }
  const std::string text = manifest.dump(2);
  add(kManifest, Bytes(text.begin(), text.end()), true);

  void* data = nullptr;
  std::size_t size = 0;
  if (!mz_zip_writer_finalize_heap_archive(&zip, &data, &size)) {
    mz_zip_writer_end(&zip);
    throw std::runtime_error("zip write failed");
  }
  Bytes out(static_cast<std::uint8_t*>(data), static_cast<std::uint8_t*>(data) + size);
  mz_zip_writer_end(&zip);
  return out;
}

Document decode_native(const std::vector<std::uint8_t>& bytes) {
  ZipReader zip(bytes);
  try {
    const Bytes text = zip.read(kManifest);
    const json manifest = json::parse(text.begin(), text.end());
    if (manifest.value("format", "") != kFormatName) fail("wrong format");
    const int version = manifest.at("version").get<int>();
    if (version > kNativeFormatVersion) {
      fail("made by a newer version (format " + std::to_string(version) + ")");
    }
    if (version < 1) fail("bad version");

    DocumentState state;
    state.width = manifest.at("width").get<int>();
    state.height = manifest.at("height").get<int>();
    for (const json& entry : manifest.at("layers")) {
      Layer layer;
      layer.name = entry.at("name").get<std::string>();
      layer.offset = {entry.at("offset").at(0).get<int>(), entry.at("offset").at(1).get<int>()};
      layer.opacity = std::clamp(entry.at("opacity").get<float>(), 0.0f, 1.0f);
      layer.visible = entry.at("visible").get<bool>();
      layer.blend = parse_blend(entry.at("blend").get<std::string>());
      decode_tiles(zip.read(entry.at("pixels").get<std::string>()), 4,
                   [&](Rect r, const std::uint8_t* data) {
                     Image tile(r.width, r.height);
                     std::memcpy(tile.data(), data, tile.size_bytes());
                     layer.pixels.write(tile, {r.x, r.y});
                   });
      if (entry.contains("mask")) {
        const json& m = entry.at("mask");
        MaskGrid mask(m.at("background").get<std::uint8_t>());
        decode_tiles(zip.read(m.at("tiles").get<std::string>()), 1,
                     [&](Rect r, const std::uint8_t* data) {
                       mask.write(std::vector<std::uint8_t>(data, data + kTilePixels), r);
                     });
        layer.mask = std::move(mask);
      }
      state.layers.insert(state.layers.size(), std::move(layer));
    }
    if (state.width <= 0 || state.height <= 0) fail("bad canvas size");
    Document doc(1, 1);
    doc.restore(state);
    return doc;
  } catch (const json::exception& e) {
    fail(std::string("bad manifest (") + e.what() + ")");
  }
}

void save_native(const Document& doc, const std::string& path) {
  const Bytes bytes = encode_native(doc);
  const std::string temp = path + ".saving";
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    if (!out) {
      std::remove(temp.c_str());
      throw std::runtime_error("Could not write '" + path + "'");
    }
  }
  if (std::rename(temp.c_str(), path.c_str()) != 0) {
    std::remove(temp.c_str());
    throw std::runtime_error("Could not write '" + path + "'");
  }
}

Document load_native(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("Could not open '" + path + "'");
  const Bytes bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return decode_native(bytes);
}

}  // namespace ps
