import { useEffect, useState } from "react";
import { useBackend } from "./hooks/useBackend";
import { ConnectionBar } from "./components/ConnectionBar";
import { ControlPanel } from "./components/ControlPanel";
import { CalibResult } from "./components/CalibResult";
import { ErrorChart } from "./components/ErrorChart";
import { LogConsole } from "./components/LogConsole";
import { ManualPanel } from "./components/ManualPanel";
import { OverviewTab } from "./components/OverviewTab";
import { Tabs } from "./components/ui";

const TABS = ["概览", "自动校准", "手动校准", "日志"];

function App() {
  const { status, report, lines, probes, probeName, refreshProbes, clearAll } = useBackend();
  const [tab, setTab] = useState(0);

  useEffect(() => {
    void refreshProbes();
  }, [refreshProbes]);

  return (
    <div className="flex h-screen flex-col bg-slate-950 text-slate-100">
      <ConnectionBar probes={probes} status={status} probeName={probeName} onRefresh={refreshProbes} />
      <Tabs tabs={TABS} active={tab} onChange={setTab} />
      <div className="flex min-h-0 flex-1 flex-col">
        {tab === 0 && (
          <div className="min-h-0 flex-1 overflow-y-auto p-4">
            <OverviewTab status={status} report={report} probeName={probeName} />
          </div>
        )}
        {tab === 1 && (
          <div className="min-h-0 flex-1 overflow-y-auto p-4">
            <ControlPanel connected={status.connected} report={report} />
            <CalibResult report={report} />
            <ErrorChart report={report} />
          </div>
        )}
        {tab === 2 && (
          <div className="min-h-0 flex-1 overflow-y-auto p-4">
            <ManualPanel connected={status.connected} report={report} />
          </div>
        )}
        {tab === 3 && (
          <div className="min-h-0 flex-1">
            <LogConsole lines={lines} onClear={clearAll} />
          </div>
        )}
      </div>
    </div>
  );
}

export default App;
