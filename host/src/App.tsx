import { useEffect } from "react";
import { useBackend } from "./hooks/useBackend";
import { ConnectionBar } from "./components/ConnectionBar";
import { ControlPanel } from "./components/ControlPanel";
import { CalibResult } from "./components/CalibResult";
import { ErrorChart } from "./components/ErrorChart";
import { LogConsole } from "./components/LogConsole";

function App() {
  const { status, report, lines, probes, probeName, refreshProbes, clearAll } = useBackend();

  useEffect(() => {
    void refreshProbes();
  }, [refreshProbes]);

  return (
    <div className="flex h-screen flex-col bg-slate-950 text-slate-100">
      <ConnectionBar
        probes={probes}
        connected={status.connected}
        statusMessage={status.message}
        probeName={probeName}
        onRefresh={refreshProbes}
      />
      <div className="flex min-h-0 flex-1">
        <div className="flex min-h-0 flex-1 flex-col overflow-auto border-r border-slate-800">
          <ControlPanel connected={status.connected} report={report} />
          <CalibResult report={report} />
          <ErrorChart report={report} />
        </div>
        <div className="flex min-h-0 w-[42%] flex-col">
          <LogConsole lines={lines} onClear={clearAll} />
        </div>
      </div>
    </div>
  );
}

export default App;
