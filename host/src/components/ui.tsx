import type { ReactNode } from "react";

export function Card({ children, className = "" }: { children: ReactNode; className?: string }) {
  return (
    <div className={`rounded-2xl border border-slate-800 bg-slate-900/60 p-4 backdrop-blur ${className}`}>
      {children}
    </div>
  );
}

export function CardTitle({ children, right }: { children: ReactNode; right?: ReactNode }) {
  return (
    <div className="mb-3 flex items-center justify-between gap-2">
      <span className="text-sm font-semibold text-slate-200">{children}</span>
      {right}
    </div>
  );
}

export type Tone = "green" | "red" | "amber" | "slate" | "blue";

const toneClass: Record<Tone, string> = {
  green: "bg-green-600/20 text-green-400 border-green-700",
  red: "bg-red-600/20 text-red-400 border-red-700",
  amber: "bg-amber-600/20 text-amber-400 border-amber-700",
  slate: "bg-slate-600/20 text-slate-400 border-slate-600",
  blue: "bg-blue-600/20 text-blue-400 border-blue-700",
};

export function Pill({ label, tone = "slate" }: { label: ReactNode; tone?: Tone }) {
  return (
    <span className={`inline-flex items-center rounded-full border px-2 py-0.5 text-xs ${toneClass[tone]}`}>
      {label}
    </span>
  );
}

export function Button({
  children,
  onClick,
  disabled,
  variant = "ghost",
  className = "",
}: {
  children: ReactNode;
  onClick?: () => void;
  disabled?: boolean;
  variant?: "primary" | "ghost" | "danger";
  className?: string;
}) {
  const base =
    "rounded-lg px-3 py-1.5 text-xs font-medium transition-colors disabled:cursor-not-allowed disabled:opacity-40";
  const variants = {
    primary: "bg-blue-600 text-white hover:bg-blue-500",
    ghost: "border border-slate-600 text-slate-200 hover:bg-slate-800",
    danger: "border border-red-700 bg-red-950/40 text-red-300 hover:bg-red-900/40",
  };
  return (
    <button className={`${base} ${variants[variant]} ${className}`} disabled={disabled} onClick={onClick}>
      {children}
    </button>
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
    <label className="flex items-center gap-2 text-xs text-slate-400">
      <span className="whitespace-nowrap">{label}</span>
      {children}
      {suffix && <span className="text-slate-500">{suffix}</span>}
    </label>
  );
}

export function Tabs({
  tabs,
  active,
  onChange,
}: {
  tabs: string[];
  active: number;
  onChange: (index: number) => void;
}) {
  return (
    <div role="tablist" className="flex gap-1 border-b border-slate-800 bg-slate-900/40 px-4">
      {tabs.map((t, i) => (
        <button
          key={t}
          role="tab"
          aria-selected={active === i}
          className={`-mb-px border-b-2 px-3 py-2 text-sm transition-colors ${
            active === i
              ? "border-blue-500 text-slate-100"
              : "border-transparent text-slate-400 hover:text-slate-200"
          }`}
          onClick={() => onChange(i)}
        >
          {t}
        </button>
      ))}
    </div>
  );
}
