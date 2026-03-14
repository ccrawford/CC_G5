#pragma once

// effect_gradient_alpha.hpp
//
// Custom LovyanGFX effectors that alpha-blend across a region by interpolating
// between two argb8888_t values -- one for each end of the gradient.
// Both the RGB color and the alpha channel are interpolated independently,
// so you can fade color, opacity, or both simultaneously.
//
// Usage:
//
//   // Horizontal: left edge blends with black at 75% alpha,
//   //             right edge blends with black at 0% alpha (invisible)
//   sprite.effect(x, y, w, h,
//       effect_alpha_gradient_h(lgfx::argb8888_t{0xC0, 0x00, 0x00, 0x00},  // left
//                               lgfx::argb8888_t{0x00, 0x00, 0x00, 0x00},  // right
//                               GradientCurve::Linear));
//
//   // Vertical: fade from semi-transparent red at top to semi-transparent
//   //           blue at bottom
//   sprite.effect(x, y, w, h,
//       effect_alpha_gradient_v(lgfx::argb8888_t{0x80, 0xFF, 0x00, 0x00},  // top
//                               lgfx::argb8888_t{0x80, 0x00, 0x00, 0xFF},  // bottom
//                               GradientCurve::Exponential));
//
// Notes:
//   - 'start' is the left/top end; 'end' is the right/bottom end.
//     Swap start and end to reverse the gradient direction.
//   - A pixel with alpha=0 at a given position is a no-op; the pixel is
//     left completely untouched at that end of the gradient.
//   - x, y in operator() are absolute display coordinates; the effector
//     self-calibrates its bounding box from the first pixel it sees.
//   - GradientCurve::Exponential uses a t^2 curve (gamma 2.0), which looks
//     perceptually smoother than linear for fade-to-transparent effects.

#include <LovyanGFX.hpp>
#include <cstdint>

// ---------------------------------------------------------------------------
// Curve selection
// ---------------------------------------------------------------------------

enum class GradientCurve : uint8_t
{
    Linear,       // position ramps linearly across the region
    Exponential,  // position ramps as t^2 (perceptually smoother)
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace detail
{
    // Apply curve shaping to t255 in [0,255], returning shaped t in [0,255].
    inline uint_fast16_t apply_curve(uint_fast16_t t255, GradientCurve curve)
    {
        switch (curve)
        {
        case GradientCurve::Exponential:
            return (t255 * t255) >> 8;  // t^2, stays in [0,255]
        case GradientCurve::Linear:
        default:
            return t255;
        }
    }

    // Lerp a single channel from 'a' to 'b' by t255 in [0,255].
    inline uint_fast16_t lerp8(uint_fast16_t a, uint_fast16_t b,
                                uint_fast16_t t255)
    {
        return (a * (256 - t255) + b * (t255 + 1)) >> 8;
    }

    // Blend a source color+alpha onto a destination pixel.
    // Mirrors effect_fill_alpha arithmetic exactly.
    template <typename TDstColor>
    inline void blend(TDstColor& dst,
                      uint_fast16_t r8, uint_fast16_t g8, uint_fast16_t b8,
                      uint_fast16_t alpha)
    {
        uint_fast16_t inv = 256 - alpha;
        dst.set((r8 * (1 + alpha) + dst.R8() * inv) >> 8,
                (g8 * (1 + alpha) + dst.G8() * inv) >> 8,
                (b8 * (1 + alpha) + dst.B8() * inv) >> 8);
    }
} // namespace detail

// ---------------------------------------------------------------------------
// Shared CRTP base -- avoids duplicating operator() between H and V variants
// ---------------------------------------------------------------------------

template <typename Derived>
struct effect_alpha_gradient_base
{
    effect_alpha_gradient_base(lgfx::argb8888_t start, lgfx::argb8888_t end,
                               GradientCurve curve)
        : _r0   { start.R8() }, _g0 { start.G8() }, _b0 { start.B8() }
        , _a0   { start.A8() }
        , _r1   { end.R8()   }, _g1 { end.G8()   }, _b1 { end.B8()   }
        , _a1   { end.A8()   }
        , _curve{ curve       }
        , _origin { INT32_MIN }
        , _span   { 0         }
    {}

    template <typename TDstColor>
    void operator()(int32_t x, int32_t y, TDstColor& dst)
    {
        int32_t pos = static_cast<Derived*>(this)->axis(x, y);

        if (_origin == INT32_MIN) _origin = pos;

        uint_fast16_t offset = static_cast<uint_fast16_t>(pos - _origin);
        if (offset >= _span) _span = offset + 1;

        uint_fast16_t t255 = (_span > 1)
            ? static_cast<uint_fast16_t>(
                  static_cast<uint32_t>(offset) * 255 / (_span - 1))
            : 0;

        t255 = detail::apply_curve(t255, _curve);

        // Interpolate all four channels between start and end
        uint_fast16_t r = detail::lerp8(_r0, _r1, t255);
        uint_fast16_t g = detail::lerp8(_g0, _g1, t255);
        uint_fast16_t b = detail::lerp8(_b0, _b1, t255);
        uint_fast16_t a = detail::lerp8(_a0, _a1, t255);

        if (a == 0) return;  // fully transparent -- leave pixel untouched

        detail::blend(dst, r, g, b, a);
    }

protected:
    uint8_t       _r0, _g0, _b0, _a0;  // start (left/top) color+alpha
    uint8_t       _r1, _g1, _b1, _a1;  // end (right/bottom) color+alpha
    GradientCurve _curve;
    int32_t       _origin;              // first coordinate seen (self-calibrated)
    uint_fast16_t _span;                // running width/height (self-calibrated)
};

// ---------------------------------------------------------------------------
// Horizontal gradient: start = left edge, end = right edge
// ---------------------------------------------------------------------------

struct effect_alpha_gradient_h
    : effect_alpha_gradient_base<effect_alpha_gradient_h>
{
    effect_alpha_gradient_h(lgfx::argb8888_t start,
                            lgfx::argb8888_t end,
                            GradientCurve    curve = GradientCurve::Linear)
        : effect_alpha_gradient_base(start, end, curve)
    {}

    // CRTP hook: return the coordinate used for interpolation
    static int32_t axis(int32_t x, int32_t y) { (void)y; return x; }
};

// ---------------------------------------------------------------------------
// Vertical gradient: start = top edge, end = bottom edge
// ---------------------------------------------------------------------------

struct effect_alpha_gradient_v
    : effect_alpha_gradient_base<effect_alpha_gradient_v>
{
    effect_alpha_gradient_v(lgfx::argb8888_t start,
                            lgfx::argb8888_t end,
                            GradientCurve    curve = GradientCurve::Linear)
        : effect_alpha_gradient_base(start, end, curve)
    {}

    static int32_t axis(int32_t x, int32_t y) { (void)x; return y; }
};