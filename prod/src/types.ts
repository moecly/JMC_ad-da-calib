export interface LogLine {
  level: string;
  file: string;
  line: number;
  func: string;
  message: string;
  raw: string;
}

export interface Stat {
  max: number;
  rmse: number;
}

export interface ChannelReport {
  ch: number;
  samples: number[];
  verify_samples: number[];
  coeff: [number, number, number, number] | null;
  resid: Stat | null;
  resid_at: number | null;
  verify: Stat | null;
  verify_at: number | null;
  before: Stat | null;
  after: Stat | null;
  valid: boolean;
}

export interface RetryEvent {
  verify: boolean;
  round: number;
  idx: number;
  ch: number;
  at: number;
  meas: number;
  d: number;
  old: number;
  dropped: boolean;
}

export interface RefInfo {
  board: string | null;
  version: string | null;
  has: boolean;
  valid_mask: number;
  no_reply: boolean;
}

export interface ManualPoint {
  ch: number;
  n: number;
  ideal: number;
  raw: number;
}

export interface ManualChannel {
  ch: number;
  points: ManualPoint[];
  coeff: [number, number, number, number] | null;
  resid: Stat | null;
  verify: Stat | null;
  fitted: boolean;
}

export interface ManualReport {
  on: boolean;
  channels: ManualChannel[];
  saved: boolean;
  verify_pass: boolean | null;
  errors: string[];
}

export interface FwParams {
  gate: number;
  resid: number;
  stable: number;
  settle: number;
  first: number;
  samp: number;
  refsamp: number;
  n: number;
  skipv: boolean;
  retry: number;
}

export interface CalibParams {
  residual_limit: number;
  check_gate: number;
  stable_code: number;
  settle_ms: number;
  first_settle_ms: number;
  sample_interval_ms: number;
  ref_sample_interval_ms: number;
  stable_count: number;
  skip_verify: boolean;
  retry: number;
}

export interface CalibReport {
  role: string | null;
  phase: string;
  points: number;
  total_points: number;
  grid: number[];
  verify_grid: number[];
  verify_points: number;
  retries: RetryEvent[];
  channels: ChannelReport[];
  saved: boolean;
  verify_started: boolean;
  verify_pass: boolean | null;
  verify_skipped: boolean;
  fw_params: FwParams | null;
  valid_mask: number;
  run_ch: number | null;
  ad_read: (number | null)[];
  errors: string[];
  device: string | null;
  app_version: string | null;
  board_dir: string | null;
  record_loaded: boolean;
  ref: RefInfo | null;
  manual: ManualReport;
}

export interface ProbeInfo {
  index: number;
  name: string;
  serial: string | null;
}

export interface PipelineConfig {
  probe_index: number;
  chip: string;
  dut_path: string;
  app_path: string;
  ch_mask: number;
  ref_da: number[];
  params: CalibParams;
}

export interface PipelineStatus {
  step: string;
  ok: boolean | null;
  message: string;
  log_lines: LogLine[];
  report: CalibReport;
  running: boolean;
}
