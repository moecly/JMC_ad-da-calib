#include "calib/PolyFit.h"

#include <cmath>

static double PowI(double x, int e) {
    double result = 1.0;
    double square = x;
    while (e != 0) {
        if (e & 1) result *= square;
        square *= square;
        e >>= 1;
    }
    return result;
}

bool PolyFit(const double* xs, const double* ys, int n, double* coeffOut) {
    if (n <= POLY_DEGREE) {
        return false;
    }

    double xmin = xs[0];
    double xmax = xs[0];
    for (int i = 1; i < n; i++) {
        if (xs[i] < xmin) xmin = xs[i];
        if (xs[i] > xmax) xmax = xs[i];
    }
    const double mid = 0.5 * (xmin + xmax);
    double half = 0.5 * (xmax - xmin);
    if (half == 0.0 || !std::isfinite(half)) {
        half = 1.0;
    }

    const int nc = POLY_COEFF_NUM;
    double a[POLY_COEFF_NUM][POLY_COEFF_NUM + 1];
    for (int i = 0; i < nc; i++) {
        for (int j = 0; j <= nc; j++) {
            a[i][j] = 0.0;
        }
    }
    for (int s = 0; s < n; s++) {
        const double u = (xs[s] - mid) / half;
        double p[2 * POLY_DEGREE + 1];
        for (int k = 0; k <= 2 * POLY_DEGREE; k++) {
            p[k] = PowI(u, k);
        }
        for (int i = 0; i < nc; i++) {
            for (int j = 0; j < nc; j++) {
                a[i][j] += p[i + j];
            }
            a[i][nc] += p[i] * ys[s];
        }
    }

    for (int col = 0; col < nc; col++) {
        int piv = col;
        for (int r = col + 1; r < nc; r++) {
            if (std::fabs(a[r][col]) > std::fabs(a[piv][col])) {
                piv = r;
            }
        }
        if (piv != col) {
            for (int k = 0; k <= nc; k++) {
                const double tmp = a[col][k];
                a[col][k] = a[piv][k];
                a[piv][k] = tmp;
            }
        }
        const double d = a[col][col];
        if (d == 0.0 || !std::isfinite(d)) {
            return false;
        }
        for (int r = 0; r < nc; r++) {
            if (r == col) continue;
            const double f = a[r][col] / d;
            if (f != 0.0) {
                for (int k = col; k <= nc; k++) {
                    a[r][k] -= f * a[col][k];
                }
            }
        }
    }

    double e[POLY_COEFF_NUM];
    for (int i = 0; i < nc; i++) {
        e[i] = a[i][nc] / a[i][i];
    }

    const double ia = 1.0 / half;
    const double ib = -mid / half;

    double binom[POLY_COEFF_NUM][POLY_COEFF_NUM];
    for (int i = 0; i < nc; i++) {
        for (int k = 0; k < nc; k++) {
            binom[i][k] = 0.0;
        }
    }
    for (int i = 0; i < nc; i++) {
        binom[i][0] = 1.0;
        binom[i][i] = 1.0;
        for (int k = 1; k < i; k++) {
            binom[i][k] = binom[i - 1][k - 1] + binom[i - 1][k];
        }
    }

    for (int k = 0; k < nc; k++) {
        coeffOut[k] = 0.0;
    }
    for (int j = 0; j < nc; j++) {
        double apk = 1.0;
        for (int k = 0; k <= j; k++) {
            coeffOut[k] += e[j] * binom[j][k] * apk * PowI(ib, j - k);
            apk *= ia;
        }
    }
    return true;
}

double PolyEval(const double* coeff, double x) {
    double acc = 0.0;
    for (int k = POLY_DEGREE; k >= 0; k--) {
        acc = acc * x + coeff[k];
    }
    return acc;
}
