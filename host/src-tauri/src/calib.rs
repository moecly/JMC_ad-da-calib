use std::sync::LazyLock;

use regex::Regex;
use serde::Serialize;

pub const TOTAL_POINTS: u32 = 67;
const CHANNELS: usize = 4;

#[derive(Clone, Debug, Default, Serialize)]
pub struct Stat {
    pub max: f64,
    pub rmse: f64,
}

#[derive(Clone, Default, Serialize)]
pub struct ChannelReport {
    pub ch: usize,
    pub samples: Vec<f64>,
    pub verify_samples: Vec<f64>,
    pub coeff: Option<[f64; 4]>,
    pub resid: Option<Stat>,
    pub resid_at: Option<u16>,
    pub verify: Option<Stat>,
    pub verify_at: Option<u16>,
    pub before: Option<Stat>,
    pub after: Option<Stat>,
    pub valid: bool,
}

#[derive(Clone, Default, Serialize)]
pub struct RefInfo {
    pub board: Option<String>,
    pub version: Option<String>,
    pub has: bool,
    pub valid_mask: u16,
    pub no_reply: bool,
}

#[derive(Clone, Default, Serialize)]
pub struct ManualPoint {
    pub ch: usize,
    pub n: usize,
    pub ideal: u16,
    pub raw: u16,
}

#[derive(Clone, Default, Serialize)]
pub struct ManualReplace {
    pub ch: usize,
    pub n: usize,
    pub old: u16,
    pub ideal: u16,
}

#[derive(Clone, Default, Serialize)]
pub struct ManualChannel {
    pub ch: usize,
    pub points: Vec<ManualPoint>,
    pub coeff: Option<[f64; 4]>,
    pub resid: Option<Stat>,
    pub verify: Option<Stat>,
    pub fitted: bool,
    /// DA 方向已写入、尚未被采点消费的 raw；采点后清空
    pub pending_raw: Option<u16>,
}

#[derive(Clone, Default, Serialize)]
pub struct ManualReport {
    pub on: bool,
    pub sampling_ch: Option<usize>,
    pub last_replace: Option<ManualReplace>,
    pub channels: Vec<ManualChannel>,
    pub saved: bool,
    pub verify_pass: Option<bool>,
    pub errors: Vec<String>,
}

#[derive(Clone, Debug, Default, Serialize)]
pub struct FwParams {
    pub gate: u16,
    pub resid: u16,
    pub stable: u16,
    pub settle: u16,
    pub first: u16,
    pub samp: u16,
    pub refsamp: u16,
    pub n: u16,
    pub skipv: bool,
    pub retry: u16,
}

#[derive(Clone, Debug, Default, Serialize)]
pub struct RetryEvent {
    pub verify: bool,
    pub round: u8,
    pub idx: u32,
    pub ch: usize,
    /// 扫描段为 ideal，复检段为 set
    pub at: f64,
    pub meas: f64,
    /// 重测样本相对当前系数（复检为落盘系数）的偏差
    pub d: f64,
    /// 被替换的旧样本偏差
    pub old: f64,
    /// true = |d| 不优于 |old|，保留旧样本
    pub dropped: bool,
}

#[derive(Clone, Default, Serialize)]
pub struct CalibReport {
    pub role: Option<String>,
    pub phase: String,
    pub points: u32,
    pub total_points: u32,
    pub grid: Vec<u16>,
    pub verify_grid: Vec<u16>,
    pub verify_points: u32,
    /// 坏点重测事件（扫描与复检，按到达顺序）
    pub retries: Vec<RetryEvent>,
    pub channels: Vec<ChannelReport>,
    pub saved: bool,
    pub verify_started: bool,
    pub verify_pass: Option<bool>,
    pub verify_skipped: bool,
    pub fw_params: Option<FwParams>,
    pub valid_mask: u16,
    pub run_ch: Option<usize>,
    pub ad_read: [Option<f64>; 4],
    pub errors: Vec<String>,
    pub device: Option<String>,
    pub app_version: Option<String>,
    pub board_dir: Option<String>,
    pub record_loaded: bool,
    #[serde(rename = "ref")]
    pub ref_info: Option<RefInfo>,
    pub manual: ManualReport,
}

static RESID_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r"^ch(\d+) resid max=([-0-9.eE+]+) @ideal=(\d+) rmse=([-0-9.eE+]+)").unwrap()
});
static VERIFY_CH_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r"ch(\d+) max=([-0-9.eE+]+) @set=(\d+) rmse=([-0-9.eE+]+)").unwrap()
});
static RUN_RE: LazyLock<Regex> = LazyLock::new(|| Regex::new(r"^run ch=(\d+) da=(\d+)").unwrap());
static AD_RE: LazyLock<Regex> = LazyLock::new(|| Regex::new(r"^ad ch=(\d+) raw=(\d+)").unwrap());
static DPT_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"^dpt(\d+) ch=(\d+) ideal=(\d+) meas=([-0-9.eE+]+)").unwrap());
static DVFY_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r"^dvfy(\d+) ch=(\d+) set=(\d+) meas=([-0-9.eE+]+)").unwrap()
});
static REFAD_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"^refad ch=(\d+) ideal=([-0-9.eE+]+)$").unwrap());
static REFAD_RAW_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"^refad ch=(\d+) raw=(\d+)$").unwrap());
static REF_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r"^ref board=(\d+) ver=(\d+)\.(\d+) has=(\d+) mask=0x([0-9A-Fa-f]+)$").unwrap()
});
static REF_WARN_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"^ref warn ch=(\d+) not calibrated$").unwrap());
static MPT_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"^mpt ch=(\d+) n=(\d+) ideal=(\d+) raw=(\d+)$").unwrap());
static MREP_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r"^mrep ch=(\d+) n=(\d+) old=(\d+) ideal=(\d+) raw=(\d+)$").unwrap()
});
static MSAMPLE_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"^msample ch=(\d+) ideal=(\d+)$").unwrap());
static MSET_RE: LazyLock<Regex> = LazyLock::new(|| Regex::new(r"^mset ch=(\d+) raw=(\d+)$").unwrap());
static MCOEFF_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r"^mcoeff ch=(\d+) ([-0-9.eE+]+) ([-0-9.eE+]+) ([-0-9.eE+]+) ([-0-9.eE+]+)$").unwrap()
});
static MFIT_ERR_RE: LazyLock<Regex> = LazyLock::new(|| Regex::new(r"^mfit \S+ ch=(\d+)").unwrap());
static MRES_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r"^mres ch=(\d+) n=(\d+) max=([-0-9.eE+]+) @ideal=(\d+) rmse=([-0-9.eE+]+)$").unwrap()
});
static MVERIFY_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r"^mverify ch=(\d+) max=([-0-9.eE+]+) @ideal=(\d+) rmse=([-0-9.eE+]+)$").unwrap()
});
static PARAMS_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(
        r"^params gate=(\d+) resid=(\d+) stable=(\d+) settle=(\d+) first=(\d+) samp=(\d+) refsamp=(\d+) n=(\d+) skipv=(\d+) retry=(\d+)$",
    )
    .unwrap()
});
static RETRY_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"^retry (\d+)/(\d+) n=(\d+)$").unwrap());
static VRETRY_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"^vretry (\d+)/(\d+) n=(\d+)$").unwrap());
static RTRY_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r"^rtry(\d+) ch=(\d+) ideal=(\d+) meas=([-0-9.eE+]+) d=([-0-9.eE+]+) old=([-0-9.eE+]+)( drop)?$")
        .unwrap()
});
static VRTRY_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r"^vrtry(\d+) ch=(\d+) set=(\d+) meas=([-0-9.eE+]+) d=([-0-9.eE+]+) old=([-0-9.eE+]+)( drop)?$")
        .unwrap()
});

