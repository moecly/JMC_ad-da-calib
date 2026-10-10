//! 上位机→MCU 下行帧编码，对齐固件 calib/CalibLink.cpp 的 [0xA5,len,payload,crc32(LE)] 布局。

pub const OP_HOST_CONFIG: u8 = 0x10;
pub const OP_HOST_START: u8 = 0x11;
pub const OP_HOST_PAUSE: u8 = 0x12;
pub const OP_HOST_RESUME: u8 = 0x13;
pub const OP_HOST_RESTART: u8 = 0x14;
pub const OP_HOST_STOP: u8 = 0x15;
pub const OP_HOST_SET_DA: u8 = 0x16;
pub const OP_HOST_READ_AD: u8 = 0x17;
pub const OP_HOST_STATUS: u8 = 0x18;

const FRAME_HEAD: u8 = 0xA5;
const MAX_PAYLOAD: usize = 26;

pub fn frame(payload: &[u8]) -> Vec<u8> {
    debug_assert!(payload.len() <= MAX_PAYLOAD);
    let mut out = Vec::with_capacity(2 + payload.len() + 4);
    out.push(FRAME_HEAD);
    out.push(payload.len() as u8);
    out.extend_from_slice(payload);
    out.extend_from_slice(&crc32fast::hash(payload).to_le_bytes());
    out
}

pub fn config(mask: u8, da: [u8; 4]) -> Vec<u8> {
    frame(&[OP_HOST_CONFIG, mask, da[0], da[1], da[2], da[3]])
}

pub fn start() -> Vec<u8> {
    frame(&[OP_HOST_START])
}

pub fn pause() -> Vec<u8> {
    frame(&[OP_HOST_PAUSE])
}

pub fn resume() -> Vec<u8> {
    frame(&[OP_HOST_RESUME])
}

pub fn restart() -> Vec<u8> {
    frame(&[OP_HOST_RESTART])
}

pub fn stop() -> Vec<u8> {
    frame(&[OP_HOST_STOP])
}

pub fn set_da(ch: u8, ideal: u16) -> Vec<u8> {
    frame(&[OP_HOST_SET_DA, ch, (ideal & 0xFF) as u8, (ideal >> 8) as u8])
}

pub fn read_ad() -> Vec<u8> {
    frame(&[OP_HOST_READ_AD])
}

pub fn status() -> Vec<u8> {
    frame(&[OP_HOST_STATUS])
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn frame_layout_matches_firmware() {
        let f = start();
        assert_eq!(f.len(), 7);
        assert_eq!(f[0], 0xA5);
        assert_eq!(f[1], 1);
        assert_eq!(f[2], OP_HOST_START);
        assert_eq!(&f[3..7], &crc32fast::hash(&[OP_HOST_START]).to_le_bytes());
    }

    #[test]
    fn payload_encodings() {
        assert_eq!(&config(0x2, [1, 2, 3, 4])[2..8], &[OP_HOST_CONFIG, 0x2, 1, 2, 3, 4]);
        assert_eq!(&set_da(1, 25000)[2..6], &[OP_HOST_SET_DA, 1, 0xA8, 0x61]);
        assert_eq!(&read_ad()[2..3], &[OP_HOST_READ_AD]);
        assert_eq!(&status()[2..3], &[OP_HOST_STATUS]);
    }
}
