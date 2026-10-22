#pragma once

#include <cstdint>

constexpr int POLY_DEGREE = 3;
constexpr int POLY_COEFF_NUM = POLY_DEGREE + 1;

bool PolyFit(const double* xs, const double* ys, int n, double* coeffOut);
bool PolyFitU16(const uint16_t* xs, const uint16_t* ys, int n, double* coeffOut);
double PolyEval(const double* coeff, double x);
