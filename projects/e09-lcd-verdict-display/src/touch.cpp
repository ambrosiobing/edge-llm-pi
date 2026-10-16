// touch.cpp: the ads7846 touch controller of the panel as an evdev node. The driver
// names its input device "ADS7846 Touchscreen" (drivers/input/touchscreen/ads7846.c,
// read Wednesday 14 October 2026: snprintf(ts->name, ..., "ADS%d Touchscreen",
// ts->model)) and reports BTN_TOUCH with ABS_X, ABS_Y and ABS_PRESSURE. This project
// uses only BTN_TOUCH going to 1, the pen-down edge, as the signal that the enlarged
// pane changes; where the finger landed is not read, so no calibration matrix is
// needed for the acceptance test, and the X and Y values stay in the queue for a
// later exercise that selects a pane by position.
//
// The node is found by name through EVIOCGNAME over /dev/input/event0 to event31, or
// given with --touch, for example a /dev/input/by-path link. It is opened
// non-blocking and drained on every poll(), so a press that arrived while the model
// was deciding is counted after the decision rather than lost.
#include "panel.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace e09 {

namespace {

int open_event(const std::string &path, std::string &name)
{
    int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return -1;
    char buf[128] = { 0 };
    if (ioctl(fd, EVIOCGNAME(sizeof buf - 1), buf) < 0) {
        close(fd);
        return -1;
    }
    name = buf;
    return fd;
}

} // namespace

Touch::Touch(const std::string &node)
{
    if (!node.empty()) {
        fd_ = open_event(node, name_);
        if (fd_ < 0) throw std::runtime_error("cannot open the touch node " + node);
        node_ = node;
        return;
    }
    for (int n = 0; n < 32 && fd_ < 0; ++n) {
        std::string path = "/dev/input/event" + std::to_string(n);
        std::string name;
        int fd = open_event(path, name);
        if (fd < 0) continue;
        if (name.find("ADS7846") != std::string::npos) {
            fd_ = fd;
            node_ = path;
            name_ = name;
        } else {
            close(fd);
        }
    }
    if (fd_ < 0)
        throw std::runtime_error("no /dev/input/event* is named ADS7846; is the touch "
                                 "controller bound, and is this process allowed to read it?");
}

Touch::~Touch()
{
    if (fd_ >= 0) close(fd_);
}

int Touch::poll()
{
    int presses = 0;
    struct input_event events[32];
    for (;;) {
        ssize_t n = read(fd_, events, sizeof events);
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) break;
            throw std::runtime_error(node_ + ": read failed: " + std::strerror(errno));
        }
        if (n == 0) break;
        size_t count = (size_t)n / sizeof(struct input_event);
        for (size_t i = 0; i < count; ++i)
            if (events[i].type == EV_KEY && events[i].code == BTN_TOUCH && events[i].value == 1)
                ++presses;
        if (count < sizeof events / sizeof events[0]) break;
    }
    return presses;
}

} // namespace e09
