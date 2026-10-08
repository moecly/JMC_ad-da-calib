#pragma once

constexpr int POLY_DEGREE = 3;
constexpr int POLY_COEFF_NUM = POLY_DEGREE + 1;

bool PolyFit(const double* xs, const double* ys, int n, double* coeffOut);
double PolyEval(const double* coeff, double x);
