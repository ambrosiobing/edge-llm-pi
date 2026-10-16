// stwin_frames.hpp: the frame parser and the window builder for the STWIN.box CDC stream.
//
// The frame form this parser reads is defined here, by this project, because kit lab P04
// fixes no frame layout: it names the firmware that enumerates as CDC-ACM (the Zephyr
// sensors sample for steval_stwinbx1, whose default console is the CDC-ACM port, verified
// Wednesday 14 October 2026) and leaves the line format to the firmware build. The form
// below is therefore TO VERIFY against the firmware actually flashed, and the parser is the
// one file to change if the firmware prints something else. One text line per frame, four
// samples of three axes in milli-g integers, a sequence number and the timestamp of the
// frame's first sample in microseconds:
//
//     S,<seq>,<t_us>,<ax0>,<ay0>,<az0>,<ax1>,<ay1>,<az1>,<ax2>,<ay2>,<az2>,<ax3>,<ay3>,<az3>
//
// Four samples per line keeps a line at about 80 characters and 1500 lines per second at
// 6 kHz, and keeps the 14 numbers inside IO_MAX_VALUES (16), so the shared io_serial
// backend delivers one frame per io_event with no change to common/.
//
// The sequence number is the cheapest instrument in the chapter: a gap is a dropped frame,
// and a window that contains one is counted and discarded, never transformed, because a
// hole in the samples is a broadband artefact that reads like a real event.
#ifndef E12_STWIN_FRAMES_HPP
#define E12_STWIN_FRAMES_HPP

#include "io.h"

#include <vector>

namespace e12 {

constexpr int kSamplesPerFrame = 4;
constexpr int kFrameValues     = 2 + 3 * kSamplesPerFrame;   // seq, t_us, then x y z per sample

struct Frame {
    unsigned long seq = 0;
    double        t_us = 0.0;                 // the first sample's timestamp, firmware clock
    float ax[kSamplesPerFrame] = { 0, 0, 0, 0 };
    float ay[kSamplesPerFrame] = { 0, 0, 0, 0 };
    float az[kSamplesPerFrame] = { 0, 0, 0, 0 };
};

// A text line in the form above -> a frame. Returns false on anything else.
bool parse_frame_line(const char *line, Frame &f);

// An io_event from the serial backend (14 numbers in v[]) -> a frame. Returns false
// when the event does not carry exactly kFrameValues numbers.
bool frame_from_event(const io_event &ev, Frame &f);

struct Window {
    std::vector<float> ax, ay, az;            // kWindow samples each
    double        t0_us = 0.0;                // the first frame's timestamp
    double        t1_us = 0.0;                // the last frame's timestamp
    unsigned long first_seq = 0, last_seq = 0;
    int           dropped = 0;                // frames missing inside this window
    // The rate measured from the firmware's own timestamps: the sample intervals between
    // the first and the last frame's first sample, over the span between them.
    double fs_hz() const;
};

class WindowBuilder {
public:
    // Adds one frame. Returns true when a window is complete and has been moved into out.
    bool push(const Frame &f, Window &out);
    unsigned long frames() const { return frames_; }
    unsigned long dropped_total() const { return dropped_; }

private:
    Window        cur_;
    bool          have_prev_ = false;
    unsigned long prev_seq_ = 0;
    unsigned long frames_ = 0, dropped_ = 0;
};

} // namespace e12

#endif // E12_STWIN_FRAMES_HPP
