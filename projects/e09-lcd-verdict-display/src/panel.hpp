// panel.hpp: the Waveshare 3.5 inch RPi LCD (A) as seen from the e09 loop. Two faces of
// one board share this header: the panel, three text panes composed into a DRM dumb
// buffer on the card whose driver is ili9486 and shown by a page flip; and the touch
// controller, the evdev node the ads7846 driver creates, polled for presses. The loop
// in main.cpp owns both and decides what the panes say; nothing here reads a sensor,
// runs a rule or asks the model.
//
// Both classes are optional at run time: with --headless the loop never constructs
// them, which is how the oracle test runs in CI. A build without libdrm (the CMake
// file says so at configure time) still compiles; the Panel constructor then throws.
#ifndef E09_PANEL_HPP
#define E09_PANEL_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace e09 {

// One pane: a title line in its own colour and a body that is word-wrapped to fit.
struct Pane {
    std::string title;
    std::string text;
    std::uint16_t colour;        // RGB565 for the title and the frame
};

std::uint16_t rgb565(int r, int g, int b);

class Panel {
public:
    // card: "/dev/dri/cardN", or "" to take the first card whose driver name is
    // ili9486. Throws std::runtime_error with the reason when no panel is found.
    explicit Panel(const std::string &card);
    ~Panel();
    Panel(const Panel &) = delete;
    Panel &operator=(const Panel &) = delete;

    int width() const { return w_; }
    int height() const { return h_; }
    const std::string &card() const { return card_; }

    // Composes the three panes into the back buffer: the enlarged pane across the top
    // half at twice the font size, the other two side by side below it.
    void compose(const Pane (&panes)[3], int enlarged);

    // Copies the back buffer into the next dumb buffer, asks for a page flip with an
    // event, waits for that event and returns the milliseconds from the call to the
    // event, read on std::chrono::steady_clock. The first call sets the mode instead.
    double flip();

private:
    struct Drm;                                   // the libdrm state, in panel.cpp
    void fill(int x, int y, int w, int h, std::uint16_t colour);
    void glyph(int x, int y, unsigned char c, std::uint16_t colour, int scale);
    void block(int x, int y, int w, int h, const Pane &pane, int scale);

    std::unique_ptr<Drm> drm_;
    std::string card_;
    int w_ = 0, h_ = 0;
    std::vector<std::uint16_t> frame_;            // the composed frame, RGB565
};

class Touch {
public:
    // node: "/dev/input/eventN" or a /dev/input/by-path link, or "" to take the first
    // event node whose name contains ADS7846. Throws std::runtime_error when none.
    explicit Touch(const std::string &node);
    ~Touch();
    Touch(const Touch &) = delete;
    Touch &operator=(const Touch &) = delete;

    const std::string &node() const { return node_; }
    const std::string &name() const { return name_; }

    // Drains the queued events without blocking and returns how many pen-down
    // presses (BTN_TOUCH going to 1) were among them. A press that arrived while the
    // model was deciding is still counted: evdev queues it.
    int poll();

private:
    int fd_ = -1;
    std::string node_;
    std::string name_;
};

} // namespace e09

#endif // E09_PANEL_HPP
