#include "scene/effect_input_mapping.h"

#include "check.h"

#include <array>
#include <cmath>
#include <limits>

namespace {
  umbriel::EffectInputMapping map(const std::array<double, 9>& values) { return umbriel::EffectInputMapping{values}; }
} // namespace

UMBRIEL_TEST(effectInputIdentity) {
  auto m = map({1, 0, 0, 0, 1, 0, 0, 0, 1});
  CHECK(m.valid());
  const auto p = m.sourceAt(0.25, 0.75);
  CHECK(p.has_value());
  if (p) {
    CHECK(std::abs(p->x - 0.25) < 1e-12);
    CHECK(std::abs(p->y - 0.75) < 1e-12);
  }
}

UMBRIEL_TEST(effectInputCenteredScale085) {
  // Source = center + (visible - center)/0.85.
  const double k = 1.0 / 0.85;
  auto m = map({k, 0, 0.5 * (1 - k), 0, k, 0.5 * (1 - k), 0, 0, 1});
  CHECK(m.valid());
  const auto p = m.sourceAt(0.585, 0.5);
  CHECK(p.has_value());
  if (p) {
    CHECK(std::abs(p->x - 0.6) < 1e-12);
    CHECK(std::abs(p->y - 0.5) < 1e-12);
  }
  CHECK(!m.sourceAt(0.0, 0.0).has_value());
}

UMBRIEL_TEST(effectInputProjective) {
  auto m = map({1, 0, 0, 0, 1, 0, 0.5, 0, 1});
  CHECK(m.valid());
  const auto p = m.sourceAt(0.8, 0.4);
  CHECK(p.has_value());
  if (p) {
    CHECK(std::abs(p->x - (0.8 / 1.4)) < 1e-12);
    CHECK(std::abs(p->y - (0.4 / 1.4)) < 1e-12);
  }
}

UMBRIEL_TEST(effectInputRejectsInvalid) {
  CHECK(!map({1, 0, 0, 0, 0, 0, 0, 0, 1}).valid());
  CHECK(!map({0, 0, 0, 0, 0, 0, 0, 0, 0}).valid());
  CHECK(!map({1, 0, 0, 0, 1, 0, 0, 0, std::numeric_limits<double>::quiet_NaN()}).valid());
  auto m = map({1, 0, 0, 0, 1, 0, 0, 0, 1});
  CHECK(!m.sourceAt(-0.1, 0.5).has_value());
  CHECK(!m.sourceAt(1.1, 0.5).has_value());
  CHECK(!m.sourceAt(0.5, std::numeric_limits<double>::infinity()).has_value());
}

int main() { return RUN_TESTS(); }
