import { useEffect, useMemo, useRef, useState } from "react";
import { open } from "@tauri-apps/plugin-dialog";
import type { LogLine } from "../types";
import * as api from "../lib/ipc";

interface Props {
  lines: LogLine[];
  onClear: () => Promise<void>;
}

const levelClass = (level: string) => {
  switch (level) {
    case "ERROR":
    case "FATAL":
      return "text-red-400";
    case "WARNING":
      return "text-amber-400";
    case "INFO":
      return "text-slate-200";
    case "DEBUG":
      return "text-slate-400";
    default:
      return "text-slate-500";
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

  const loadFile = async () => {
    const picked = await open({
      multiple: false,
      filters: [{ name: "日志", extensions: ["log", "txt"] }],
    });
    if (typeof picked === "string") await api.loadLogFile(picked);
  };

  return (
    <div className="flex h-full flex-col">
      <div className="flex items-center gap-2 border-b border-slate-700 px-3 py-2 text-xs">
        <span className="font-semibold text-slate-200">日志</span>
        <span className="text-slate-500">{shown.length} 行</span>
        <input
          className="ml-2 w-40 rounded border border-slate-600 bg-slate-800 px-2 py-0.5 text-slate-200"
          placeholder="过滤…"
          value={filter}
          onChange={(e) => setFilter(e.currentTarget.value)}
        />
        <label className="flex items-center gap-1 text-slate-400">
          <input type="checkbox" checked={autoScroll} onChange={(e) => setAutoScroll(e.currentTarget.checked)} />
          自动滚动
        </label>
        <div className="ml-auto flex gap-2">
          <button className="rounded border border-slate-600 px-2 py-0.5 text-slate-200" onClick={() => void loadFile()}>
            加载日志
          </button>
          <button className="rounded border border-slate-600 px-2 py-0.5 text-slate-200" onClick={() => void onClear()}>
            清空
          </button>
        </div>
      </div>
      <div className="flex-1 overflow-auto bg-slate-950 p-2 font-mono text-xs leading-relaxed">
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
