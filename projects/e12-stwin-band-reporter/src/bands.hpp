// bands.hpp: the window, the transform and the band energies of project e12. One window is
// 4096 samples of one axis at the stream's rate (0.683 s at 6 kHz); each axis has its mean
// removed (gravity and offset), is multiplied by a Hann window and transformed; the one-sided
// power spectrum is summed into four bands and the three axes are added. Nothing here knows
// about the probe, the port or the model.
//
// The transform is pocketfft (BSD 3-clause, header only) when the build found
// third_party/pocketfft/pocketfft_hdronly.h; otherwise the radix-2 transform at the end of
// bands.cpp, which handles the power-of-two length this project uses and nothing else.
// fft_backend() names which one was compiled in, and the log's engine column does not:
// read it from the program's start-up line.
#ifndef E12_BANDS_HPP
#define E12_BANDS_HPP

#include <array>
#include <vector>

namespace e12 {

constexpr int    kWindow       = 4096;     // samples per window and per axis
constexpr double kSampleRateHz = 6000.0;   // the rate the stream is expected to run at
constexpr int    kBands        = 4;

struct Band {
    const char *name;
    double      lo_hz;   // included
    double      hi_hz;   // excluded
};

// The four bands, from 10 Hz to the Nyquist frequency, in order: hand and mounting motion;
// the band a phone's vibration motor and most small rotating parts land in; impulsive
// ringing from a tap; the enclosure and desk resonances, where a dropped frame shows first.
extern const std::array<Band, kBands> kBandTable;

// Index of the band the rule watches (the ratio of its energy to the total).
constexpr int kWatchedBand = 1;

struct BandEnergies {
    std::array<double, kBands> e{};   // sum of |X_k|^2 over the bins of each band
    double total = 0.0;               // the four bands together
    double share(int band) const { return total > 0.0 ? e[band] / total : 0.0; }
};

// Symmetric Hann window of n points.
std::vector<double> hann(int n);

// One-sided power spectrum |X_k|^2 for k = 0 .. n/2 of a real sequence of even length n.
// No scaling: the full two-sided sum equals n times the sum of squares (Parseval), which
// the tone test checks on whichever backend is compiled in.
void power_spectrum(const std::vector<double> &x, std::vector<double> &p);

// Sum the bins of a one-sided spectrum at sample rate fs_hz into the band table.
BandEnergies band_energies(const std::vector<double> &p, double fs_hz);

// Three axes of one window -> the band energies summed over the axes. Each axis must hold
// exactly kWindow samples. Mean removed, Hann applied.
BandEnergies window_energies(const std::vector<float> &ax, const std::vector<float> &ay,
                             const std::vector<float> &az, double fs_hz);

const char *fft_backend();

} // namespace e12

#endif // E12_BANDS_HPP
