#include "matrix.hpp"

// Educational adaptation of Tau Labs/dRonin matrix_mul.
// Copyright (C) 2012-2013 Tau Labs. GPL-3.0-or-later.

#if defined(_MSC_VER)
#define PERFBANK_NOINLINE __declspec(noinline)
#else
#define PERFBANK_NOINLINE __attribute__((noinline))
#endif

// BEFORE: one multiply-add and one loop-control step per iteration.
PERFBANK_NOINLINE void matrix_mul_before(
    const float* a,
    const float* b,
    float* out,
    int a_rows,
    int shared_dimension,
    int b_columns) {
  for (int row = 0; row < a_rows; ++row) {
    for (int column = 0; column < b_columns; ++column) {
      float sum = 0.0F;
      const float* a_position = a + row * shared_dimension;
      const float* b_position = b + column;

      for (int k = 0; k < shared_dimension; ++k) {
        sum += a_position[k] * (*b_position);
        b_position += b_columns;
      }

      out[row * b_columns + column] = sum;
    }
  }
}
