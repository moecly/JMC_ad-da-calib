import { Select } from "./ui";

export function ChannelLinkMatrix(props: {
  enabled: boolean[];
  daSel: number[];
  dutName: string;
  refName: string;
  disabled: boolean;
  onToggle: (ch: number, on: boolean) => void;
  onDaSelect: (ch: number, da: number) => void;
}) {
  const { enabled, daSel, dutName, refName, disabled, onToggle, onDaSelect } = props;
  return (
    <div className="flex flex-col gap-1">
      <div className="text-xs text-ink-soft">{`按连线选择通道（${dutName} chN ← REF ${refName} chM）`}</div>
      {[0, 1, 2, 3].map((ch) => (
        <div key={ch} className="flex items-center gap-3">
          <label className="flex w-28 items-center gap-2 text-[13px] text-ink">
            <input
              type="checkbox"
              className="h-4 w-4 accent-accent"
              checked={enabled[ch]}
              disabled={disabled}
              onChange={(e) => onToggle(ch, e.currentTarget.checked)}
            />
            {dutName} ch{ch}
          </label>
          <Select
            className="w-32"
            value={daSel[ch]}
            disabled={disabled}
            onChange={(e) => onDaSelect(ch, Number(e.currentTarget.value))}
          >
            {[0, 1, 2, 3].map((m) => (
              <option key={m} value={m}>
                {refName} ch{m}
              </option>
            ))}
          </Select>
        </div>
      ))}
    </div>
  );
}