pub fn poly_eval(coeff: &[f64; 4], x: f64) -> f64 {
    let mut acc = 0.0;
    for k in (0..4).rev() {
        acc = acc * x + coeff[k];
    }
    acc
}

fn put(vec: &mut Vec<u16>, idx: usize, value: u16) {
    if vec.len() <= idx {
        vec.resize(idx + 1, 0);
    }
    vec[idx] = value;
}

fn putf(vec: &mut Vec<f64>, idx: usize, value: f64) {
    if vec.len() <= idx {
        vec.resize(idx + 1, 0.0);
    }
    vec[idx] = value;
}

fn parse_u16(s: &str) -> u16 {
    s.parse().unwrap_or(0)
}

fn parse_f64(s: &str) -> f64 {
    s.parse().unwrap_or(0.0)
}

pub struct Parser {
    pub report: CalibReport,
    /// 当前重测轮次（由 `calib: retry r/N` / `calib: vretry r/N` 横幅更新）
    sweep_round: u8,
    verify_round: u8,
}

impl Default for Parser {
    fn default() -> Self {
        Self::new()
    }
}

fn fresh_manual() -> ManualReport {
    ManualReport {
        on: false,
        sampling_ch: None,
        last_replace: None,
        channels: (0..CHANNELS)
            .map(|ch| ManualChannel {
                ch,
                ..Default::default()
            })
            .collect(),
        saved: false,
        verify_pass: None,
        errors: Vec::new(),
    }
}

fn fresh_report() -> CalibReport {
    let channels = (0..CHANNELS)
        .map(|ch| ChannelReport {
            ch,
            ..Default::default()
        })
        .collect();
    CalibReport {
        role: None,
        phase: "idle".to_string(),
        points: 0,
        total_points: TOTAL_POINTS,
        grid: Vec::new(),
        verify_grid: Vec::new(),
        verify_points: 0,
        retries: Vec::new(),
        channels,
        saved: false,
        verify_started: false,
        verify_pass: None,
        verify_skipped: false,
        fw_params: None,
        valid_mask: 0,
        run_ch: None,
        ad_read: [None; 4],
        errors: Vec::new(),
        device: None,
        app_version: None,
        board_dir: None,
        record_loaded: false,
        ref_info: None,
        manual: fresh_manual(),
    }
}

impl Parser {
    pub fn new() -> Self {
        Self {
            report: fresh_report(),
            sweep_round: 0,
            verify_round: 0,
        }
    }

    fn reset_run(&mut self) {
        // 保留设备/板型/REF 摘要，避免每次 START 后概览卡闪空；run 级与 manual 清空
        let old = std::mem::take(&mut self.report);
        self.report = CalibReport {
            device: old.device,
            app_version: old.app_version,
            role: old.role,
            board_dir: old.board_dir,
            record_loaded: old.record_loaded,
            ref_info: old.ref_info,
            fw_params: old.fw_params,
            ..fresh_report()
        };
    }

    fn chan(&mut self, ch: usize) -> &mut ChannelReport {
        &mut self.report.channels[ch]
    }

    /// `rtry<idx> ch= ideal= meas= d= old=[ drop]` 与
    /// `vrtry<idx> ch= set= meas= d= old=[ drop]` 分组布局一致，共用一个解析
    fn push_retry(&mut self, verify: bool, c: &regex::Captures) {
        let ev = RetryEvent {
            verify,
            round: if verify { self.verify_round } else { self.sweep_round },
            idx: parse_u16(&c[1]) as u32,
            ch: parse_u16(&c[2]) as usize,
            at: parse_f64(&c[3]),
            meas: parse_f64(&c[4]),
            d: parse_f64(&c[5]),
            old: parse_f64(&c[6]),
            dropped: c.get(7).is_some(),
        };
        self.report.retries.push(ev);
    }

    pub fn feed(&mut self, message: &str) -> bool {
        if let Some(rest) = message.strip_prefix("Device: ") {
            if let Some((name, ver)) = rest.split_once(", App Version: ") {
                self.report.device = Some(name.trim().to_string());
                self.report.app_version = Some(ver.trim().to_string());
                return true;
            }
            return false;
        }
        let Some(rest) = message.strip_prefix("calib: ") else {
            return false;
        };
        let changed = self.dispatch(rest);
        if changed {
            self.recompute();
        }
        changed
    }

