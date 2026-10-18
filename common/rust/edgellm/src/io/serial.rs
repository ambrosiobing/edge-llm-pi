//! io::serial: one serial line over termios, for the project that owns its own port.
//!
//! Written on Sunday 18 October 2026 for e18, whose service drives RS232 and two RS485
//! channels on a Waveshare WS-28164 and whose header has named this interface since
//! Wednesday 14 October 2026. It is behind the "serial" cargo feature so that e05, e13 and
//! e14, which have no serial line, keep their dependency set: they pull this crate with
//! default-features = false and depend on nothing else, and a termios port needs libc.
//!
//! open(), the baud map and the raw-mode setup mirror common/c/src/io_serial.c call for
//! call, in the same order with the same flags: open with O_RDWR | O_NOCTTY, tcgetattr,
//! cfmakeraw, both speeds, CLOCAL | CREAD, VMIN 1 and VTIME 0, tcsetattr TCSANOW, then
//! tcflush TCIFLUSH. The eight rates are the eight that file accepts, and a rate outside
//! them is refused here as it is there rather than being silently approximated.
//!
//! TWO PLACES WHERE THIS IS NOT A MIRROR, because the C file has nothing to mirror.
//!
//! set_rs485 has no counterpart: common/c/src/io_serial.c mentions RS485 nowhere, because
//! the C projects that use a serial line use RS232. The ioctl request comes from libc so
//! that it is not a number written from memory, and the two flag bits were read off the
//! board's own linux/serial.h on Sunday 18 October 2026 rather than recalled. What is still
//! unknown is one layer down and cannot be settled by a header: whether the driver behind
//! a given port implements TIOCSRS485 at all. The WS-28164's SC16IS752 channels have not
//! been tried, a UART that does direction control in hardware may answer ENOTTY and work
//! anyway, and that is why the errno is returned rather than swallowed.
//!
//! read_exact_timeout has no counterpart either: the C backend sets VMIN 1 and VTIME 0 and
//! blocks until a byte arrives, which is right for a reader that has nothing else to do.
//! A request-and-reply service needs a deadline, so this polls for the remainder of the
//! timeout and returns how many bytes arrived. The name is inherited from e18's header and
//! is a little generous: a short read is Ok(n) and the caller decodes the n bytes it got,
//! which is what e18 does, while a deadline that passes with nothing read is TimedOut
//! rather than Ok(0), so an echo that never came back is counted as the failure it is.
//!
//! Nothing has run this module. The baud map and the two refusal paths have tests; write_all
//! and read_exact_timeout are compiled and unexercised, because exercising them needs a
//! line. The first thing to run them is the Pi 4 Model B, card B, with the HAT fitted.

use std::ffi::CString;
use std::io;
use std::os::unix::ffi::OsStrExt;
use std::path::Path;
use std::time::{Duration, Instant};

/// From linux/serial.h, CONFIRMED on Sunday 18 October 2026 against
/// /usr/include/linux/serial.h on the card eplepi, which carries
///     #define SER_RS485_ENABLED        _BITUL(0)
///     #define SER_RS485_RX_DURING_TX   _BITUL(4)
/// so the two values below are the target's own and not a recollection.
const SER_RS485_ENABLED: u32 = 1 << 0;
const SER_RS485_RX_DURING_TX: u32 = 1 << 4;

/// struct serial_rs485 from linux/serial.h. Declared here because the libc crate does not
/// carry it; the ioctl request itself comes from libc below.
#[repr(C)]
#[derive(Default)]
struct SerialRs485 {
    flags: u32,
    delay_rts_before_send: u32,
    delay_rts_after_send: u32,
    padding: [u32; 5],
}

/// The eight rates common/c/src/io_serial.c accepts, and nothing else. A rate it would
/// refuse is refused here, so that the two trees agree about what is a supported line
/// rather than one of them rounding.
fn baud_code(baud: u32) -> Option<libc::speed_t> {
    Some(match baud {
        9600 => libc::B9600,
        19200 => libc::B19200,
        38400 => libc::B38400,
        57600 => libc::B57600,
        115200 => libc::B115200,
        230400 => libc::B230400,
        460800 => libc::B460800,
        921600 => libc::B921600,
        _ => return None,
    })
}

/// One open serial line. The file descriptor is closed on drop; the type is neither Clone
/// nor Copy, so the descriptor has one owner and is closed once.
pub struct Port {
    fd: libc::c_int,
}

