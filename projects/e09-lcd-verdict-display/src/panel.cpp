// panel.cpp: the compositor behind panel.hpp. Two dumb buffers on the card whose driver
// is ili9486, RGB565 because that is the format the panel's controller takes and the
// one the driver converts nothing for; one std::vector as the composed frame; a page
// flip with an event per shown frame; and the 8 by 16 bitmap font of font8x16.h.
//
// The sequence, in libdrm's names: open the primary node, drmGetVersion to check the
// driver name, drmModeGetResources, the first connected connector and its first mode,
// the encoder's CRTC, DRM_IOCTL_MODE_CREATE_DUMB twice, drmModeAddFB2 with
// DRM_FORMAT_RGB565, DRM_IOCTL_MODE_MAP_DUMB and mmap. The first flip() is a
// drmModeSetCrtc, which also makes this process the DRM master; every later one is a
// drmModePageFlip with DRM_MODE_PAGE_FLIP_EVENT, and flip() returns only after
// drmHandleEvent has delivered that event, so the time it reports covers the SPI
// transfer the tiny driver makes in its commit tail. Whether the event follows the
// transfer or precedes it is a property of the driver, and the chapter's acceptance
// test checks the figure against the arithmetic of a 2.46 Mbit frame for that reason.
//
// The DRM user-space interface is documented at
// https://www.kernel.org/doc/html/latest/gpu/drm-uapi.html (read Wednesday
// 14 October 2026); libdrm is MIT.
#include "panel.hpp"
#include "font8x16.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

#ifdef EDGELLM_HAVE_DRM
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#endif

