#include <unity.h>
#include "light_core.h"

void setUp(void) {}
void tearDown(void) {}

void test_kelvin_to_rgb_2000K(void) {
  float r = 0.0f, g = 0.0f, b = 0.0f;
  light_core::kelvinToRgbFloat(2000, r, g, b);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, r);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.54f, g);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.12f, b);
}

void test_kelvin_to_rgb_6500K(void) {
  float r = 0.0f, g = 0.0f, b = 0.0f;
  light_core::kelvinToRgbFloat(6500, r, g, b);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, r);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, g);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, b);
}

void test_kelvin_clamping(void) {
  float r1 = 0, g1 = 0, b1 = 0;
  float r2 = 0, g2 = 0, b2 = 0;
  light_core::kelvinToRgbFloat(1000, r1, g1, b1);
  light_core::kelvinToRgbFloat(2000, r2, g2, b2);
  TEST_ASSERT_EQUAL_FLOAT(r2, r1);
  TEST_ASSERT_EQUAL_FLOAT(g2, g1);
  TEST_ASSERT_EQUAL_FLOAT(b2, b1);
}

void test_gamma_12_bounds(void) {
  TEST_ASSERT_EQUAL_UINT32(0, light_core::applyGamma12(0.0f, 4095));
  TEST_ASSERT_EQUAL_UINT32(4095, light_core::applyGamma12(1.0f, 4095));
  TEST_ASSERT_UINT32_WITHIN(10, 598, light_core::applyGamma12(0.5f, 4095));
}

void test_hue_cardinals(void) {
  float r = 0, g = 0, b = 0;
  light_core::hueToRgbFloat(0.0f, r, g, b); // Red
  TEST_ASSERT_EQUAL_FLOAT(1.0f, r);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, g);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, b);

  light_core::hueToRgbFloat(1.0f / 3.0f, r, g, b); // Green
  TEST_ASSERT_EQUAL_FLOAT(0.0f, r);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, g);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, b);

  light_core::hueToRgbFloat(2.0f / 3.0f, r, g, b); // Blue
  TEST_ASSERT_EQUAL_FLOAT(0.0f, r);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, g);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, b);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_kelvin_to_rgb_2000K);
  RUN_TEST(test_kelvin_to_rgb_6500K);
  RUN_TEST(test_kelvin_clamping);
  RUN_TEST(test_gamma_12_bounds);
  RUN_TEST(test_hue_cardinals);
  return UNITY_END();
}
