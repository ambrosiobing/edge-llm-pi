// bands.cpp: the window, the transform and the four band energies of project e12. See
// bands.hpp for the contract and the backend rule.
#include "bands.hpp"

#include <cmath>
#include <complex>
#include <cstddef>
#include <stdexcept>

#ifdef E12_HAVE_POCKETFFT
#include "pocketfft_hdronly.h"
#endif

namespace e12 {

const std::array<Band, kBands> kBandTable = {{
    { "B0 10 to 100 Hz",     10.0,  100.0 },
    { "B1 100 to 500 Hz",   100.0,  500.0 },
    { "B2 500 to 1500 Hz",  500.0, 1500.0 },
    { "B3 1500 to 3000 Hz", 1500.0, 3000.0 },
}};

std::vector<double> hann(int n)
{
    std::vector<double> w((size_t)n);
    const double pi = 3.14159265358979323846;
    for (int i = 0; i < n; ++i)
        w[(size_t)i] = 0.5 - 0.5 * std::cos(2.0 * pi * i / (n - 1));
    return w;
}

namespace {

#ifndef E12_HAVE_POCKETFFT
// The fallback: an iterative radix-2 complex transform in place. n must be a power of two;
// the caller checks. Used only when the pocketfft submodule is absent.
void fft_radix2(std::vector<std::complex<double>> &a)
{
    const size_t n = a.size();
    const double pi = 3.14159265358979323846;
    for (size_t i = 1, j = 0; i < n; ++i) {               // bit reversal
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * pi / (double)len;
        const std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k) {
                std::complex<double> u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}
#endif

} // namespace

void power_spectrum(const std::vector<double> &x, std::vector<double> &p)
{
    const size_t n = x.size();
    if (n < 2 || (n & 1u) != 0)
        throw std::invalid_argument("power_spectrum: the length must be even and at least 2");
    p.assign(n / 2 + 1, 0.0);
#ifdef E12_HAVE_POCKETFFT
    std::vector<std::complex<double>> out(n / 2 + 1);
    pocketfft::shape_t  shape{ n };
    pocketfft::stride_t s_in{ (ptrdiff_t)sizeof(double) };
    pocketfft::stride_t s_out{ (ptrdiff_t)sizeof(std::complex<double>) };
    pocketfft::r2c(shape, s_in, s_out, 0, pocketfft::FORWARD, x.data(), out.data(), 1.0);
    for (size_t k = 0; k <= n / 2; ++k)
        p[k] = std::norm(out[k]);
#else
    if ((n & (n - 1)) != 0)
        throw std::invalid_argument("power_spectrum: the built-in transform needs a power of two");
    std::vector<std::complex<double>> a(n);
    for (size_t i = 0; i < n; ++i)
        a[i] = std::complex<double>(x[i], 0.0);
    fft_radix2(a);
    for (size_t k = 0; k <= n / 2; ++k)
        p[k] = std::norm(a[k]);
#endif
}

BandEnergies band_energies(const std::vector<double> &p, double fs_hz)
{
    BandEnergies be;
    const size_t n = (p.size() - 1) * 2;             // the real length behind the spectrum
    const double bin_hz = fs_hz / (double)n;
    for (size_t k = 0; k < p.size(); ++k) {
        const double f = (double)k * bin_hz;
        for (int b = 0; b < kBands; ++b) {
            if (f >= kBandTable[(size_t)b].lo_hz && f < kBandTable[(size_t)b].hi_hz) {
                be.e[(size_t)b] += p[k];
                be.total += p[k];
                break;
            }
        }
    }
    return be;
}

BandEnergies window_energies(const std::vector<float> &ax, const std::vector<float> &ay,
                             const std::vector<float> &az, double fs_hz)
{
    if (ax.size() != (size_t)kWindow || ay.size() != (size_t)kWindow ||
        az.size() != (size_t)kWindow)
        throw std::invalid_argument("window_energies: every axis must hold kWindow samples");
    static const std::vector<double> w = hann(kWindow);
    BandEnergies sum;
    std::vector<double> x((size_t)kWindow), p;
    for (const std::vector<float> *axis : { &ax, &ay, &az }) {
        double mean = 0.0;
        for (float v : *axis) mean += v;
        mean /= (double)kWindow;
        for (size_t i = 0; i < (size_t)kWindow; ++i)
            x[i] = ((double)(*axis)[i] - mean) * w[i];
        power_spectrum(x, p);
        BandEnergies be = band_energies(p, fs_hz);
        for (int b = 0; b < kBands; ++b)
            sum.e[(size_t)b] += be.e[(size_t)b];
        sum.total += be.total;
    }
    return sum;
}

const char *fft_backend()
{
#ifdef E12_HAVE_POCKETFFT
    return "pocketfft";
#else
    return "radix-2 built in";
#endif
}

} // namespace e12