    fn dispatch(&mut self, rest: &str) -> bool {
        if let Some(v) = rest.strip_prefix("role ") {
            let role = v.split_whitespace().next().unwrap_or("").to_string();
            if role == "DUT" {
                self.report.phase = "sweep".to_string();
            }
            // role 行只表示固件角色；MANUAL 会话是否开始由 `manual on|off` 明确上报
            if role != "MANUAL" {
                self.report.manual = fresh_manual();
            }
            self.report.role = Some(role);
            return true;
        }

        if let Some(v) = rest.strip_prefix("manual ") {
            match v.trim() {
                "on" => {
                    self.report.manual.on = true;
                    self.report.manual.sampling_ch = None;
                    self.report.phase = "manual".to_string();
                }
                "off" => {
                    self.report.manual = fresh_manual();
                    self.report.phase = "manual".to_string();
                }
                _ => return false,
            }
            return true;
        }

        if let Some(v) = rest.strip_prefix("board ") {
            self.report.board_dir = Some(v.trim().to_string());
            return true;
        }

        if rest == "loaded" {
            self.report.record_loaded = true;
            return true;
        }

        if rest == "ref no-reply" {
            self.report.ref_info = Some(RefInfo {
                no_reply: true,
                ..Default::default()
            });
            return true;
        }
        if let Some(c) = REF_RE.captures(rest) {
            let board = match &c[1] {
                "1" => "ad",
                "2" => "da",
                _ => "?",
            };
            self.report.ref_info = Some(RefInfo {
                board: Some(board.to_string()),
                version: Some(format!("{}.{}", &c[2], &c[3])),
                has: &c[4] == "1",
                valid_mask: u16::from_str_radix(&c[5], 16).unwrap_or(0),
                no_reply: false,
            });
            return true;
        }
        if REF_WARN_RE.is_match(rest) {
            self.report.errors.push(rest.to_string());
            return true;
        }

        if rest == "mbegin" {
            self.report.manual = fresh_manual();
            self.report.manual.on = true;
            self.report.phase = "manual".to_string();
            return true;
        }
        if rest == "mabort" {
            self.report.manual = fresh_manual();
            self.report.phase = "manual".to_string();
            return true;
        }
        if let Some(c) = MSAMPLE_RE.captures(rest) {
            let ch = parse_u16(&c[1]) as usize;
            self.report.manual.sampling_ch = if ch < CHANNELS { Some(ch) } else { None };
            self.report.manual.last_replace = None;
            self.report.phase = "manual".to_string();
            return true;
        }
        if let Some(c) = MSET_RE.captures(rest) {
            let ch = parse_u16(&c[1]) as usize;
            if ch < CHANNELS {
                self.report.manual.channels[ch].pending_raw = Some(parse_u16(&c[2]));
            }
            self.report.manual.last_replace = None;
            self.report.phase = "manual".to_string();
            return true;
        }
        if let Some(c) = MPT_RE.captures(rest) {
            let ch = parse_u16(&c[1]) as usize;
            if ch < CHANNELS {
                let point = ManualPoint {
                    ch,
                    n: parse_u16(&c[2]) as usize,
                    ideal: parse_u16(&c[3]),
                    raw: parse_u16(&c[4]),
                };
                let mc = &mut self.report.manual.channels[ch];
                mc.points.push(point);
                mc.fitted = false;
                mc.coeff = None;
                mc.resid = None;
                mc.verify = None;
                mc.pending_raw = None;
            }
            self.report.manual.sampling_ch = None;
            self.report.manual.last_replace = None;
            self.report.manual.saved = false;
            self.report.manual.verify_pass = None;
            self.report.phase = "manual".to_string();
            return true;
        }
        if let Some(c) = MREP_RE.captures(rest) {
            let ch = parse_u16(&c[1]) as usize;
            let n = parse_u16(&c[2]) as usize;
            let old = parse_u16(&c[3]);
            let ideal = parse_u16(&c[4]);
            let raw = parse_u16(&c[5]);
            if ch < CHANNELS {
                let mc = &mut self.report.manual.channels[ch];
                if let Some(p) = mc.points.iter_mut().find(|p| p.n == n) {
                    p.ideal = ideal;
                    p.raw = raw;
                } else {
                    mc.points.push(ManualPoint { ch, n, ideal, raw });
                }
                mc.fitted = false;
                mc.coeff = None;
                mc.resid = None;
                mc.verify = None;
                mc.pending_raw = None;
            }
            self.report.manual.sampling_ch = None;
            self.report.manual.last_replace = Some(ManualReplace { ch, n, old, ideal });
            self.report.manual.saved = false;
            self.report.manual.verify_pass = None;
            self.report.phase = "manual".to_string();
            return true;
        }
        if let Some(c) = MRES_RE.captures(rest) {
            let ch = parse_u16(&c[1]) as usize;
            if ch < CHANNELS {
                let mc = &mut self.report.manual.channels[ch];
                mc.resid = Some(Stat {
                    max: parse_f64(&c[3]),
                    rmse: parse_f64(&c[5]),
                });
                mc.fitted = true;
                mc.verify = None;
            }
            self.report.manual.saved = false;
            self.report.manual.verify_pass = None;
            self.report.phase = "manual".to_string();
            return true;
        }
        if let Some(c) = MCOEFF_RE.captures(rest) {
            let ch = parse_u16(&c[1]) as usize;
            if ch < CHANNELS {
                self.report.manual.channels[ch].coeff = Some([
                    parse_f64(&c[2]),
                    parse_f64(&c[3]),
                    parse_f64(&c[4]),
                    parse_f64(&c[5]),
                ]);
            }
            return true;
        }
        if rest.starts_with("mfit reject") || rest.starts_with("mfit resid") || rest.starts_with("mfit failed") {
            if let Some(c) = MFIT_ERR_RE.captures(rest) {
                let ch = parse_u16(&c[1]) as usize;
                if ch < CHANNELS {
                    let mc = &mut self.report.manual.channels[ch];
                    mc.fitted = false;
                    mc.coeff = None;
                }
            }
            self.report.manual.errors.push(rest.to_string());
            self.report.phase = "manual".to_string();
            return true;
        }
        if rest == "msaved" {
            self.report.manual.saved = true;
            self.report.phase = "manual-verify".to_string();
            return true;
        }
        if let Some(c) = MVERIFY_RE.captures(rest) {
            let ch = parse_u16(&c[1]) as usize;
            if ch < CHANNELS {
                self.report.manual.channels[ch].verify = Some(Stat {
                    max: parse_f64(&c[2]),
                    rmse: parse_f64(&c[4]),
                });
            }
            self.report.phase = "manual-verify".to_string();
            return true;
        }
        if rest == "mverify PASS" {
            self.report.manual.verify_pass = Some(true);
            self.report.phase = "done".to_string();
            return true;
        }
        if rest == "mverify FAIL" {
            self.report.manual.verify_pass = Some(false);
            self.report.phase = "fail".to_string();
            return true;
        }
        if let Some(err) = rest.strip_prefix("error manual-") {
            if err == "off"
                || err == "busy"
                || err == "full"
                || err == "no-fit"
                || err == "no-raw"
                || err == "no-points"
            {
                self.report.manual.errors.push(rest.to_string());
                return true;
            }
        }
        if rest == "error bad-op" && self.report.role.as_deref() == Some("MANUAL") {
            self.report.manual.errors.push(rest.to_string());
            return true;
        }

        if rest == "reset" {
            self.reset_run();
            return true;
        }

        if let Some(v) = rest.strip_prefix("state ") {
            self.report.phase = v.trim().to_string();
            return true;
        }

        if let Some(v) = rest.strip_prefix("mask ") {
            let tok = v.trim();
            let hex = tok.strip_prefix("0x").or_else(|| tok.strip_prefix("0X")).unwrap_or(tok);
            let Ok(mask) = u16::from_str_radix(hex, 16) else {
                return false;
            };
            self.report.valid_mask = mask;
            for ch in self.report.channels.iter_mut() {
                ch.valid = (mask >> ch.ch) & 1 == 1;
            }
            return true;
        }

        if rest.starts_with("cfg ") {
            return false;
        }

        if let Some(c) = RUN_RE.captures(rest) {
            self.report.run_ch = c[1].parse().ok();
            return true;
        }

        if let Some(c) = DPT_RE.captures(rest) {
            let idx = parse_u16(&c[1]) as usize;
            let ch = parse_u16(&c[2]) as usize;
            let ideal = parse_u16(&c[3]);
            let meas = parse_f64(&c[4]);
            put(&mut self.report.grid, idx, ideal);
            if ch < CHANNELS {
                putf(&mut self.chan(ch).samples, idx, meas);
            }
            let done = (idx + 1) as u32;
            if done > self.report.points {
                self.report.points = done;
            }
            self.report.phase = "sweep".to_string();
            return true;
        }

        // 坏点重测：横幅行只记轮次；逐点行记成事件（不置 fail、不进 errors——
        // 只有用尽轮数后的 `chN resid>` / `vfy chN |d|>=` 才是失败）
        if let Some(c) = RETRY_RE.captures(rest) {
            self.sweep_round = parse_u16(&c[1]) as u8;
            return true;
        }
        if let Some(c) = VRETRY_RE.captures(rest) {
            self.verify_round = parse_u16(&c[1]) as u8;
            return true;
        }
        if let Some(c) = RTRY_RE.captures(rest) {
            self.push_retry(false, &c);
            return true;
        }
        if let Some(c) = VRTRY_RE.captures(rest) {
            self.push_retry(true, &c);
            return true;
        }

        if let Some(c) = DVFY_RE.captures(rest) {
            let idx = parse_u16(&c[1]) as usize;
            let ch = parse_u16(&c[2]) as usize;
            put(&mut self.report.verify_grid, idx, parse_u16(&c[3]));
            if ch < CHANNELS {
                putf(&mut self.chan(ch).verify_samples, idx, parse_f64(&c[4]));
            }
            let done = (idx + 1) as u32;
            if done > self.report.verify_points {
                self.report.verify_points = done;
            }
            self.report.phase = "verify".to_string();
            return true;
        }

        if let Some(c) = AD_RE.captures(rest) {
            let ch = parse_u16(&c[1]) as usize;
            if ch >= CHANNELS {
                return false;
            }
            self.report.ad_read[ch] = Some(parse_u16(&c[2]) as f64);
            return true;
        }

        if let Some(c) = REFAD_RAW_RE.captures(rest) {
            let ch = parse_u16(&c[1]) as usize;
            if ch >= CHANNELS {
                return false;
            }
            self.report.ad_read[ch] = Some(parse_u16(&c[2]) as f64);
            return true;
        }

        if let Some(c) = REFAD_RE.captures(rest) {
            let ch = parse_u16(&c[1]) as usize;
            if ch >= CHANNELS {
                return false;
            }
            self.report.ad_read[ch] = Some(parse_f64(&c[2]));
            return true;
        }

        if let Some(v) = rest.strip_prefix("dump grid ") {
            return self.feed_dump(v, None);
        }
        if let Some(v) = rest.strip_prefix("dump ch") {
            let mut it = v.split_whitespace();
            let Some(ch) = it.next().and_then(|s| s.parse::<usize>().ok()) else {
                return false;
            };
            let tail = v
                .split_once(char::is_whitespace)
                .map(|(_, t)| t)
                .unwrap_or("");
            return self.feed_dump(tail, Some(ch));
        }

        if let Some(v) = rest.strip_prefix("coeff ch") {
            return self.feed_coeff(v);
        }

        if rest == "saved" {
            self.report.saved = true;
            self.report.phase = "verify".to_string();
            return true;
        }

        if rest == "verify skipped" {
            self.report.verify_skipped = true;
            return true;
        }

        if let Some(c) = PARAMS_RE.captures(rest) {
            self.report.fw_params = Some(FwParams {
                gate: parse_u16(&c[1]),
                resid: parse_u16(&c[2]),
                stable: parse_u16(&c[3]),
                settle: parse_u16(&c[4]),
                first: parse_u16(&c[5]),
                samp: parse_u16(&c[6]),
                refsamp: parse_u16(&c[7]),
                n: parse_u16(&c[8]),
                skipv: &c[9] == "1",
                retry: parse_u16(&c[10]),
            });
            return true;
        }

        if let Some(v) = rest.strip_prefix("verify ") {
            return self.feed_verify(v);
        }

        if let Some(v) = rest.strip_prefix("vfy") {
            if v.contains("|d|>=") {
                self.report.errors.push(rest.to_string());
                return true;
            }
            return false;
        }

        if let Some(v) = rest.strip_prefix("chk ") {
            if v.starts_with("fail") {
                self.report.errors.push(rest.to_string());
                self.report.phase = "fail".to_string();
                return true;
            }
            if v == "pass" {
                self.report.verify_pass = Some(true);
                self.report.phase = "done".to_string();
                return true;
            }
            return false;
        }

        if rest.starts_with("ch") && rest[2..].starts_with(|c: char| c.is_ascii_digit()) {
            if let Some(c) = RESID_RE.captures(rest) {
                let ch = parse_u16(&c[1]) as usize;
                if ch < CHANNELS {
                    let rep = self.chan(ch);
                    rep.resid = Some(Stat {
                        max: parse_f64(&c[2]),
                        rmse: parse_f64(&c[4]),
                    });
                    rep.resid_at = Some(parse_u16(&c[3]));
                    return true;
                }
            } else if rest.contains("resid>") {
                self.report.errors.push(rest.to_string());
                // 固件此行走 LogError 后立即 EnterFail：不置 fail 会让上位机等到 600 s 超时才停
                self.report.phase = "fail".to_string();
                return true;
            }
            return false;
        }

        if rest == "ref not found"
            || rest == "ref timeout"
            || rest == "flash write failed"
            || rest == "error ref no-reply"
            || rest == "error ref mismatch"
            || rest == "error read_ad timeout"
        {
            self.report.errors.push(rest.to_string());
            self.report.phase = "fail".to_string();
            if rest == "error ref no-reply" {
                self.report.ref_info = Some(RefInfo {
                    no_reply: true,
                    ..Default::default()
                });
            }
            return true;
        }
        if rest.starts_with("fit failed ch=") {
            self.report.errors.push(rest.to_string());
            self.report.phase = "fail".to_string();
            return true;
        }

        false
    }