impl Port {
    /// Open path at baud in raw mode, in the order common/c/src/io_serial.c uses.
    pub fn open(path: impl AsRef<Path>, baud: u32) -> io::Result<Port> {
        let path = path.as_ref();
        let code = match baud_code(baud) {
            Some(c) => c,
            None => {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    format!("serial: {} baud is not one of the eight supported rates", baud),
                ));
            }
        };
        let c_path = CString::new(path.as_os_str().as_bytes()).map_err(|_| {
            io::Error::new(io::ErrorKind::InvalidInput, "serial: path holds a NUL byte")
        })?;

        let fd = unsafe { libc::open(c_path.as_ptr(), libc::O_RDWR | libc::O_NOCTTY) };
        if fd < 0 {
            return Err(io::Error::last_os_error());
        }
        // From here on every early return closes the descriptor, which is what the C file's
        // close-and-free ladder does on each of its three failure paths.
        let port = Port { fd };

        let mut tio: libc::termios = unsafe { std::mem::zeroed() };
        if unsafe { libc::tcgetattr(port.fd, &mut tio) } != 0 {
            return Err(io::Error::last_os_error());
        }
        unsafe {
            libc::cfmakeraw(&mut tio);
            libc::cfsetispeed(&mut tio, code);
            libc::cfsetospeed(&mut tio, code);
        }
        tio.c_cflag |= libc::CLOCAL | libc::CREAD;
        tio.c_cc[libc::VMIN] = 1;
        tio.c_cc[libc::VTIME] = 0;
        if unsafe { libc::tcsetattr(port.fd, libc::TCSANOW, &tio) } != 0 {
            return Err(io::Error::last_os_error());
        }
        unsafe { libc::tcflush(port.fd, libc::TCIFLUSH) };
        Ok(port)
    }

    /// Turn on RS485 half duplex, optionally keeping the receiver on while transmitting.
    ///
    /// Whether the driver behind this port implements the ioctl at all is a property of
    /// that driver: a UART that does direction control in hardware may answer ENOTTY and
    /// still work, and the WS-28164's SC16IS752 channels have not been tried. The errno is
    /// returned rather than swallowed so that the caller can say which of the two happened.
    pub fn set_rs485(&mut self, rx_during_tx: bool) -> io::Result<()> {
        let mut conf = SerialRs485::default();
        conf.flags = SER_RS485_ENABLED;
        if rx_during_tx {
            conf.flags |= SER_RS485_RX_DURING_TX;
        }
        let rc = unsafe {
            libc::ioctl(self.fd, libc::TIOCSRS485, &mut conf as *mut SerialRs485)
        };
        if rc != 0 {
            return Err(io::Error::last_os_error());
        }
        Ok(())
    }

    /// Write the whole buffer, continuing through a short write and through EINTR.
    pub fn write_all(&mut self, buf: &[u8]) -> io::Result<()> {
        let mut sent = 0usize;
        while sent < buf.len() {
            let n = unsafe {
                libc::write(
                    self.fd,
                    buf[sent..].as_ptr() as *const libc::c_void,
                    buf.len() - sent,
                )
            };
            if n < 0 {
                let e = io::Error::last_os_error();
                if e.kind() == io::ErrorKind::Interrupted {
                    continue;
                }
                return Err(e);
            }
            if n == 0 {
                return Err(io::Error::new(
                    io::ErrorKind::WriteZero,
                    "serial: the port accepted no bytes",
                ));
            }
            sent += n as usize;
        }
        Ok(())
    }

    /// Read until buf is full or the timeout passes, and return how many bytes arrived.
    /// Nothing at all by the deadline is TimedOut; something but not everything is Ok(n).
    pub fn read_exact_timeout(
        &mut self,
        buf: &mut [u8],
        timeout: Duration,
    ) -> io::Result<usize> {
        let deadline = Instant::now() + timeout;
        let mut got = 0usize;
        while got < buf.len() {
            let left = deadline.saturating_duration_since(Instant::now());
            if left.is_zero() {
                break;
            }
            let ms = left.as_millis().min(libc::c_int::MAX as u128) as libc::c_int;
            let mut pfd = libc::pollfd { fd: self.fd, events: libc::POLLIN, revents: 0 };
            let rc = unsafe { libc::poll(&mut pfd, 1, ms) };
            if rc < 0 {
                let e = io::Error::last_os_error();
                if e.kind() == io::ErrorKind::Interrupted {
                    continue;
                }
                return Err(e);
            }
            if rc == 0 {
                break; // the deadline passed with the line quiet
            }
            let n = unsafe {
                libc::read(
                    self.fd,
                    buf[got..].as_mut_ptr() as *mut libc::c_void,
                    buf.len() - got,
                )
            };
            if n < 0 {
                let e = io::Error::last_os_error();
                if e.kind() == io::ErrorKind::Interrupted {
                    continue;
                }
                return Err(e);
            }
            if n == 0 {
                break; // the other end closed
            }
            got += n as usize;
        }
        if got == 0 {
            return Err(io::Error::new(
                io::ErrorKind::TimedOut,
                "serial: nothing arrived before the deadline",
            ));
        }
        Ok(got)
    }
}

impl Drop for Port {
    fn drop(&mut self) {
        unsafe { libc::close(self.fd) };
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_eight_rates_of_the_c_backend_are_the_eight_accepted() {
        for baud in [9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600] {
            assert!(baud_code(baud).is_some(), "{} should be accepted", baud);
        }
    }

    #[test]
    fn a_rate_the_c_backend_refuses_is_refused_here() {
        // The other direction: a map that answered Some for everything would pass the test
        // above and approximate a rate the C tree would have rejected.
        for baud in [0, 1200, 100_000, 115_201, 1_000_000] {
            assert!(baud_code(baud).is_none(), "{} should be refused", baud);
        }
    }

    #[test]
    fn an_unsupported_rate_is_refused_before_the_device_is_opened() {
        // /dev/null is not a terminal, so a port that got as far as tcgetattr would fail
        // with a different error. InvalidInput proves the rate was checked first.
        let e = Port::open("/dev/null", 1200).unwrap_err();
        assert_eq!(e.kind(), io::ErrorKind::InvalidInput);
    }

    #[test]
    fn a_path_that_is_not_there_reports_the_os_error() {
        let e = Port::open("/dev/there-is-no-such-port-here", 115_200).unwrap_err();
        assert_eq!(e.kind(), io::ErrorKind::NotFound);
    }
}
