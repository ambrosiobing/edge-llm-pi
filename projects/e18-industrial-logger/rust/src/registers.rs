//! The register stream in sixty-four bytes: the frame the logger reads from the peer
//! over RS485, with explicit offsets, an explicit byte order and a checksum.
//!
//! The shape follows the joint node volume's rule and for the same reason: never put a
//! structure on a wire. The compiler on one side pads where the other does not, the two
//! sides disagree about byte order, and the failure appears as one field wrong and the
//! rest right, which is very hard to find from a bus trace. So the code below writes
//! and reads fixed offsets, and a test packs a frame, unpacks it and compares every
//! field including the boundary values of each type.
//!
//! ```text
//!  offset  bytes  field        meaning
//!       0      1  addr         the address the logger polled
//!       1      1  func         0x03, read holding registers
//!       2      1  count        56, the byte count of the register block
//!       3      1  seq          wraps at 256; a gap is a lost frame
//!       4     56  reg[0..28]   big endian u16 each; reg[0] is the level in counts
//!      60      2  flags        bit i set means reg[i] is meaningful
//!      62      2  crc          CRC-16/MODBUS over bytes 0 to 61, LOW byte first
//!                 total 64
//! ```
//!
//! Two deliberate asymmetries, both because the protocol says so rather than because
//! it is tidy. The registers are big endian, which is the register order of the
//! protocol this frame imitates. The checksum is little endian, because that protocol
//! transmits its CRC low byte first. A reader who assumes one byte order throughout
//! gets a checksum failure on every frame and suspects the wiring.
//!
//! On the bench there is no peer, so nothing here is ever fed by a real device. The
//! replay path of the service encodes each replayed row into a frame with `encode` and
//! reads it back with `decode`, so these offsets, this byte order and this checksum are
//! exercised on every row of the oracle run with no wire present. That is the whole
//! claim: the decoder is tested, the peer is absent.

/// Every frame is exactly this long. A serial read that returns fewer bytes has not
/// returned a frame, which is the pitfall the service's reader is written around.
pub const FRAME_LEN: usize = 64;

/// How many registers the block carries.
pub const N_REGISTERS: usize = 28;

/// The function code of a read holding registers response.
pub const FUNC_READ_HOLDING: u8 = 0x03;

/// The register block's byte count, which the frame also states at offset 2.
pub const REGISTER_BYTES: u8 = (N_REGISTERS * 2) as u8;

/// Offsets, named once so that no number appears twice in this file.
const OFF_ADDR: usize = 0;
const OFF_FUNC: usize = 1;
const OFF_COUNT: usize = 2;
const OFF_SEQ: usize = 3;
const OFF_REGS: usize = 4;
const OFF_FLAGS: usize = 60;
const OFF_CRC: usize = 62;

/// A decoded frame. The registers stay in their own unit, counts, as the peer sent
/// them: converting here would hide which number came off the wire.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Frame {
    pub addr: u8,
    pub func: u8,
    pub seq: u8,
    pub regs: [u16; N_REGISTERS],
    pub flags: u16,
}

/// Why a frame was refused. Every variant is a thing that happened on a real bus at
/// some point in somebody's life, and the service counts each separately, because
/// "the echo failed" is not a diagnosis and "nine short reads and one bad checksum"
/// is.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FrameError {
    /// Fewer than FRAME_LEN bytes arrived.
    Short(usize),
    /// More than FRAME_LEN bytes were handed in.
    Long(usize),
    /// The function code was not a read holding registers response.
    Func(u8),
    /// The stated register byte count was not REGISTER_BYTES.
    Count(u8),
    /// The checksum over bytes 0 to 61 did not match the two bytes at 62.
    Crc { want: u16, got: u16 },
}

impl core::fmt::Display for FrameError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            FrameError::Short(n) => write!(f, "short frame, {} of {} bytes", n, FRAME_LEN),
            FrameError::Long(n) => write!(f, "long frame, {} of {} bytes", n, FRAME_LEN),
            FrameError::Func(v) => write!(f, "function code 0x{:02x}, expected 0x{:02x}", v,
                                          FUNC_READ_HOLDING),
            FrameError::Count(v) => write!(f, "register byte count {}, expected {}", v,
                                           REGISTER_BYTES),
            FrameError::Crc { want, got } => {
                write!(f, "checksum 0x{:04x}, computed 0x{:04x}", got, want)
            }
        }
    }
}

