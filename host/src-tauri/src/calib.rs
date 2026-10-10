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
    pub samples: Vec<u16>,
    pub coeff: Option<[f64; 4]>,
    pub resid: Option<Stat>,
    pub resid_at: Option<u16>,
    pub verify: Option<Stat>,
    pub verify_at: Option<u16>,
    pub before: Option<Stat>,
    pub after: Option<Stat>,
}

#[derive(Clone, Default, Serialize)]
pub struct CalibReport {
    pub role: Option<String>,
    pub phase: String,
    pub points: u32,
    pub total_points: u32,
    pub grid: Vec<u16>,
    pub channels: Vec<ChannelReport>,
    pub saved: bool,
    pub verify_started: bool,
    pub verify_pass: Option<bool>,
    pub errors: Vec<String>,
}

static PT_RE: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"^(\d+) ideal=(\d+) raw=(\d+),(\d+),(\d+),(\d+)").unwrap());
static RESID_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r"^ch(\d+) resid max=([-0-9.eE+]+) @ideal=(\d+) rmse=([-0-9.eE+]+)").unwrap()
});
static VERIFY_CH_RE: LazyLock<Regex> = LazyLock::new(|| {
    Regex::new(r"ch(\d+) max=([-0-9.eE+]+) @set=(\d+) rmse=([-0-9.eE+]+)").unwrap()
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

fn parse_u16(s: &str) -> u16 {
    s.parse().unwrap_or(0)
}

fn parse_f64(s: &str) -> f64 {
    s.parse().unwrap_or(0.0)
}

pub struct Parser {
    pub report: CalibReport,
}

impl Default for Parser {
    fn default() -> Self {
        Self::new()
    }
}

impl Parser {
    pub fn new() -> Self {
        let channels = (0..CHANNELS)
            .map(|ch| ChannelReport {
                ch,
                ..Default::default()
            })
            .collect();
        Self {
            report: CalibReport {
                role: None,
                phase: "idle".to_string(),
                points: 0,
                total_points: TOTAL_POINTS,
                grid: Vec::new(),
                channels,
                saved: false,
                verify_started: false,
                verify_pass: None,
                errors: Vec::new(),
            },
        }
    }

    fn chan(&mut self, ch: usize) -> &mut ChannelReport {
        &mut self.report.channels[ch]
    }

    pub fn feed(&mut self, message: &str) -> bool {
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
            if role == "DUT" || role == "CHK" {
                self.report.phase = "sweep".to_string();
            }
            self.report.role = Some(role);
            return true;
        }

        if let Some(v) = rest.strip_prefix("pt") {
            if v.starts_with(|c: char| c.is_ascii_digit()) {
                return self.feed_pt(v);
            }
            return false;
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
                return true;
            }
            return false;
        }

        if rest == "ref not found" || rest == "ref timeout" || rest == "flash write failed" {
            self.report.errors.push(rest.to_string());
            self.report.phase = "fail".to_string();
            return true;
        }
        if rest.starts_with("fit failed ch=") {
            self.report.errors.push(rest.to_string());
            self.report.phase = "fail".to_string();
            return true;
        }

        false
    }

    fn feed_pt(&mut self, v: &str) -> bool {
        let Some(c) = PT_RE.captures(v) else {
            return false;
        };
        let idx = parse_u16(&c[1]) as usize;
        let ideal = parse_u16(&c[2]);
        put(&mut self.report.grid, idx, ideal);
        for (ch, capture) in (3..7).enumerate() {
            let value = parse_u16(&c[capture]);
            put(&mut self.chan(ch).samples, idx, value);
        }
        self.report.points += 1;
        self.report.phase = "sweep".to_string();
        true
    }

    fn feed_dump(&mut self, tail: &str, ch: Option<usize>) -> bool {
        let mut it = tail.split_whitespace();
        let Some(off) = it.next().and_then(|s| s.parse::<usize>().ok()) else {
            return false;
        };
        let values: Vec<u16> = it.map(parse_u16).collect();
        match ch {
            None => {
                for (i, v) in values.iter().enumerate() {
                    put(&mut self.report.grid, off + i, *v);
                }
            }
            Some(ch) if ch < CHANNELS => {
                let rep = self.chan(ch);
                for (i, v) in values.iter().enumerate() {
                    put(&mut rep.samples, off + i, *v);
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

fn stat_of(samples: &[u16], grid: &[u16], coeff: Option<&[f64; 4]>) -> Stat {
    let mut sum_sq = 0.0;
    let mut max = 0.0;
    for (s, g) in samples.iter().zip(grid.iter()) {
        let measured = match coeff {
            Some(c) => poly_eval(c, *s as f64),
            None => *s as f64,
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
            let unstable = if i == 5 { " unstable" } else { "" };
            lines.push(framed(
                "DoSweep",
                &format!(
                    "calib: pt{i} ideal={} raw={},{},{},{}{}",
                    grid[i], samples[i], samples[i], samples[i], samples[i], unstable
                ),
            ));
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
            lines.push(framed(
                "DoVerify",
                &format!(
                    "calib: vfy{i} set={} raw={},{},{},{}",
                    grid[i], samples[i], samples[i], samples[i], samples[i]
                ),
            ));
        }
        let mut verify = String::from("calib: verify");
        for ch in 0..CHANNELS {
            let sep = if ch == 0 { "" } else { "; " };
            verify.push_str(&format!("{sep}ch{ch} max=0.0 @set=0 rmse=0.0"));
        }
        lines.push(framed("DoVerify", &verify));
        lines.push(framed("DoVerify", "calib: verify PASS"));
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
        for ch in &r.channels {
            assert_eq!(ch.samples.len(), TOTAL_POINTS as usize);
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
}
