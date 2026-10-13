import { useEffect, useMemo, useRef, useState } from "react";
import type { LogLine } from "../types";
import { Button, Input, Switch } from "./ui";

interface Props {
  lines: LogLine[];
  onClear: () => Promise<void>;
}

const levelClass = (level: string) => {
  switch (level) {
    case "ERROR":
    case "FATAL":
      return "text-danger-ink";
    case "WARNING":
      return "text-warn-ink";
    case "INFO":
      return "text-ink";
    case "DEBUG":
      return "text-ink-soft";
    default:
      return "text-ink-faint";
  }
};

export function LogConsole({ lines, onClear }: Props) {
  const [filter, setFilter] = useState("");
  const [autoScroll, setAutoScroll] = useState(true);
  const bottomRef = useRef<HTMLDivElement>(null);

  const shown = useMemo(() => {
    if (!filter) return lines;
    const needle = filter.toLowerCase();
    return lines.filter((l) => l.raw.toLowerCase().includes(needle));
  }, [lines, filter]);

  useEffect(() => {
    if (autoScroll) bottomRef.current?.scrollIntoView({ block: "end" });
  }, [shown, autoScroll]);

  return (
    <div className="flex h-full flex-col bg-panel">
      <div className="flex flex-wrap items-center gap-2 border-b border-line px-3 py-2 text-xs">
        <span className="font-semibold text-ink">日志</span>
        <span className="text-ink-faint">{shown.length} 行</span>
        <Input
          className="ml-2 w-40"
          placeholder="过滤…"
          value={filter}
          onChange={(e) => setFilter(e.currentTarget.value)}
        />
        <Switch checked={autoScroll} onChange={setAutoScroll} label="自动滚动" />
        <div className="ml-auto flex gap-2">
          <Button size="sm" onClick={() => void onClear()}>
            清空
          </Button>
        </div>
      </div>
      <div className="flex-1 overflow-auto p-3 font-mono text-xs leading-relaxed">
        {shown.map((l, i) => (
          <div key={i} className={levelClass(l.level)}>
            {l.raw}
          </div>
        ))}
        <div ref={bottomRef} />
      </div>
    </div>
  );
}