/// CRC-16/MODBUS: reflected polynomial 0xa001, initial value 0xffff, no final xor.
/// Written out rather than taken from a crate, because it is nine lines, it is the
/// one thing a frame's acceptance depends on, and a dependency whose licence has to
/// be named in the chapter for nine lines is a poor trade.
pub fn crc16(bytes: &[u8]) -> u16 {
    let mut crc: u16 = 0xffff;
    for &b in bytes {
        crc ^= b as u16;
        for _ in 0..8 {
            if crc & 1 != 0 {
                crc = (crc >> 1) ^ 0xa001;
            } else {
                crc >>= 1;
            }
        }
    }
    crc
}

/// Writes the frame into sixty-four bytes, checksum last.
pub fn encode(frame: &Frame) -> [u8; FRAME_LEN] {
    let mut out = [0u8; FRAME_LEN];
    out[OFF_ADDR] = frame.addr;
    out[OFF_FUNC] = frame.func;
    out[OFF_COUNT] = REGISTER_BYTES;
    out[OFF_SEQ] = frame.seq;
    for (i, &r) in frame.regs.iter().enumerate() {
        let at = OFF_REGS + i * 2;
        out[at] = (r >> 8) as u8;
        out[at + 1] = (r & 0xff) as u8;
    }
    out[OFF_FLAGS] = (frame.flags >> 8) as u8;
    out[OFF_FLAGS + 1] = (frame.flags & 0xff) as u8;
    let crc = crc16(&out[..OFF_CRC]);
    out[OFF_CRC] = (crc & 0xff) as u8;
    out[OFF_CRC + 1] = (crc >> 8) as u8;
    out
}

/// Reads sixty-four bytes into a frame, or says exactly why not.
pub fn decode(bytes: &[u8]) -> Result<Frame, FrameError> {
    if bytes.len() < FRAME_LEN {
        return Err(FrameError::Short(bytes.len()));
    }
    if bytes.len() > FRAME_LEN {
        return Err(FrameError::Long(bytes.len()));
    }
    let want = crc16(&bytes[..OFF_CRC]);
    let got = bytes[OFF_CRC] as u16 | ((bytes[OFF_CRC + 1] as u16) << 8);
    if want != got {
        return Err(FrameError::Crc { want, got });
    }
    if bytes[OFF_FUNC] != FUNC_READ_HOLDING {
        return Err(FrameError::Func(bytes[OFF_FUNC]));
    }
    if bytes[OFF_COUNT] != REGISTER_BYTES {
        return Err(FrameError::Count(bytes[OFF_COUNT]));
    }
    let mut regs = [0u16; N_REGISTERS];
    for (i, r) in regs.iter_mut().enumerate() {
        let at = OFF_REGS + i * 2;
        *r = ((bytes[at] as u16) << 8) | bytes[at + 1] as u16;
    }
    Ok(Frame {
        addr: bytes[OFF_ADDR],
        func: bytes[OFF_FUNC],
        seq: bytes[OFF_SEQ],
        regs,
        flags: bytes[OFF_FLAGS + 1] as u16 | ((bytes[OFF_FLAGS] as u16) << 8),
    })
}

/// The request the logger writes before every response: address, function code,
/// starting register and count, with the same checksum. Eight bytes, and it is here
/// so that the live path and the echo test send the same bytes a real peer would be
/// asked by.
pub fn request(addr: u8, first: u16, n: u16) -> [u8; 8] {
    let mut out = [0u8; 8];
    out[0] = addr;
    out[1] = FUNC_READ_HOLDING;
    out[2] = (first >> 8) as u8;
    out[3] = (first & 0xff) as u8;
    out[4] = (n >> 8) as u8;
    out[5] = (n & 0xff) as u8;
    let crc = crc16(&out[..6]);
    out[6] = (crc & 0xff) as u8;
    out[7] = (crc >> 8) as u8;
    out
}

