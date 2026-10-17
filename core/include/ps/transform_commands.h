#pragma once

#include <cstddef>
#include <string>

#include "ps/command.h"
#include "ps/transform.h"

namespace ps {

// Image Size: resamples the whole document. Throws std::invalid_argument
// for an empty size.
class ImageSizeCommand : public Command {
 public:
  ImageSizeCommand(int width, int height, Interpolation interpolation)
      : width(width), height(height), interpolation(interpolation) {}
  std::string name() const override { return "Image Size"; }

  const int width;
  const int height;
  const Interpolation interpolation;

 protected:
  void apply(Document& doc) override { doc.resize_image(width, height, interpolation); }
};

// Layer > Transform: maps one layer and its mask through a matrix.
class TransformLayerCommand : public Command {
 public:
  TransformLayerCommand(std::size_t index, Matrix3 matrix, Interpolation interpolation)
      : index(index), matrix(matrix), interpolation(interpolation) {}
  std::string name() const override { return "Transform"; }

  const std::size_t index;
  const Matrix3 matrix;  // document coordinates
  const Interpolation interpolation;

 protected:
  void apply(Document& doc) override {
    doc.transform_layer(index, matrix, interpolation);
  }
};

}  // namespace ps