namespace e09 {

std::uint16_t rgb565(int r, int g, int b)
{
    return (std::uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

namespace {

const std::uint16_t BACKGROUND = rgb565(11, 13, 18);      // the volume's page colour
const std::uint16_t INK = rgb565(242, 244, 248);
const int MARGIN = 4;

double ms_now()
{
    using clk = std::chrono::steady_clock;
    return std::chrono::duration<double, std::milli>(clk::now().time_since_epoch()).count();
}

} // namespace

#ifdef EDGELLM_HAVE_DRM

struct Panel::Drm {
    struct Buffer {
        std::uint32_t handle = 0, fb = 0, pitch = 0;
        std::uint64_t size = 0;
        std::uint8_t *map = nullptr;
    };
    int fd = -1;
    std::uint32_t crtc = 0, conn = 0;
    drmModeModeInfo mode;
    Buffer buf[2];
    int back = 0;
    bool shown = false;
    bool flipped = false;
};

namespace {

// Opens the card at path when its driver is ili9486, else returns -1.
int open_if_ili9486(const std::string &path, std::string &name)
{
    int fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) return -1;
    drmVersionPtr v = drmGetVersion(fd);
    bool ok = v != nullptr && v->name != nullptr && std::strcmp(v->name, "ili9486") == 0;
    if (v != nullptr) {
        name = v->name != nullptr ? v->name : "";
        drmFreeVersion(v);
    }
    if (!ok) {
        close(fd);
        return -1;
    }
    return fd;
}

void page_flip_handler(int, unsigned int, unsigned int, unsigned int, void *user)
{
    *static_cast<bool *>(user) = true;
}

} // namespace

Panel::Panel(const std::string &card) : drm_(new Drm)
{
    std::string name;
    if (!card.empty()) {
        drm_->fd = open_if_ili9486(card, name);
        if (drm_->fd < 0)
            throw std::runtime_error(card + " is not the ili9486 panel (driver " +
                                     (name.empty() ? "unknown" : name) + ")");
        card_ = card;
    } else {
        for (int n = 0; n < 8 && drm_->fd < 0; ++n) {
            std::string path = "/dev/dri/card" + std::to_string(n);
            drm_->fd = open_if_ili9486(path, name);
            if (drm_->fd >= 0) card_ = path;
        }
        if (drm_->fd < 0)
            throw std::runtime_error("no /dev/dri/card* has the ili9486 driver; is the "
                                     "panel overlay loaded?");
    }

    drmModeResPtr res = drmModeGetResources(drm_->fd);
    if (res == nullptr) throw std::runtime_error(card_ + ": drmModeGetResources failed");
    drmModeConnectorPtr conn = nullptr;
    for (int i = 0; i < res->count_connectors && conn == nullptr; ++i) {
        drmModeConnectorPtr c = drmModeGetConnector(drm_->fd, res->connectors[i]);
        if (c != nullptr && c->connection == DRM_MODE_CONNECTED && c->count_modes > 0)
            conn = c;
        else if (c != nullptr)
            drmModeFreeConnector(c);
    }
    if (conn == nullptr) {
        drmModeFreeResources(res);
        throw std::runtime_error(card_ + ": no connected connector with a mode");
    }
    drm_->conn = conn->connector_id;
    drm_->mode = conn->modes[0];
    w_ = drm_->mode.hdisplay;
    h_ = drm_->mode.vdisplay;
    drmModeEncoderPtr enc = conn->encoder_id != 0 ? drmModeGetEncoder(drm_->fd, conn->encoder_id)
                                                  : nullptr;
    if (enc == nullptr && conn->count_encoders > 0)
        enc = drmModeGetEncoder(drm_->fd, conn->encoders[0]);
    if (enc != nullptr && enc->crtc_id != 0) {
        drm_->crtc = enc->crtc_id;
    } else if (enc != nullptr) {
        for (int i = 0; i < res->count_crtcs; ++i)
            if (enc->possible_crtcs & (1u << i)) { drm_->crtc = res->crtcs[i]; break; }
    }
    if (enc != nullptr) drmModeFreeEncoder(enc);
    drmModeFreeConnector(conn);
    drmModeFreeResources(res);
    if (drm_->crtc == 0) throw std::runtime_error(card_ + ": no CRTC for the connector");

    for (Drm::Buffer &b : drm_->buf) {
        drm_mode_create_dumb creq;
        std::memset(&creq, 0, sizeof creq);
        creq.width = (std::uint32_t)w_;
        creq.height = (std::uint32_t)h_;
        creq.bpp = 16;
        if (drmIoctl(drm_->fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0)
            throw std::runtime_error(card_ + ": DRM_IOCTL_MODE_CREATE_DUMB failed");
        b.handle = creq.handle;
        b.pitch = creq.pitch;
        b.size = creq.size;
        std::uint32_t handles[4] = { b.handle, 0, 0, 0 }, pitches[4] = { b.pitch, 0, 0, 0 };
        std::uint32_t offsets[4] = { 0, 0, 0, 0 };
        if (drmModeAddFB2(drm_->fd, (std::uint32_t)w_, (std::uint32_t)h_, DRM_FORMAT_RGB565,
                          handles, pitches, offsets, &b.fb, 0) != 0)
            throw std::runtime_error(card_ + ": drmModeAddFB2 with RGB565 failed");
        drm_mode_map_dumb mreq;
        std::memset(&mreq, 0, sizeof mreq);
        mreq.handle = b.handle;
        if (drmIoctl(drm_->fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) < 0)
            throw std::runtime_error(card_ + ": DRM_IOCTL_MODE_MAP_DUMB failed");
        void *p = mmap(nullptr, (size_t)b.size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_->fd,
                       (off_t)mreq.offset);
        if (p == MAP_FAILED) throw std::runtime_error(card_ + ": mmap of a dumb buffer failed");
        b.map = static_cast<std::uint8_t *>(p);
        std::memset(b.map, 0, (size_t)b.size);
    }
    frame_.assign((size_t)w_ * (size_t)h_, BACKGROUND);
}

Panel::~Panel()
{
    if (!drm_) return;
    for (Drm::Buffer &b : drm_->buf) {
        if (b.map != nullptr) munmap(b.map, (size_t)b.size);
        if (b.fb != 0) drmModeRmFB(drm_->fd, b.fb);
        if (b.handle != 0) {
            drm_mode_destroy_dumb dreq;
            std::memset(&dreq, 0, sizeof dreq);
            dreq.handle = b.handle;
            drmIoctl(drm_->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        }
    }
    if (drm_->fd >= 0) close(drm_->fd);
}

double Panel::flip()
{
    double t0 = ms_now();
    Drm::Buffer &b = drm_->buf[drm_->back];
    for (int y = 0; y < h_; ++y)
        std::memcpy(b.map + (size_t)y * b.pitch, &frame_[(size_t)y * (size_t)w_],
                    (size_t)w_ * sizeof(std::uint16_t));
    if (!drm_->shown) {
        if (drmModeSetCrtc(drm_->fd, drm_->crtc, b.fb, 0, 0, &drm_->conn, 1, &drm_->mode) != 0)
            throw std::runtime_error(card_ + ": drmModeSetCrtc failed (is another DRM "
                                     "master open, or fbcon still bound?)");
        drm_->shown = true;
    } else {
        drm_->flipped = false;
        if (drmModePageFlip(drm_->fd, drm_->crtc, b.fb, DRM_MODE_PAGE_FLIP_EVENT,
                            &drm_->flipped) != 0)
            throw std::runtime_error(card_ + ": drmModePageFlip failed");
        drmEventContext ctx;
        std::memset(&ctx, 0, sizeof ctx);
        ctx.version = DRM_EVENT_CONTEXT_VERSION;
        ctx.page_flip_handler = page_flip_handler;
        while (!drm_->flipped) {
            struct pollfd pfd = { drm_->fd, POLLIN, 0 };
            if (poll(&pfd, 1, 2000) <= 0)
                throw std::runtime_error(card_ + ": no page flip event within 2 s");
            drmHandleEvent(drm_->fd, &ctx);
        }
    }
    drm_->back ^= 1;
    return ms_now() - t0;
}

#else // no libdrm at build time: the loop still runs with --headless

struct Panel::Drm {};

Panel::Panel(const std::string &)
{
    throw std::runtime_error("edgellm-e09 was built without libdrm; run with --headless, "
                             "or install libdrm-dev and configure again");
}

Panel::~Panel() = default;

double Panel::flip() { return -1.0; }

#endif

// ------------------------------------------------------------------ the compositor

void Panel::fill(int x, int y, int w, int h, std::uint16_t colour)
{
    for (int j = std::max(0, y); j < std::min(h_, y + h); ++j)
        for (int i = std::max(0, x); i < std::min(w_, x + w); ++i)
            frame_[(size_t)j * (size_t)w_ + (size_t)i] = colour;
}

void Panel::glyph(int x, int y, unsigned char c, std::uint16_t colour, int scale)
{
    if (c < FONT_FIRST || c > FONT_LAST) c = '?';
    const unsigned char *rows = font8x16[c - FONT_FIRST];
    for (int r = 0; r < FONT_H; ++r)
        for (int b = 0; b < FONT_W; ++b)
            if (rows[r] & (0x80 >> b))
                fill(x + b * scale, y + r * scale, scale, scale, colour);
}

// One pane in a rectangle: a frame in the pane's colour, the title on the first line,
// the body word-wrapped below it, as many lines as fit. A newline in the body starts a
// new line; a word longer than a line is cut.
void Panel::block(int x, int y, int w, int h, const Pane &pane, int scale)
{
    const int cw = FONT_W * scale, ch = FONT_H * scale;
    const int cols = (w - 2 * MARGIN) / cw, rows = (h - 2 * MARGIN) / ch;
    fill(x, y, w, h, BACKGROUND);
    fill(x, y, w, 1, pane.colour);
    fill(x, y + h - 1, w, 1, pane.colour);
    fill(x, y, 1, h, pane.colour);
    fill(x + w - 1, y, 1, h, pane.colour);
    if (cols <= 0 || rows <= 0) return;
    int line = 0, col = 0;
    auto put = [&](const std::string &word) {
        if (line >= rows) return;
        if (col > 0 && col + (int)word.size() > cols) { ++line; col = 0; }
        for (char c : word) {
            if (line >= rows) return;
            if (col >= cols) { ++line; col = 0; if (line >= rows) return; }
            glyph(x + MARGIN + col * cw, y + MARGIN + line * ch, (unsigned char)c,
                  line == 0 ? pane.colour : INK, scale);
            ++col;
        }
        if (col < cols) ++col;                        // the space after a word
    };
    put(pane.title);
    ++line;
    col = 0;
    std::string word;
    for (char c : pane.text) {
        if (c == ' ' || c == '\n') {
            if (!word.empty()) put(word);
            word.clear();
            if (c == '\n') { ++line; col = 0; }
        } else {
            word.push_back(c);
        }
    }
    if (!word.empty()) put(word);
}

void Panel::compose(const Pane (&panes)[3], int enlarged)
{
    if (w_ <= 0 || h_ <= 0) return;
    const int top = h_ / 2;
    block(0, 0, w_, top, panes[enlarged % 3], 2);
    int k = 0;
    for (int i = 0; i < 3; ++i) {
        if (i == enlarged % 3) continue;
        block(k * (w_ / 2), top, w_ / 2, h_ - top, panes[i], 1);
        ++k;
    }
}

} // namespace e09
