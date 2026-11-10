#pragma once

#include "common/Image.h"

#include <filesystem>
#include <stdint.h>

namespace ImageUtils {

MultiImage ReadImage(const std::filesystem::path& path);

bool IsBackground(const MultiImage& images);
Image StitchBackground(const MultiImage& images);

Image Tint(const Image& image, uint8_t r, uint8_t g, uint8_t b, uint8_t rotation, uint8_t swap);
// every other pixel made transparent, in a checkerboard: the see-through
// look of the C2 era, which had no alpha blending (the invisible hand)
Image Stipple(const Image& image);
// the image scaled by k (each side rounded to whole pixels): area sampled,
// with on/off transparency kept (a pixel is opaque where at least half of
// what it covers was); 16-bit and RGBA sprites are averaged, others nearest
Image Scale(const Image& image, float k);

Color GetPixelColor(const Image& image, unsigned int x, unsigned int y);

Image ToRGB24(const Image& image);

} // namespace ImageUtils
