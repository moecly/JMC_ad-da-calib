const base = {
  width: 16,
  height: 16,
  viewBox: "0 0 24 24",
  fill: "none",
  stroke: "currentColor",
  strokeWidth: 1.6,
  strokeLinecap: "round" as const,
  strokeLinejoin: "round" as const,
};

export function IconOverview({ className }: { className?: string }) {
  return (
    <svg {...base} className={className}>
      <rect x="3" y="3" width="7" height="7" rx="1.5" />
      <rect x="14" y="3" width="7" height="7" rx="1.5" />
      <rect x="3" y="14" width="7" height="7" rx="1.5" />
      <rect x="14" y="14" width="7" height="7" rx="1.5" />
    </svg>
  );
}

export function IconCalib({ className }: { className?: string }) {
  return (
    <svg {...base} className={className}>
      <line x1="4" y1="7" x2="20" y2="7" />
      <line x1="4" y1="12" x2="20" y2="12" />
      <line x1="4" y1="17" x2="20" y2="17" />
      <circle cx="9" cy="7" r="2" />
      <circle cx="15" cy="12" r="2" />
      <circle cx="7" cy="17" r="2" />
    </svg>
  );
}

export function IconVerify({ className }: { className?: string }) {
  return (
    <svg {...base} className={className}>
      <path d="M12 3l7 3v6c0 4-3 7-7 9-4-2-7-5-7-9V6z" />
      <path d="M9 12l2 2 4-4" />
    </svg>
  );
}

export function IconManual({ className }: { className?: string }) {
  return (
    <svg {...base} className={className}>
      <path d="M4 20h4l10-10-4-4L4 16z" />
      <path d="M13.5 6.5l4 4" />
    </svg>
  );
}

export function IconLog({ className }: { className?: string }) {
  return (
    <svg {...base} className={className}>
      <line x1="8" y1="6" x2="20" y2="6" />
      <line x1="8" y1="12" x2="20" y2="12" />
      <line x1="8" y1="18" x2="20" y2="18" />
      <circle cx="4" cy="6" r="1" />
      <circle cx="4" cy="12" r="1" />
      <circle cx="4" cy="18" r="1" />
    </svg>
  );
}

export function IconParams({ className }: { className?: string }) {
  return (
    <svg {...base} className={className}>
      <circle cx="12" cy="12" r="3" />
      <path d="M12 3v2.5M12 18.5V21M3 12h2.5M18.5 12H21M5.6 5.6l1.8 1.8M16.6 16.6l1.8 1.8M18.4 5.6l-1.8 1.8M7.4 16.6l-1.8 1.8" />
    </svg>
  );
}
