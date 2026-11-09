#include "ImageUtils.h"
#include "common/Exception.h"
#include "common/Image.h"
#include "common/endianlove.h"
#include "common/io/FileWriter.h"

#include "common/io/Reader.h"

#include <cstring>
#include <vector>
#include <zlib.h>
#include <stdint.h>

void WritePngFile(const Image& image, const std::string& filename) {
	FileWriter out(filename);

	out.write("\x89PNG\r\n\x1a\n", 8);

	uLong crc;
	auto chunk_write = [&](const char* buf, size_t len) {
		crc = crc32_z(crc, reinterpret_cast<const unsigned char*>(buf), len);
		out.write(buf, len);
	};
	auto chunk_write8 = [&](uint8_t val) {
		chunk_write(reinterpret_cast<char*>(&val), 1);
	};
	auto chunk_write32 = [&](uint32_t val) {
		val = is_little_endian() ? byte_swap_32(val) : val;
		chunk_write(reinterpret_cast<char*>(&val), 4);
	};

	// write IHDR chunk
	write32be(out, 13); // ihdr length 13 bytes
	crc = crc32(0L, Z_NULL, 0);
	chunk_write("IHDR", 4);
	chunk_write32(image.width);
	chunk_write32(image.height);
	chunk_write8(8); // bitdepth - 8 bits per channel
	chunk_write8([&] {
		switch (image.format) {
			case if_index8:
				return 3;
			case if_rgb555:
			case if_rgb565:
			case if_bgr24:
			case if_rgb24:
				return 2;
			case if_rgba32:
				break;
		}
		throw Exception("WritePngFile: RGBA images aren't supported yet");
	}()); // colortype
	chunk_write8(0); // compression type 0
	chunk_write8(0); // filter type 0
	chunk_write8(0); // interlace method 0, no interlacing
	write32be(out, crc);

	// write PLTE chunk
	if (image.format == if_index8) {
		write32be(out, 256 * 3); // plte length
		crc = crc32(0L, Z_NULL, 0);
		chunk_write("PLTE", 4);
		for (size_t i = 0; i < 256; ++i) {
			chunk_write8(image.palette[i].r);
			chunk_write8(image.palette[i].g);
			chunk_write8(image.palette[i].b);
		}
		write32be(out, crc);
	}

	// write sBIT chunk
	if (image.format == if_rgb555 || image.format == if_rgb565) {
		write32be(out, 3); // sbit length
		crc = crc32(0L, Z_NULL, 0);
		chunk_write("sBIT", 4);
		chunk_write8(5);
		chunk_write8(image.format == if_rgb565 ? 6 : 5);
		chunk_write8(5);
		write32be(out, crc);
	}

	// generate filtered and compressed data
	auto newimage = image.format == if_index8 ? image : ImageUtils::ToRGB24(image);
	int num_channels = image.format == if_index8 ? 1 : 3;
	std::vector<uint8_t> filtered_data(newimage.data.size() + newimage.height);
	for (int y = 0; y < newimage.height; ++y) {
		auto out = &filtered_data[y * (newimage.width * num_channels + 1)];
		*out = 0; // no filtering
		std::memcpy(out + 1, &newimage.data[y * newimage.width * num_channels], newimage.width * num_channels);
	}
	uLongf compressed_size = compressBound(filtered_data.size());
	std::vector<char> compressed_data(compressed_size);
	int status = compress((Bytef*)compressed_data.data(), &compressed_size, (const Bytef*)filtered_data.data(), filtered_data.size());
	if (status != Z_OK) {
		throw Exception("Failure in compress()");
	}
	compressed_data.resize(compressed_size);

	// write IDAT chunk
	write32be(out, compressed_data.size()); // idat length
	crc = crc32(0L, Z_NULL, 0);
	chunk_write("IDAT", 4);
	chunk_write(compressed_data.data(), compressed_data.size());
	write32be(out, crc);

	// write IEND chunk
	write32be(out, 0);
	crc = crc32(0L, Z_NULL, 0);
	chunk_write("IEND", 4);
	write32be(out, crc);
}
MultiImage ReadPngFile(Reader& in) {
	std::vector<uint8_t> file = in.read_to_end();
	if (file.size() < 8 || memcmp(file.data(), "\x89PNG\r\n\x1a\n", 8) != 0)
		throw Exception("not a PNG file");
	auto be32 = [&](size_t at) {
		return (uint32_t(file[at]) << 24) | (uint32_t(file[at + 1]) << 16) | (uint32_t(file[at + 2]) << 8) | uint32_t(file[at + 3]);
	};
	uint32_t width = 0, height = 0;
	int bitdepth = 0, colortype = 0, interlace = 0;
	unsigned int frames = 1;
	std::vector<uint8_t> idat;
	size_t at = 8;
	while (at + 8 <= file.size()) {
		uint32_t len = be32(at);
		std::string type(reinterpret_cast<const char*>(&file[at + 4]), 4);
		size_t data = at + 8;
		if (data + len > file.size())
			throw Exception("truncated PNG file");
		if (type == "IHDR") {
			width = be32(data);
			height = be32(data + 4);
			bitdepth = file[data + 8];
			colortype = file[data + 9];
			interlace = file[data + 12];
		} else if (type == "IDAT") {
			idat.insert(idat.end(), file.begin() + data, file.begin() + data + len);
		} else if (type == "tEXt") {
			std::string text(reinterpret_cast<const char*>(&file[data]), len);
			size_t nul = text.find('\0');
			if (nul != std::string::npos && text.substr(0, nul) == "openc2e-frames")
				frames = std::max(1, atoi(text.c_str() + nul + 1));
		} else if (type == "IEND") {
			break;
		}
		at = data + len + 4; // skip the CRC
	}
	if (bitdepth != 8 || (colortype != 6 && colortype != 2) || interlace != 0)
		throw Exception("PNG sprites must be 8-bit RGB or RGBA, not interlaced");
	const unsigned int channels = colortype == 6 ? 4 : 3;
	const size_t stride = size_t(width) * channels;

	std::vector<uint8_t> raw((stride + 1) * height);
	uLongf rawlen = raw.size();
	if (uncompress(raw.data(), &rawlen, idat.data(), idat.size()) != Z_OK || rawlen != raw.size())
		throw Exception("bad PNG image data");

	// undo the per-row filters
	std::vector<uint8_t> pix(stride * height);
	for (uint32_t y = 0; y < height; y++) {
		uint8_t filter = raw[y * (stride + 1)];
		const uint8_t* src = &raw[y * (stride + 1) + 1];
		uint8_t* row = &pix[y * stride];
		const uint8_t* up = y ? &pix[(y - 1) * stride] : nullptr;
		for (size_t i = 0; i < stride; i++) {
			int a = i >= channels ? row[i - channels] : 0;
			int b = up ? up[i] : 0;
			int c = (up && i >= channels) ? up[i - channels] : 0;
			int v = src[i];
			switch (filter) {
				case 0: break;
				case 1: v += a; break;
				case 2: v += b; break;
				case 3: v += (a + b) / 2; break;
				case 4: {
					int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
					v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
					break;
				}
				default: throw Exception("bad PNG filter");
			}
			row[i] = uint8_t(v);
		}
	}

	if (height % frames != 0)
		throw Exception("PNG sprite height isn't a whole number of frames");
	const uint32_t fh = height / frames;
	MultiImage images;
	for (unsigned int f = 0; f < frames; f++) {
		Image img;
		img.width = width;
		img.height = fh;
		img.format = if_rgba32;
		img.data = shared_array<uint8_t>(size_t(width) * fh * 4);
		for (uint32_t y = 0; y < fh; y++) {
			const uint8_t* row = &pix[(f * fh + y) * stride];
			for (uint32_t x = 0; x < width; x++) {
				uint8_t* d = &img.data[(size_t(y) * width + x) * 4];
				d[0] = row[x * channels];
				d[1] = row[x * channels + 1];
				d[2] = row[x * channels + 2];
				d[3] = channels == 4 ? row[x * channels + 3] : 255;
			}
		}
		images.push_back(img);
	}
	return images;
}
