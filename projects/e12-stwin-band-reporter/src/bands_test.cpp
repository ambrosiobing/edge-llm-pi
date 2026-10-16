// bands_test.cpp: the unit test that keeps the bands honest. A synthetic tone of known
// frequency goes through the same mean removal, Hann window, transform and band table the
// bench run uses, and the test asks three things: that the tone's energy lands in the band
// the table says it should (at least 99 percent of the total), that a gravity offset on an
// axis changes nothing, and that the transform's scaling obeys Parseval on whichever
// backend was compiled in. Exit 0 when every check passes, 1 otherwise, with each failure
// printed. Registered with ctest as e12_bands_test.
#include "bands.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int failures = 0;

void expect(bool ok, const char *what)
{
    std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++failures;
}

std::vector<float> tone(double f_hz, double amp, double offset)
{
    std::vector<float> s((size_t)e12::kWindow);
    const double pi = 3.14159265358979323846;
    for (int i = 0; i < e12::kWindow; ++i)
        s[(size_t)i] = (float)(offset + amp * std::sin(2.0 * pi * f_hz * i / e12::kSampleRateHz));
    return s;
}

const std::vector<float> &silence()
{
    static const std::vector<float> z((size_t)e12::kWindow, 0.0f);
    return z;
}

void tone_lands_in(double f_hz, int band, const char *label)
{
    e12::BandEnergies be = e12::window_energies(tone(f_hz, 100.0, 0.0), silence(), silence(),
                                                e12::kSampleRateHz);
    char msg[128];
    std::snprintf(msg, sizeof msg, "%s: %.0f Hz tone puts %.4f of the total in %s", label,
                  f_hz, be.share(band), e12::kBandTable[(size_t)band].name);
    expect(be.share(band) >= 0.99, msg);
}

} // namespace

int main()
{
    std::printf("e12_bands_test: backend %s, window %d, rate %.0f Hz\n", e12::fft_backend(),
                e12::kWindow, e12::kSampleRateHz);

    tone_lands_in(50.0, 0, "B0");
    tone_lands_in(200.0, 1, "B1");
    tone_lands_in(900.0, 2, "B2");
    tone_lands_in(2200.0, 3, "B3");

    // A 1 g offset on the axis (gravity, 1000 mg) must not move the share: the mean is
    // removed before the window, and the Hann window keeps the residual out of B0.
    {
        e12::BandEnergies plain = e12::window_energies(tone(200.0, 100.0, 0.0), silence(),
                                                       silence(), e12::kSampleRateHz);
        e12::BandEnergies lifted = e12::window_energies(tone(200.0, 100.0, 1000.0), silence(),
                                                        silence(), e12::kSampleRateHz);
        double d = std::fabs(plain.share(1) - lifted.share(1));
        char msg[96];
        std::snprintf(msg, sizeof msg, "a 1000 mg offset moves the B1 share by %.2e", d);
        expect(d < 1e-6, msg);
    }

    // Parseval on the transform itself: the two-sided power sum equals n times the sum of
    // squares of the input. This is the check that catches a wrongly scaled backend.
    {
        std::vector<double> x((size_t)e12::kWindow), p;
        double sumsq = 0.0;
        for (size_t i = 0; i < x.size(); ++i) {
            x[i] = std::sin(0.37 * (double)i) + 0.25 * std::cos(1.91 * (double)i);
            sumsq += x[i] * x[i];
        }
        e12::power_spectrum(x, p);
        double two_sided = p.front() + p.back();
        for (size_t k = 1; k + 1 < p.size(); ++k)
            two_sided += 2.0 * p[k];
        double rel = std::fabs(two_sided - (double)e12::kWindow * sumsq) /
                     ((double)e12::kWindow * sumsq);
        char msg[96];
        std::snprintf(msg, sizeof msg, "Parseval holds to a relative %.2e", rel);
        expect(rel < 1e-9, msg);
    }

    // The band table covers 10 Hz to the Nyquist frequency with no gap and no overlap.
    {
        bool contiguous = true;
        for (int b = 1; b < e12::kBands; ++b)
            if (e12::kBandTable[(size_t)b].lo_hz != e12::kBandTable[(size_t)b - 1].hi_hz)
                contiguous = false;
        expect(contiguous && e12::kBandTable.back().hi_hz == e12::kSampleRateHz / 2.0,
               "the four bands are contiguous from 10 Hz to the Nyquist frequency");
    }

    std::printf("e12_bands_test: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
