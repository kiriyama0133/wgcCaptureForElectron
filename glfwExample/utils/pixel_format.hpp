#pragma once

#include <cstddef>
#include <cstdint>

namespace wgc::pixel {

/// D3D11 Map 只读后缀通常为 DXGI BGRA（B,G,R,A）；转成前端常用 RGBA（R,G,B,A）。
/// \param dst 紧密排列，步长 = width * 4。
/// \param src 每行起始字节；行距可为 RowPitch（≥ width*4）。
inline void copy_mapped_bgra_to_rgba(
    uint8_t* dst,
    const uint8_t* src,
    int width,
    int height,
    std::size_t src_row_pitch)
{
    const std::size_t width_in_bytes = static_cast<std::size_t>(width) * 4u;
    for (int y = 0; y < height; ++y) {
        const uint8_t* row_src = src + static_cast<std::size_t>(y) * src_row_pitch;
        uint8_t* row_dst = dst + static_cast<std::size_t>(y) * width_in_bytes;
        for (int x = 0; x < width; ++x) {
            const std::size_t i = static_cast<std::size_t>(x) * 4u;
            row_dst[i + 0] = row_src[i + 2];
            row_dst[i + 1] = row_src[i + 1];
            row_dst[i + 2] = row_src[i + 0];
            row_dst[i + 3] = row_src[i + 3];
        }
    }
}

} // namespace wgc::pixel
