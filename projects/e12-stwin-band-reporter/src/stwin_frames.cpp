// stwin_frames.cpp: see stwin_frames.hpp for the frame form and the TO VERIFY mark.
#include "stwin_frames.hpp"
#include "bands.hpp"

#include <cstdlib>
#include <cstring>

namespace e12 {

bool parse_frame_line(const char *line, Frame &f)
{
    double v[kFrameValues];
    int n = 0;
    const char *p = line;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p != 'S' || p[1] != ',') return false;
    p += 2;
    while (n < kFrameValues) {
        char *end = nullptr;
        v[n] = std::strtod(p, &end);
        if (end == p) return false;
        ++n;
        p = end;
        if (*p == ',') ++p;
        else break;
    }
    if (n != kFrameValues) return false;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p != '\0') return false;                 // trailing junk is not a frame
    f.seq = (unsigned long)v[0];
    f.t_us = v[1];
    for (int i = 0; i < kSamplesPerFrame; ++i) {
        f.ax[i] = (float)v[2 + 3 * i];
        f.ay[i] = (float)v[3 + 3 * i];
        f.az[i] = (float)v[4 + 3 * i];
    }
    return true;
}

bool frame_from_event(const io_event &ev, Frame &f)
{
    if (ev.n != kFrameValues) return false;
    f.seq = (unsigned long)ev.v[0];
    f.t_us = ev.v[1];
    for (int i = 0; i < kSamplesPerFrame; ++i) {
        f.ax[i] = (float)ev.v[2 + 3 * i];
        f.ay[i] = (float)ev.v[3 + 3 * i];
        f.az[i] = (float)ev.v[4 + 3 * i];
    }
    return true;
}

double Window::fs_hz() const
{
    const double span_s = (t1_us - t0_us) * 1e-6;
    const double intervals = (double)(ax.size() - (size_t)kSamplesPerFrame);
    return span_s > 0.0 ? intervals / span_s : 0.0;
}

bool WindowBuilder::push(const Frame &f, Window &out)
{
    if (have_prev_) {
        const unsigned long expected = prev_seq_ + 1;
        if (f.seq != expected) {
            const unsigned long gap = f.seq > expected ? f.seq - expected : 1;
            cur_.dropped += (int)gap;
            dropped_ += gap;
        }
    }
    prev_seq_ = f.seq;
    have_prev_ = true;
    ++frames_;

    if (cur_.ax.empty()) {
        cur_.t0_us = f.t_us;
        cur_.first_seq = f.seq;
        cur_.ax.reserve((size_t)kWindow);
        cur_.ay.reserve((size_t)kWindow);
        cur_.az.reserve((size_t)kWindow);
    }
    for (int i = 0; i < kSamplesPerFrame; ++i) {
        cur_.ax.push_back(f.ax[i]);
        cur_.ay.push_back(f.ay[i]);
        cur_.az.push_back(f.az[i]);
    }
    cur_.t1_us = f.t_us;
    cur_.last_seq = f.seq;
    if (cur_.ax.size() < (size_t)kWindow) return false;

    out = std::move(cur_);
    cur_ = Window();
    return true;
}

} // namespace e12
