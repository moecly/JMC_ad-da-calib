import { useEffect, useState } from "react";
import { useBackend } from "./hooks/useBackend";
import { ConnectionBar } from "./components/ConnectionBar";
import { ControlPanel } from "./components/ControlPanel";
import { CalibResult } from "./components/CalibResult";
import { ErrorChart } from "./components/ErrorChart";
import { LogConsole } from "./components/LogConsole";
import { ManualPanel } from "./components/ManualPanel";
import { OverviewTab } from "./components/OverviewTab";
import { VerifyTab } from "./components/VerifyTab";
import { IconCalib, IconLog, IconManual, IconOverview, IconVerify } from "./components/icons";

const TABS = [
  { label: "概览", Icon: IconOverview },
  { label: "自动校准", Icon: IconCalib },
  { label: "复检", Icon: IconVerify },
  { label: "手动校准", Icon: IconManual },
  { label: "日志", Icon: IconLog },
];

function App() {
  const { status, report, lines, probes, probeName, refreshProbes, clearAll } = useBackend();
  const [tab, setTab] = useState(0);

  useEffect(() => {
    void refreshProbes();
  }, [refreshProbes]);

  return (
    <div className="flex h-screen bg-canvas text-ink">
      <aside className="flex w-[220px] shrink-0 flex-col border-r border-line bg-sidebar">
        <div className="px-4 pb-2 pt-4">
          <div className="text-[15px] font-semibold">AD/DA 校准</div>
          <div className="text-[11px] text-ink-faint">上位机</div>
        </div>
        <nav className="flex flex-col gap-0.5 px-2">
          {TABS.map(({ label, Icon }, i) => (
            <button
              key={label}
              type="button"
              onClick={() => setTab(i)}
              aria-current={tab === i ? "page" : undefined}
              className={`flex h-8 items-center gap-2 rounded-md px-2.5 text-[13px] transition-colors ${
                tab === i
                  ? "bg-accent-soft font-medium text-accent"
                  : "text-ink-soft hover:bg-black/[0.04] hover:text-ink"
              }`}
            >
              <Icon className="h-4 w-4 shrink-0" />
              {label}
            </button>
          ))}
        </nav>
        <div className="mt-auto px-4 py-3 text-[11px] text-ink-faint">{probeName ?? "未选择探针"}</div>
      </aside>

      <main className="flex min-w-0 flex-1 flex-col">
        <ConnectionBar probes={probes} status={status} probeName={probeName} onRefresh={refreshProbes} />
        {tab === 4 ? (
          <div className="min-h-0 flex-1">
            <LogConsole lines={lines} onClear={clearAll} />
          </div>
        ) : (
          <div className="min-h-0 flex-1 overflow-y-auto">
            <div className="mx-auto w-full max-w-[1100px] p-6">
              {tab === 0 && <OverviewTab status={status} report={report} probeName={probeName} />}
              {tab === 1 && (
                <div className="flex flex-col gap-4">
                  <ControlPanel connected={status.connected} report={report} />
                  <CalibResult report={report} />
                  <ErrorChart report={report} mode="sweep" />
                </div>
              )}
              {tab === 2 && <VerifyTab connected={status.connected} report={report} />}
              {tab === 3 && <ManualPanel connected={status.connected} report={report} />}
            </div>
          </div>
        )}
      </main>
    </div>
  );
}

export default App;
