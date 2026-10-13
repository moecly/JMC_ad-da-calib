//! 上位机→MCU 下行帧编码，对齐固件 calib/CalibLink.cpp 的 [0xA5,len,payload,crc32(LE)] 布局。

use serde::{Deserialize, Serialize};

pub const OP_HOST_CONFIG: u8 = 0x10;
pub const OP_HOST_START: u8 = 0x11;
pub const OP_HOST_PAUSE: u8 = 0x12;
pub const OP_HOST_RESUME: u8 = 0x13;
pub const OP_HOST_RESTART: u8 = 0x14;
pub const OP_HOST_STOP: u8 = 0x15;
pub const OP_HOST_SET_DA: u8 = 0x16;
pub const OP_HOST_READ_AD: u8 = 0x17;
pub const OP_HOST_STATUS: u8 = 0x18;
pub const OP_HOST_VERIFY: u8 = 0x19;
pub const OP_HOST_PARAMS: u8 = 0x1A;
pub const OP_HOST_MANUAL_BEGIN: u8 = 0x20;
pub const OP_HOST_MANUAL_ABORT: u8 = 0x21;
pub const OP_HOST_MANUAL_SET_RAW: u8 = 0x22;
pub const OP_HOST_MANUAL_ADD: u8 = 0x23;
pub const OP_HOST_MANUAL_FIT: u8 = 0x24;
pub const OP_HOST_MANUAL_SAVE: u8 = 0x25;

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

pub fn set_da(ch: u8, val: u16, mode: u8) -> Vec<u8> {
    frame(&[OP_HOST_SET_DA, ch, (val & 0xFF) as u8, (val >> 8) as u8, mode & 1])
}

pub fn read_ad(mode: u8) -> Vec<u8> {
    frame(&[OP_HOST_READ_AD, mode & 1])
}

pub fn status() -> Vec<u8> {
    frame(&[OP_HOST_STATUS])
}

pub fn verify() -> Vec<u8> {
    frame(&[OP_HOST_VERIFY])
}

// 运行时可调参数（须在 START/VERIFY 之前下发；不落 Flash，复位回默认）
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CalibParams {
    pub residual_limit: u16,
    pub check_gate: u16,
    pub stable_code: u16,
    pub settle_ms: u16,
    pub first_settle_ms: u16,
    pub sample_interval_ms: u16,
    pub ref_sample_interval_ms: u16,
    pub stable_count: u8,
    pub skip_verify: bool,
}

impl Default for CalibParams {
    fn default() -> Self {
        Self {
            residual_limit: 100,
            check_gate: 16,
            stable_code: 20,
            settle_ms: 200,
            first_settle_ms: 600,
            sample_interval_ms: 5,
            ref_sample_interval_ms: 20,
            stable_count: 16,
            skip_verify: false,
        }
    }
}

pub fn params(p: &CalibParams) -> Vec<u8> {
    let mut body = Vec::with_capacity(17);
    body.push(OP_HOST_PARAMS);
    body.extend_from_slice(&p.residual_limit.to_le_bytes());
    body.extend_from_slice(&p.check_gate.to_le_bytes());
    body.extend_from_slice(&p.stable_code.to_le_bytes());
    body.extend_from_slice(&p.settle_ms.to_le_bytes());
    body.extend_from_slice(&p.first_settle_ms.to_le_bytes());
    body.extend_from_slice(&p.sample_interval_ms.to_le_bytes());
    body.extend_from_slice(&p.ref_sample_interval_ms.to_le_bytes());
    body.push(p.stable_count);
    body.push(u8::from(p.skip_verify));
    frame(&body)
}

pub fn manual_begin() -> Vec<u8> {
    frame(&[OP_HOST_MANUAL_BEGIN])
}

pub fn manual_abort() -> Vec<u8> {
    frame(&[OP_HOST_MANUAL_ABORT])
}

pub fn manual_set_raw(ch: u8, raw: u16) -> Vec<u8> {
    frame(&[OP_HOST_MANUAL_SET_RAW, ch & 0x03, (raw & 0xFF) as u8, (raw >> 8) as u8])
}

pub fn manual_add(ch: u8, ideal: u16) -> Vec<u8> {
    frame(&[OP_HOST_MANUAL_ADD, ch & 0x03, (ideal & 0xFF) as u8, (ideal >> 8) as u8])
}

pub fn manual_fit() -> Vec<u8> {
    frame(&[OP_HOST_MANUAL_FIT])
}

pub fn manual_save() -> Vec<u8> {
    frame(&[OP_HOST_MANUAL_SAVE])
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
        assert_eq!(&set_da(1, 25000, 1)[2..7], &[OP_HOST_SET_DA, 1, 0xA8, 0x61, 1]);
        assert_eq!(&read_ad(0)[2..4], &[OP_HOST_READ_AD, 0]);
        assert_eq!(&status()[2..3], &[OP_HOST_STATUS]);
        assert_eq!(&verify()[2..3], &[OP_HOST_VERIFY]);
        assert_eq!(&manual_set_raw(2, 40000)[2..6], &[0x22, 2, 0x40, 0x9C]);
        assert_eq!(&manual_add(0, 65535)[2..6], &[0x23, 0, 0xFF, 0xFF]);
        assert_eq!(&manual_fit()[2..3], &[0x24]);
    }

    #[test]
    fn params_frame_layout() {
        let f = params(&CalibParams::default());
        assert_eq!(f[0], 0xA5);
        assert_eq!(f[1], 17);
        assert_eq!(f.len(), 23);
        assert_eq!(
            &f[2..19],
            &[
                0x1A, 0x64, 0x00, 0x10, 0x00, 0x14, 0x00, 0xC8, 0x00, 0x58, 0x02, 0x05, 0x00, 0x14, 0x00, 16, 0
            ]
        );
    }
}