    fn feed_dump(&mut self, tail: &str, ch: Option<usize>) -> bool {
        let mut it = tail.split_whitespace();
        let Some(off) = it.next().and_then(|s| s.parse::<usize>().ok()) else {
            return false;
        };
        match ch {
            None => {
                for (i, v) in it.enumerate() {
                    put(&mut self.report.grid, off + i, parse_u16(v));
                }
            }
            Some(ch) if ch < CHANNELS => {
                for (i, v) in it.enumerate() {
                    let val = parse_f64(v);
                    putf(&mut self.chan(ch).samples, off + i, val);
                }
            }
            _ => return false,
        }
        self.report.phase = "dump".to_string();
        true
    }

    fn feed_coeff(&mut self, v: &str) -> bool {
        let mut it = v.split_whitespace();
        let Some(ch) = it.next().and_then(|s| s.parse::<usize>().ok()) else {
            return false;
        };
        if ch >= CHANNELS {
            return false;
        }
        let mut coeff = [0.0f64; 4];
        let mut n = 0;
        for tok in it {
            if n >= 4 {
                break;
            }
            coeff[n] = parse_f64(tok);
            n += 1;
        }
        if n != 4 {
            return false;
        }
        self.chan(ch).coeff = Some(coeff);
        self.report.manual.channels[ch].coeff = Some(coeff);
        true
    }

    fn feed_verify(&mut self, v: &str) -> bool {
        match v {
            "start" => {
                self.report.verify_started = true;
                true
            }
            "PASS" => {
                self.report.verify_pass = Some(true);
                self.report.phase = "done".to_string();
                true
            }
            "FAIL" => {
                self.report.verify_pass = Some(false);
                self.report.phase = "done".to_string();
                true
            }
            "ref timeout" => {
                self.report.errors.push("verify ref timeout".to_string());
                self.report.phase = "fail".to_string();
                true
            }
            _ => {
                let mut any = false;
                for c in VERIFY_CH_RE.captures_iter(v) {
                    let ch = parse_u16(&c[1]) as usize;
                    if ch >= CHANNELS {
                        continue;
                    }
                    let rep = self.chan(ch);
                    rep.verify = Some(Stat {
                        max: parse_f64(&c[2]),
                        rmse: parse_f64(&c[4]),
                    });
                    rep.verify_at = Some(parse_u16(&c[3]));
                    any = true;
                }
                if any {
                    self.report.phase = "verify".to_string();
                }
                any
            }
        }
    }

