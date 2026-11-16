#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace umbriel {

  // A projective transformation from normalized *visible* output UV to the
  // original scene UV. The origin is top-left and the matrix is row-major:
  //
  //   [a b c]   [u]           source = (a*u + b*v + c) / w,
  //   [d e f] * [v],     w = g*u + h*v + i.
  //   [g h i]   [1]
  //
  // No knowledge of the shader or its uniforms is required by the compositor.
  struct EffectInputMapping {
    struct Point {
      double x = 0.0;
      double y = 0.0;
    };

    std::array<double, 9> matrix{};
    bool operator==(const EffectInputMapping&) const = default;

    bool valid() const {
      double largest = 0.0;
      for (double value : matrix) {
        if (!std::isfinite(value)) {
          return false;
        }
        largest = std::max(largest, std::abs(value));
      }
      if (largest < 1e-12 || largest > 1e12) {
        return false;
      }
      const double a = matrix[0] / largest, b = matrix[1] / largest, c = matrix[2] / largest;
      const double d = matrix[3] / largest, e = matrix[4] / largest, f = matrix[5] / largest;
      const double g = matrix[6] / largest, h = matrix[7] / largest, i = matrix[8] / largest;
      const double determinant = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
      // A singular transform cannot establish a stable source-point mapping.
      return std::isfinite(determinant) && std::abs(determinant) > 1e-12;
    }

    // Null means no visible source pixel corresponds to this output point.
    // In particular, never route clicks from the black borders of a warped
    // scene onto unrelated source surfaces.
    std::optional<Point> sourceAt(double u, double v) const {
      if (!std::isfinite(u) || !std::isfinite(v) || u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0) {
        return std::nullopt;
      }
      const double w = matrix[6] * u + matrix[7] * v + matrix[8];
      if (!std::isfinite(w) || w <= 1e-9) {
        return std::nullopt;
      }
      const double x = (matrix[0] * u + matrix[1] * v + matrix[2]) / w;
      const double y = (matrix[3] * u + matrix[4] * v + matrix[5]) / w;
      if (!std::isfinite(x) || !std::isfinite(y) || x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0) {
        return std::nullopt;
      }
      return Point{x, y};
    }
  };

} // namespace umbriel