/// How many frames were lost between two sequence numbers, allowing for the wrap at
/// 256. The joint node volume's sequence rule: the master notices gaps, and a wrap is
/// not a gap.
pub fn seq_gap(previous: u8, current: u8) -> u32 {
    (current.wrapping_sub(previous) as u32).saturating_sub(1)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample() -> Frame {
        let mut regs = [0u16; N_REGISTERS];
        regs[0] = 577;
        regs[1] = 424;
        regs[2] = 58;
        regs[N_REGISTERS - 1] = 0xbeef;
        Frame { addr: 0x11, func: FUNC_READ_HOLDING, seq: 180, regs, flags: 0x0007 }
    }

    #[test]
    fn a_frame_is_sixty_four_bytes() {
        assert_eq!(FRAME_LEN, 64);
        assert_eq!(REGISTER_BYTES, 56);
        assert_eq!(OFF_REGS + N_REGISTERS * 2, OFF_FLAGS);
        assert_eq!(OFF_CRC + 2, FRAME_LEN);
        assert_eq!(encode(&sample()).len(), FRAME_LEN);
    }

    #[test]
    fn round_trip_keeps_every_field() {
        let f = sample();
        assert_eq!(decode(&encode(&f)).unwrap(), f);
    }

    #[test]
    fn round_trip_keeps_the_boundary_values() {
        let mut regs = [0xffffu16; N_REGISTERS];
        regs[1] = 0;
        let f = Frame { addr: 0xff, func: FUNC_READ_HOLDING, seq: 255, regs, flags: 0xffff };
        assert_eq!(decode(&encode(&f)).unwrap(), f);
        let g = Frame { addr: 0, func: FUNC_READ_HOLDING, seq: 0, regs: [0; N_REGISTERS],
                        flags: 0 };
        assert_eq!(decode(&encode(&g)).unwrap(), g);
    }

    #[test]
    fn the_registers_are_big_endian_and_the_checksum_is_not() {
        let bytes = encode(&sample());
        assert_eq!(bytes[OFF_REGS], 0x02);          // 577 = 0x0241, high byte first
        assert_eq!(bytes[OFF_REGS + 1], 0x41);
        let crc = crc16(&bytes[..OFF_CRC]);
        assert_eq!(bytes[OFF_CRC], (crc & 0xff) as u8);
        assert_eq!(bytes[OFF_CRC + 1], (crc >> 8) as u8);
    }

    #[test]
    fn one_flipped_bit_is_a_checksum_failure_and_not_a_field_error() {
        let mut bytes = encode(&sample());
        bytes[OFF_REGS] ^= 0x01;
        match decode(&bytes) {
            Err(FrameError::Crc { .. }) => {}
            other => panic!("expected a checksum failure, got {:?}", other),
        }
    }

    #[test]
    fn a_short_read_is_named_as_one() {
        let bytes = encode(&sample());
        assert_eq!(decode(&bytes[..FRAME_LEN - 1]), Err(FrameError::Short(FRAME_LEN - 1)));
        let mut long = bytes.to_vec();
        long.push(0);
        assert_eq!(decode(&long), Err(FrameError::Long(FRAME_LEN + 1)));
    }

    #[test]
    fn a_wrong_function_code_survives_the_checksum_and_is_still_refused() {
        let mut f = sample();
        f.func = 0x04;
        let mut bytes = encode(&f);
        bytes[OFF_FUNC] = 0x04;
        let crc = crc16(&bytes[..OFF_CRC]);
        bytes[OFF_CRC] = (crc & 0xff) as u8;
        bytes[OFF_CRC + 1] = (crc >> 8) as u8;
        assert_eq!(decode(&bytes), Err(FrameError::Func(0x04)));
    }

    #[test]
    fn the_checksum_holds_its_regression_vector() {
        // A regression vector, not a citation: these five bytes give 0x80b8 under the
        // loop above, cross-checked against a second implementation of the same
        // reflected polynomial written in Python on Wednesday 14 October 2026. On the
        // wire that is 0xb8 then 0x80, low byte first. Whether the protocol
        // specification's own worked example uses these bytes is to verify.
        assert_eq!(crc16(&[0x01, 0x04, 0x02, 0xff, 0xff]), 0x80b8);
    }

    #[test]
    fn a_sequence_wrap_is_not_a_gap() {
        assert_eq!(seq_gap(254, 255), 0);
        assert_eq!(seq_gap(255, 0), 0);
        assert_eq!(seq_gap(255, 3), 3);
        assert_eq!(seq_gap(10, 10), 0);
    }
}
