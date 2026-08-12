#pragma once

// Educational adaptation of Tau Labs/dRonin matrix_mul.
// Copyright (C) 2012-2013 Tau Labs.
// GPL-3.0-or-later; see THIRD_PARTY_NOTICES.md for provenance.

void matrix_mul_before(
    const float* a,
    const float* b,
    float* out,
    int a_rows,
    int shared_dimension,
    int b_columns);

void matrix_mul_after(
    const float* a,
    const float* b,
    float* out,
    int a_rows,
    int shared_dimension,
    int b_columns);
