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
  coeff: [number, number, number, number] | null;
  resid: Stat | null;
  resid_at: number | null;
  verify: Stat | null;
  verify_at: number | null;
  before: Stat | null;
  after: Stat | null;
}

export interface CalibReport {
  role: string | null;
  phase: string;
  points: number;
  total_points: number;
  grid: number[];
  channels: ChannelReport[];
  saved: boolean;
  verify_started: boolean;
  verify_pass: boolean | null;
  errors: string[];
}

export interface ProbeInfo {
  index: number;
  name: string;
  serial: string | null;
}

export interface Status {
  connected: boolean;
  message: string;
}

export interface Snapshot {
  status: Status;
  probe_name: string | null;
  lines: LogLine[];
  report: CalibReport;
}
