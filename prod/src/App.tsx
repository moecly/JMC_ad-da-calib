import { useEffect, useState } from "react";
import { useBackend } from "./hooks/useBackend";
import { ConnectionBar } from "./components/ConnectionBar";
import { LogConsole } from "./components/LogConsole";
import { PipelinePanel } from "./components/PipelinePanel";
import { CalibrationPanel } from "./components/CalibrationPanel";

function App() {
  const { snapshot, pipeline, lines, probes, refreshProbes, clearAll } = useBackend();
  const [logOpen, setLogOpen] = useState(false);

  useEffect(() => {
    void refreshProbes();
  }, [refreshProbes]);

  const status = snapshot?.status ?? {
    connected: false,
    state: "disconnected",
    message: "",
    hint: null,
    attempts: 0,
  };
  const probeName = snapshot?.probe_name ?? null;

  return (
    <div className="flex h-screen flex-col bg-canvas text-ink">
      <ConnectionBar
        probes={probes}
        status={status}
        probeName={probeName}
        onRefresh={refreshProbes}
        logOpen={logOpen}
        onToggleLog={() => setLogOpen((v) => !v)}
        logCount={lines.length}
      />
      <main className="min-h-0 flex-1 overflow-y-auto">
        <div className="mx-auto flex w-full max-w-[1100px] flex-col gap-4 p-6">
          <PipelinePanel probes={probes} pipeline={pipeline} onRefreshProbes={refreshProbes} />
          <CalibrationPanel pipeline={pipeline} />
        </div>
      </main>
      {logOpen && (
        <div className="fixed bottom-3 right-3 z-10 flex h-[280px] w-[540px] flex-col overflow-hidden rounded-xl border border-line bg-panel shadow-lg">
          <div className="flex items-center justify-between border-b border-line px-3 py-2 text-xs">
            <span className="font-semibold text-ink">
              日志 <span className="text-ink-faint">{lines.length} 行</span>
            </span>
            <button className="text-ink-faint hover:text-ink" onClick={() => setLogOpen(false)}>
              收起
            </button>
          </div>
          <div className="min-h-0 flex-1">
            <LogConsole lines={lines} onClear={clearAll} />
          </div>
        </div>
      )}
    </div>
  );
}

export default App;