    fn recompute(&mut self) {
        let n = self.report.grid.len();
        if n == 0 {
            return;
        }
        let grid = self.report.grid.clone();
        for ch in &mut self.report.channels {
            if ch.samples.len() != n {
                ch.before = None;
                ch.after = None;
                continue;
            }
            ch.before = Some(stat_of(&ch.samples, &grid, None));
            ch.after = ch
                .coeff
                .as_ref()
                .map(|coeff| stat_of(&ch.samples, &grid, Some(coeff)));
        }
    }
}

fn stat_of(samples: &[f64], grid: &[u16], coeff: Option<&[f64; 4]>) -> Stat {
    let mut sum_sq = 0.0;
    let mut max = 0.0;
    for (s, g) in samples.iter().zip(grid.iter()) {
        let measured = match coeff {
            Some(c) => poly_eval(c, *s),
            None => *s,
        };
        let d = measured - *g as f64;
        sum_sq += d * d;
        let abs = d.abs();
        if abs > max {
            max = abs;
        }
    }
    let n = samples.len() as f64;
    Stat {
        max,
        rmse: (sum_sq / n).sqrt(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn framed(func: &str, msg: &str) -> String {
        format!("[INFO] CalibProc.cpp:200 ({func}): {msg}")
    }

    fn build_log() -> Vec<String> {
        let coeff = [0.0f64, 1.0, 0.0, 0.0];
        let grid: Vec<u16> = (0..TOTAL_POINTS).map(|i| (i * 100) as u16).collect();
        let samples: Vec<u16> = grid
            .iter()
            .enumerate()
            .map(|(i, g)| *g + (i as u16) * 3)
            .collect();

        let mut lines = Vec::new();
        lines.push(framed("DoSweep", "calib: role DUT"));
        for i in 0..TOTAL_POINTS as usize {
            for ch in 0..CHANNELS {
                let unstable = if i == 5 { " unstable" } else { "" };
                lines.push(framed(
                    "DoSweep",
                    &format!(
                        "calib: dpt{i} ch={ch} ideal={} meas={}{unstable}",
                        grid[i], samples[i]
                    ),
                ));
            }
        }
        for ch in 0..CHANNELS {
            let mut sum_sq = 0.0;
            let mut max = 0.0;
            let mut at = 0;
            for i in 0..TOTAL_POINTS as usize {
                let d = poly_eval(&coeff, samples[i] as f64) - grid[i] as f64;
                sum_sq += d * d;
                if d.abs() > max {
                    max = d.abs();
                    at = i;
                }
            }
            let rmse = (sum_sq / TOTAL_POINTS as f64).sqrt();
            lines.push(framed(
                "DoFit",
                &format!(
                    "calib: ch{ch} resid max={max:.1} @ideal={} rmse={rmse:.1}",
                    grid[at]
                ),
            ));
            lines.push(framed(
                "DoFit",
                &format!("calib: ch{ch} anchor dev = 0.5"),
            ));
        }
        for line in 0..3 {
            let off = line * 24;
            let vals: Vec<String> = (off..grid.len().min(off + 24))
                .map(|i| grid[i].to_string())
                .collect();
            lines.push(framed(
                "DoDump",
                &format!("calib: dump grid {off} {}", vals.join(" ")),
            ));
        }
        for ch in 0..CHANNELS {
            for line in 0..3 {
                let off = line * 24;
                let vals: Vec<String> = (off..samples.len().min(off + 24))
                    .map(|i| samples[i].to_string())
                    .collect();
                lines.push(framed(
                    "DoDump",
                    &format!("calib: dump ch{ch} {off} {}", vals.join(" ")),
                ));
            }
        }
        for ch in 0..CHANNELS {
            lines.push(framed(
                "DoDump",
                &format!(
                    "calib: coeff ch{ch} {:.17} {:.17} {:.17} {:.17}",
                    coeff[0], coeff[1], coeff[2], coeff[3]
                ),
            ));
        }
        lines.push(framed("DoSave", "calib: saved"));
        lines.push(framed("DoVerify", "calib: verify start"));
        for i in 0..TOTAL_POINTS as usize {
            for ch in 0..CHANNELS {
                lines.push(framed(
                    "DoVerify",
                    &format!(
                        "calib: dvfy{i} ch={ch} set={} meas={}",
                        grid[i], samples[i]
                    ),
                ));
                lines.push(framed(
                    "DoVerify",
                    &format!("calib: vfy set={} ch{ch} d=0.0", grid[i]),
                ));
            }
        }
        let mut verify = String::from("calib: verify");
        for ch in 0..CHANNELS {
            let sep = if ch == 0 { "" } else { ";" };
            verify.push_str(&format!("{sep} ch{ch} max=0.0 @set=0 rmse=0.0"));
        }
        lines.push(framed("DoVerify", &verify));
        lines.push(framed("DoVerify", "calib: verify PASS"));
        lines.push(framed("HostPoll", "calib: mask 0x5"));
        lines.push(framed("HostPoll", "calib: state done"));
        lines
    }

    #[test]
    fn parses_full_log() {
        let mut parser = Parser::new();
        for raw in build_log() {
            let line = crate::rtt::parse_line(&raw);
            parser.feed(&line.message);
        }
        let r = &parser.report;
        assert_eq!(r.role.as_deref(), Some("DUT"));
        assert_eq!(r.points, TOTAL_POINTS);
        assert_eq!(r.grid.len(), TOTAL_POINTS as usize);
        assert!(r.saved);
        assert!(r.verify_started);
        assert_eq!(r.verify_pass, Some(true));
        assert_eq!(r.phase, "done");
        assert!(r.errors.is_empty());
        assert_eq!(r.valid_mask, 0x5);
        assert!(r.channels[0].valid);
        assert!(!r.channels[1].valid);
        assert!(r.channels[2].valid);
        assert!(!r.channels[3].valid);
        for ch in &r.channels {
            assert_eq!(ch.samples.len(), TOTAL_POINTS as usize);
            // 一行汇总须覆盖全部通道（固件缓冲区过小会静默截断末通道）
            assert!(ch.verify.is_some(), "ch{} verify missing", ch.ch);
            // 固件 resid 以 %.1f 打印，与本机按全精度复算的值比对需容忍半个末位
            let after = ch.after.clone().unwrap();
            let resid = ch.resid.clone().unwrap();
            assert!((after.max - resid.max).abs() < 0.05, "max {after:?} vs {resid:?}");
            assert!((after.rmse - resid.rmse).abs() < 0.05, "rmse {after:?} vs {resid:?}");
        }
        assert!(r.channels[0].before.clone().unwrap().max > 0.0);
    }

    #[test]
    fn corrupt_line_is_skipped() {
        let mut parser = Parser::new();
        for raw in build_log() {
            let line = crate::rtt::parse_line(&raw);
            parser.feed(&line.message);
        }
        let grid_before = parser.report.grid.clone();
        let points_before = parser.report.points;

        assert!(!parser.feed("calib: ptGARBAGE ideal= raw=,"));
        assert!(!parser.feed("calib: dump grid XX YY"));
        assert!(!parser.feed("[INFO] other: not a calib line"));
        assert!(!parser.feed("totally unrelated"));

        assert_eq!(parser.report.grid, grid_before);
        assert_eq!(parser.report.points, points_before);
    }

    #[test]
    fn parses_host_control_lines() {
        let mut parser = Parser::new();
        assert!(parser.feed("calib: run ch=1 da=0"));
        assert_eq!(parser.report.run_ch, Some(1));
        assert!(parser.feed("calib: ad ch=1 raw=12345 ideal=24690.0"));
        assert_eq!(parser.report.ad_read[1], Some(12345.0));
        assert_eq!(parser.report.ad_read[0], None);
        assert!(parser.feed("calib: mask 0x2"));
        assert_eq!(parser.report.valid_mask, 0x2);
        assert!(parser.feed("calib: state paused"));
        assert_eq!(parser.report.phase, "paused");
        assert!(!parser.feed("calib: cfg mask=0xF da=0,0,0,0"));
    }

    #[test]
    fn parses_da_direction_lines() {
        let mut parser = Parser::new();
        assert!(parser.feed("calib: role DUT"));
        assert!(parser.feed("calib: run ch=0 da=2"));
        assert!(parser.feed("calib: dpt3 ch=0 ideal=3000 meas=3100"));
        assert_eq!(parser.report.points, 4);
        assert_eq!(parser.report.grid[3], 3000);
        assert_eq!(parser.report.channels[0].samples[3], 3100.0);
        assert_eq!(parser.report.channels[1].samples.len(), 0);
        assert!(parser.feed("calib: dpt3 ch=1 ideal=3000 meas=3050"));
        assert_eq!(parser.report.channels[1].samples[3], 3050.0);
        assert_eq!(parser.report.points, 4);

        // REF ideal 可超 65535（AD 标定曲线），不得被当成 u16 溢出丢成 0
        assert!(parser.feed("calib: dpt4 ch=0 ideal=65535 meas=65930"));
        assert_eq!(parser.report.channels[0].samples[4], 65930.0);

        assert!(parser.feed("calib: refad ch=2 ideal=12345.5"));
        assert_eq!(parser.report.ad_read[2], Some(12345.5));

        assert!(parser.feed("calib: error ref mismatch"));
        assert!(!parser.report.errors.is_empty());
        assert_eq!(parser.report.phase, "fail");
    }

    #[test]
    fn resid_over_limit_is_failure() {
        let mut parser = Parser::new();
        assert!(parser.feed("calib: state fit"));
        assert!(parser.feed("calib: ch2 resid max=98.0 @ideal=63000 rmse=12.0"));
        assert_eq!(parser.report.phase, "fit");
        assert_eq!(parser.report.channels[2].resid.as_ref().unwrap().max, 98.0);

        assert!(parser.feed("calib: ch2 resid>50 at: 63000(98)"));
        assert_eq!(parser.report.phase, "fail");
        assert!(parser.report.errors.iter().any(|e| e.contains("resid>50")));
    }

    #[test]
    fn reset_clears_previous_run() {
        let mut parser = Parser::new();
        parser.feed("calib: role DUT");
        parser.feed("calib: dpt0 ch=0 ideal=0 meas=1");
        parser.feed("calib: ch0 resid max=5.0 @ideal=0 rmse=3.0");
        parser.feed("calib: saved");
        parser.feed("calib: verify PASS");
        assert!(parser.report.saved);
        assert_eq!(parser.report.points, 1);

        assert!(parser.feed("calib: reset"));
        assert!(!parser.report.saved);
        assert_eq!(parser.report.points, 0);
        assert_eq!(parser.report.phase, "idle");
        // 角色是会话级摘要，reset 后仍保留（固件 OP_HOST_VERIFY 不重发 role，靠此维持 DUT 识别）
        assert_eq!(parser.report.role.as_deref(), Some("DUT"));
        assert_eq!(parser.report.channels.len(), CHANNELS);
        assert!(parser.report.channels[0].coeff.is_none());
        assert!(parser.report.errors.is_empty());
    }

    #[test]
    fn parses_device_line() {
        let mut parser = Parser::new();
        assert!(parser.feed("Device: AD_4CH, App Version: v1.8"));
        assert_eq!(parser.report.device.as_deref(), Some("AD_4CH"));
        assert_eq!(parser.report.app_version.as_deref(), Some("v1.8"));
        assert!(parser.feed("calib: board ad"));
        assert_eq!(parser.report.board_dir.as_deref(), Some("ad"));
        assert!(parser.feed("calib: loaded"));
        assert!(parser.report.record_loaded);
        assert!(!parser.feed("Device: broken"));
    }

    #[test]
    fn parses_ref_info() {
        let mut parser = Parser::new();
        assert!(parser.feed("calib: ref board=2 ver=1.9 has=1 mask=0x3"));
        let r = parser.report.ref_info.clone().unwrap();
        assert_eq!(r.board.as_deref(), Some("da"));
        assert_eq!(r.version.as_deref(), Some("1.9"));
        assert!(r.has);
        assert_eq!(r.valid_mask, 0x3);
        assert!(!r.no_reply);

        assert!(parser.feed("calib: ref warn ch=1 not calibrated"));
        assert_eq!(parser.report.errors.len(), 1);

        assert!(parser.feed("calib: ref no-reply"));
        assert!(parser.report.ref_info.unwrap().no_reply);
    }

    #[test]
    fn parses_manual_session() {
        let mut parser = Parser::new();
        assert!(parser.feed("calib: mbegin"));
        assert!(parser.report.manual.on);
        assert_eq!(parser.report.phase, "manual");
        assert_eq!(parser.report.manual.channels.len(), CHANNELS);

        assert!(parser.feed("calib: mset ch=0 raw=1000"));
        assert_eq!(parser.report.manual.channels[0].pending_raw, Some(1000));

        for n in 0..5u16 {
            let ideal = 1000 * (n + 1);
            let raw = 900 * (n + 1);
            assert!(parser.feed(&format!("calib: mpt ch=0 n={} ideal={} raw={}", n + 1, ideal, raw)));
        }
        assert_eq!(parser.report.manual.channels[0].points.len(), 5);
        assert_eq!(parser.report.manual.channels[0].points[4].ideal, 5000);
        assert_eq!(parser.report.manual.channels[0].points[4].raw, 4500);
        assert_eq!(parser.report.manual.channels[0].pending_raw, None);

        assert!(parser.feed("calib: mres ch=0 n=5 max=1.5 @ideal=5000 rmse=0.5"));
        assert!(parser.report.manual.channels[0].fitted);
        assert_eq!(parser.report.manual.channels[0].resid.as_ref().unwrap().max, 1.5);
        assert!(parser.feed("calib: mcoeff ch=0 1.25 0.5 -0.0001 2e-9"));
        assert_eq!(
            parser.report.manual.channels[0].coeff,
            Some([1.25, 0.5, -0.0001, 2e-9])
        );

        assert!(parser.feed("calib: msaved"));
        assert!(parser.report.manual.saved);
        assert_eq!(parser.report.phase, "manual-verify");

        assert!(parser.feed("calib: mask 0x1"));
        assert_eq!(parser.report.valid_mask, 0x1);

        assert!(parser.feed("calib: mverify ch=0 max=1.5 @ideal=5000 rmse=0.5"));
        assert_eq!(parser.report.manual.channels[0].verify.as_ref().unwrap().max, 1.5);

        assert!(parser.feed("calib: mverify PASS"));
        assert_eq!(parser.report.manual.verify_pass, Some(true));
        assert_eq!(parser.report.phase, "done");
        assert!(parser.report.manual.errors.is_empty());
    }

    #[test]
    fn manual_role_is_not_a_session() {
        let mut parser = Parser::new();
        assert!(parser.feed("calib: role MANUAL"));
        assert_eq!(parser.report.role.as_deref(), Some("MANUAL"));
        assert!(!parser.report.manual.on, "role 行不得开启手动会话");
        assert!(parser.feed("calib: manual off"));
        assert!(parser.feed("calib: mbegin"));
        assert!(parser.report.manual.on);

        assert!(parser.feed("calib: mset ch=1 raw=2000"));
        assert!(parser.feed("calib: msample ch=1 ideal=100"));
        assert_eq!(parser.report.manual.sampling_ch, Some(1));
        assert!(parser.feed("calib: mpt ch=1 n=1 ideal=100 raw=90"));
        assert_eq!(parser.report.manual.sampling_ch, None);
        assert_eq!(parser.report.manual.channels[1].points.len(), 1);

        assert!(parser.feed("calib: manual off"));
        assert!(!parser.report.manual.on);
        assert!(parser.report.manual.channels[1].points.is_empty());
    }

    #[test]
    fn manual_residual_reject_unfits_channel() {
        let mut parser = Parser::new();
        parser.feed("calib: mbegin");
        parser.feed("calib: mres ch=2 n=9 max=1.0 @ideal=0 rmse=0.5");
        assert!(parser.report.manual.channels[2].fitted);
        assert!(parser.feed("calib: mfit resid ch=2 max=31.0 limit=20.0"));
        assert!(!parser.report.manual.channels[2].fitted);
        assert_eq!(parser.report.manual.errors.len(), 1);
        assert!(parser.feed("calib: error manual-no-raw"));
        assert_eq!(parser.report.manual.errors.len(), 2);
    }

    #[test]
    fn manual_reject_keeps_points() {
        let mut parser = Parser::new();
        parser.feed("calib: mbegin");
        parser.feed("calib: mpt ch=0 n=1 ideal=0 raw=0");
        parser.feed("calib: mpt ch=0 n=2 ideal=1000 raw=900");
        assert!(parser.feed("calib: mfit reject ch=0 n=2 span=900"));
        assert_eq!(parser.report.manual.channels[0].points.len(), 2);
        assert!(!parser.report.manual.channels[0].fitted);
        assert_eq!(parser.report.manual.errors.len(), 1);
        assert_eq!(parser.report.phase, "manual");
    }

    #[test]
    fn manual_replace_overwrites_same_raw_point() {
        let mut parser = Parser::new();
        parser.feed("calib: mbegin");
        parser.feed("calib: mset ch=0 raw=32768");
        parser.feed("calib: mpt ch=0 n=1 ideal=20000 raw=32768");
        parser.feed("calib: mres ch=0 n=1 max=1.0 @ideal=20000 rmse=0.5");
        parser.feed("calib: mcoeff ch=0 1 2 3 4");
        assert_eq!(parser.report.manual.channels[0].points.len(), 1);

        parser.feed("calib: mset ch=0 raw=32768");
        parser.feed("calib: mrep ch=0 n=1 old=20000 ideal=21000 raw=32768");
        let m = &parser.report.manual;
        let c = &m.channels[0];
        assert_eq!(c.points.len(), 1);
        assert_eq!(c.points[0].ideal, 21000);
        assert_eq!(c.points[0].raw, 32768);
        assert!(!c.fitted);
        assert!(c.coeff.is_none());
        assert_eq!(c.pending_raw, None);
        let rep = m.last_replace.as_ref().unwrap();
        assert_eq!((rep.ch, rep.n, rep.old, rep.ideal), (0, 1, 20000, 21000));
    }

    #[test]
    fn manual_replace_overwrites_same_ideal_point() {
        let mut parser = Parser::new();
        parser.feed("calib: mbegin");
        parser.feed("calib: mpt ch=0 n=1 ideal=1000 raw=900");
        parser.feed("calib: msample ch=0 ideal=1000");
        assert_eq!(parser.report.manual.sampling_ch, Some(0));

        parser.feed("calib: mrep ch=0 n=1 old=900 ideal=1000 raw=950");
        let m = &parser.report.manual;
        let c = &m.channels[0];
        assert_eq!(c.points.len(), 1);
        assert_eq!(c.points[0].ideal, 1000);
        assert_eq!(c.points[0].raw, 950);
        assert_eq!(m.sampling_ch, None);
        assert_eq!(m.last_replace.as_ref().unwrap().old, 900);
    }

    #[test]
    fn parses_verify_only_session() {
        let mut lines = vec![
            framed("HostPoll", "calib: role DUT"),
            framed("HostPoll", "calib: board ad"),
            framed("HostPoll", "calib: reset"),
            framed("HostPoll", "calib: state verify"),
            framed("HostPoll", "calib: coeff ch0 0.0 1.0 0.0 0.0"),
            framed("HostPoll", "calib: verify start"),
        ];
        for i in 0..TOTAL_POINTS {
            let set = i * 1000;
            lines.push(framed(
                "DoVerify",
                &format!("calib: dvfy{i} ch=0 set={set} meas={}", set + 3),
            ));
        }
        lines.push(framed("DoVerify", "calib: verify ch0 max=3.0 @set=0 rmse=3.0"));
        lines.push(framed("DoVerify", "calib: verify PASS"));

        let mut parser = Parser::new();
        for raw in lines {
            let line = crate::rtt::parse_line(&raw);
            parser.feed(&line.message);
        }
        let r = &parser.report;
        assert_eq!(r.verify_points, TOTAL_POINTS);
        assert_eq!(r.verify_grid.len(), TOTAL_POINTS as usize);
        assert_eq!(r.channels[0].verify_samples[5], 5003.0);
        assert!(r.channels[0].verify.is_some());
        assert_eq!(r.verify_pass, Some(true));
    }

    #[test]
    fn parses_verify_skipped() {
        // 跳过复检的完整固件序列：落盘后不得出现任何复检行
        let mut parser = Parser::new();
        let lines = [
            "HostPoll|calib: reset",
            "HostPoll|calib: role DUT",
            "HostPoll|calib: params gate=50 resid=800 stable=20 settle=300 first=600 samp=5 refsamp=20 n=16 skipv=1 retry=3",
            "DoSweep|calib: dpt0 ch=0 ideal=0 meas=1.5",
            "FitAll|calib: ch0 resid max=1.5 @ideal=0 rmse=0.5",
            "DoSave|calib: saved",
            "DoSave|calib: mask 0x1",
            "DoSave|calib: verify skipped",
            "DutTick|calib: state done",
        ];
        for l in lines {
            let (source, msg) = l.split_once('|').unwrap();
            let line = crate::rtt::parse_line(&format!("[INFO] CalibProc.cpp:1 ({source}): {msg}"));
            assert!(parser.feed(&line.message), "{l}");
        }
        assert!(parser.report.saved);
        assert!(parser.report.verify_skipped);
        assert_eq!(parser.report.fw_params.as_ref().unwrap().gate, 50);
        // "verify skipped" 不得被 "verify " 前缀吞成复检流程行
        assert!(!parser.report.verify_started);
        assert_eq!(parser.report.verify_points, 0);
        assert_eq!(parser.report.phase, "done");
    }

    #[test]
    fn parses_status_reply() {
        // DUT 的 OP_HOST_STATUS 应答：重连不复位 MCU 时靠这些行补齐概览卡
        let mut parser = Parser::new();
        let lines = [
            "HostPoll|Device: AD_4CH, App Version: v1.9",
            "User|calib: role DUT",
            "HostPoll|calib: board ad",
            "HostPoll|calib: loaded",
            "HostPoll|calib: state idle",
            "HostPoll|calib: mask 0xF",
            "HostPoll|calib: coeff ch0 30.05415 0.995169 0 0",
        ];
        for l in lines {
            let (source, msg) = l.split_once('|').unwrap();
            let line = crate::rtt::parse_line(&format!("[INFO] CalibProc.cpp:1 ({source}): {msg}"));
            assert!(parser.feed(&line.message), "{l}");
        }
        assert_eq!(parser.report.device.as_deref(), Some("AD_4CH"));
        assert_eq!(parser.report.app_version.as_deref(), Some("v1.9"));
        assert_eq!(parser.report.board_dir.as_deref(), Some("ad"));
        assert!(parser.report.record_loaded);
        assert_eq!(parser.report.role.as_deref(), Some("DUT"));
        assert_eq!(parser.report.phase, "idle");
        assert_eq!(parser.report.valid_mask, 0xF);
        assert!(parser.report.channels[0].valid);
        assert!(parser.report.channels[0].coeff.is_some());
    }

    #[test]
    fn parses_fw_params() {
        let mut parser = Parser::new();
        assert!(parser.feed(
            "calib: params gate=50 resid=800 stable=20 settle=300 first=600 samp=5 refsamp=20 n=16 skipv=1 retry=3"
        ));
        let p = parser.report.fw_params.clone().unwrap();
        assert_eq!(p.gate, 50);
        assert_eq!(p.resid, 800);
        assert_eq!(p.stable, 20);
        assert_eq!(p.settle, 300);
        assert_eq!(p.first, 600);
        assert_eq!(p.samp, 5);
        assert_eq!(p.refsamp, 20);
        assert_eq!(p.n, 16);
        assert!(p.skipv);
    }

    #[test]
    fn retry_lines_are_non_fatal() {
        // 拟合残差/复检超差触发的自动重测行必须被视为非致命：不得置 fail、不得进 errors
        let mut parser = Parser::new();
        assert!(parser.feed(
            "calib: params gate=50 resid=20 stable=20 settle=300 first=600 samp=5 refsamp=20 n=16 skipv=0 retry=3"
        ));
        assert!(parser.feed("calib: state fit"));
        assert!(parser.feed("calib: ch0 resid max=18.0 @ideal=0 rmse=1.0"));
        assert_eq!(parser.report.phase, "fit");

        // 拟合阶段的重测行：被解析但仍非致命
        assert!(parser.feed("calib: retry 1/2 n=2"));
        assert!(parser.feed("calib: rtry5 ch=0 ideal=5000 meas=4890.0 d=110.0 old=112.0"));
        assert_eq!(parser.report.phase, "fit");
        assert!(parser.report.errors.is_empty());
        let e = &parser.report.retries[0];
        assert!(!e.verify);
        assert_eq!((e.round, e.idx, e.ch), (1, 5, 0));
        assert_eq!(e.at, 5000.0);
        assert_eq!((e.meas, e.d, e.old), (4890.0, 110.0, 112.0));
        assert!(!e.dropped);

        assert!(parser.feed("calib: verify start"));
        assert!(parser.feed("calib: verify ch0 max=13.0 @set=0 rmse=3.0"));
        assert_eq!(parser.report.phase, "verify");

        // 复检阶段的重测行（含 drop）
        assert!(parser.feed("calib: vretry 1/2 n=1"));
        assert!(parser.feed("calib: vrtry7 ch=0 set=7000 meas=7013.0 d=13.0 old=22.0 drop"));
        assert_eq!(parser.report.phase, "verify");
        assert!(parser.report.errors.is_empty());
        let e = &parser.report.retries[1];
        assert!(e.verify);
        assert_eq!((e.round, e.idx, e.ch, e.at), (1, 7, 0, 7000.0));
        assert!(e.dropped);
        assert_eq!(parser.report.fw_params.as_ref().unwrap().retry, 3);
    }

    #[test]
    fn retry_round_keeps_run_passing_with_fresh_resid() {
        // 首轮残差超标 → 重测 → 第二轮达标的完整流程：末态仍应 PASS，
        // 且卡片显示重测后的残差/复检值（不是首轮超标的那个数）
        let mut parser = Parser::new();
        assert!(parser.feed("calib: role DUT"));
        assert!(parser.feed("calib: ch0 resid max=118.0 @ideal=12000 rmse=25.0"));
        assert_eq!(parser.report.channels[0].resid.as_ref().unwrap().max, 118.0);

        assert!(parser.feed("calib: retry 1/3 n=2"));
        assert!(parser.feed("calib: rtry12 ch=0 ideal=12000 meas=11882.0 d=118.0 old=121.0"));
        assert!(parser.feed("calib: rtry40 ch=1 ideal=40000 meas=39991.0 d=9.0 old=60.0"));
        assert!(parser.feed("calib: vretry 1/3 n=1"));
        assert!(parser.feed("calib: vrtry58 ch=0 set=58000 meas=58024.0 d=24.0 old=47.0 drop"));

        assert!(parser.feed("calib: ch0 resid max=1.5 @ideal=37000 rmse=0.5"));
        assert!(parser.feed("calib: saved"));
        assert!(parser.feed("calib: verify start"));
        assert!(parser.feed("calib: verify ch0 max=8.0 @set=62000 rmse=3.0"));
        assert!(parser.feed("calib: verify PASS"));

        let r = &parser.report;
        assert!(r.errors.is_empty());
        assert_eq!(r.retries.len(), 3);
        assert_eq!(r.retries.iter().filter(|e| e.dropped).count(), 1);
        assert_eq!(r.retries.iter().map(|e| e.ch).collect::<Vec<_>>(), vec![0, 1, 0]);
        assert_eq!(r.phase, "done");
        assert_eq!(r.verify_pass, Some(true));
        assert!(!r.verify_skipped);
        assert_eq!(r.channels[0].resid.as_ref().unwrap().max, 1.5);
        assert_eq!(r.channels[0].resid_at, Some(37000));
        assert_eq!(r.channels[0].verify.as_ref().unwrap().max, 8.0);
    }
}
