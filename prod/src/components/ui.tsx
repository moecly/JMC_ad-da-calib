import type {
  ButtonHTMLAttributes,
  InputHTMLAttributes,
  ReactNode,
  SelectHTMLAttributes,
} from "react";

export function Card({ children, className = "" }: { children: ReactNode; className?: string }) {
  return (
    <section className={`rounded-xl border border-line bg-panel p-5 ${className}`}>{children}</section>
  );
}

export function CardTitle({ children, right }: { children: ReactNode; right?: ReactNode }) {
  return (
    <div className="mb-4 flex items-center justify-between gap-3">
      <h2 className="text-[13px] font-semibold text-ink">{children}</h2>
      {right}
    </div>
  );
}

export type Tone = "green" | "red" | "amber" | "slate" | "blue";

const toneClass: Record<Tone, string> = {
  green: "bg-ok-soft text-ok-ink",
  red: "bg-danger-soft text-danger-ink",
  amber: "bg-warn-soft text-warn-ink",
  blue: "bg-accent-soft text-accent",
  slate: "bg-field text-ink-soft",
};

export function Pill({ label, tone = "slate" }: { label: ReactNode; tone?: Tone }) {
  return (
    <span
      className={`inline-flex items-center gap-1 rounded-full px-2 py-0.5 text-[11px] font-medium ${toneClass[tone]}`}
    >
      {label}
    </span>
  );
}

export function Button({
  variant = "secondary",
  size = "md",
  className = "",
  type = "button",
  ...props
}: ButtonHTMLAttributes<HTMLButtonElement> & {
  variant?: "primary" | "secondary" | "ghost" | "danger";
  size?: "sm" | "md" | "lg";
}) {
  const base =
    "inline-flex select-none items-center justify-center gap-1.5 rounded-md font-medium transition-colors duration-150 disabled:cursor-not-allowed disabled:opacity-40";
  const sizes = {
    sm: "h-7 px-2.5 text-xs",
    md: "h-8 px-3 text-[13px]",
    lg: "h-9 px-4 text-sm",
  };
  const variants = {
    primary: "bg-accent text-white hover:bg-accent-strong",
    secondary: "border border-line-strong bg-panel text-ink hover:bg-field",
    ghost: "text-ink-soft hover:bg-field hover:text-ink",
    danger: "border border-danger/40 bg-danger-soft text-danger-ink hover:bg-danger/15",
  };
  return <button type={type} className={`${base} ${sizes[size]} ${variants[variant]} ${className}`} {...props} />;
}

export function Input({ className = "", ...props }: InputHTMLAttributes<HTMLInputElement>) {
  return (
    <input
      className={`h-8 rounded-md border border-line-strong bg-field px-2.5 text-[13px] text-ink placeholder:text-ink-faint transition-colors focus:border-accent focus:bg-panel focus:outline-none disabled:cursor-not-allowed disabled:opacity-50 ${className}`}
      {...props}
    />
  );
}

export function Select({ className = "", ...props }: SelectHTMLAttributes<HTMLSelectElement>) {
  return (
    <select
      className={`h-8 rounded-md border border-line-strong bg-field px-2 text-[13px] text-ink transition-colors focus:border-accent focus:bg-panel focus:outline-none disabled:cursor-not-allowed disabled:opacity-50 ${className}`}
      {...props}
    />
  );
}

export function Field({
  label,
  children,
  suffix,
}: {
  label: string;
  children: ReactNode;
  suffix?: string;
}) {
  return (
    <label className="flex items-center gap-2 text-xs text-ink-soft">
      <span className="whitespace-nowrap">{label}</span>
      {children}
      {suffix && <span className="text-ink-faint">{suffix}</span>}
    </label>
  );
}

export function Switch({
  checked,
  onChange,
  label,
  disabled,
}: {
  checked: boolean;
  onChange: (v: boolean) => void;
  label?: ReactNode;
  disabled?: boolean;
}) {
  return (
    <label className="flex items-center gap-2 text-xs text-ink-soft">
      <button
        type="button"
        role="switch"
        aria-checked={checked}
        disabled={disabled}
        onClick={() => onChange(!checked)}
        className={`relative h-[18px] w-8 rounded-full transition-colors disabled:cursor-not-allowed disabled:opacity-50 ${
          checked ? "bg-accent" : "bg-line-strong"
        }`}
      >
        <span
          className={`absolute top-[2px] h-[14px] w-[14px] rounded-full bg-white transition-transform ${
            checked ? "left-[16px]" : "left-[2px]"
          }`}
        />
      </button>
      {label}
    </label>
  );
}

export function Segmented({
  options,
  value,
  onChange,
}: {
  options: { value: number; label: ReactNode }[];
  value: number;
  onChange: (v: number) => void;
}) {
  return (
    <div className="inline-flex rounded-md bg-field p-0.5">
      {options.map((o) => (
        <button
          key={o.value}
          type="button"
          aria-pressed={value === o.value}
          onClick={() => onChange(o.value)}
          className={`h-6 rounded px-2.5 text-xs transition-colors ${
            value === o.value ? "bg-panel font-medium text-ink" : "text-ink-soft hover:text-ink"
          }`}
        >
          {o.label}
        </button>
      ))}
    </div>
  );
}

export function StatusDot({ state }: { state: string }) {
  const cls =
    state === "attached"
      ? "bg-ok"
      : state === "searching"
        ? "bg-warn animate-pulse"
        : state === "error"
          ? "bg-danger"
          : state === "connecting"
            ? "bg-accent animate-pulse"
            : "bg-ink-faint";
  return <span className={`h-2.5 w-2.5 shrink-0 rounded-full ${cls}`} />;
}
