import { useState, useEffect } from "react";
import { invoke } from "@tauri-apps/api/core";

interface Props {
  onMappingSaved: () => void;
  currentMappings: { key: string; value: string }[];
}

type Action = "Up" | "Down" | "Left" | "Right" | "LP" | "MP" | "HP" | "LK" | "MK" | "HK" | "Start" | "Select";


export function Remapper({ onMappingSaved, currentMappings }: Props) {
  const [bindingAction, setBindingAction] = useState<Action | null>(null);
  const [hoveredAction, setHoveredAction] = useState<Action | null>(null);

  const getMappedValue = (action: Action) => {
    // Note: mappings are stored as "p1_mapping=Action,Input"
    // The backend returns them as { key: "p1_mapping", value: "Up,Key_W" }
    const entry = currentMappings.find(m => m.value.startsWith(action + ","));
    return entry ? entry.value.split(",")[1] : "---";
  };

  useEffect(() => {
    if (!bindingAction) return;

    const handleKey = async (e: KeyboardEvent) => {
      e.preventDefault();
      const inputId = `Key_${e.code.replace('Key', '')}`;
      
      try {
        await invoke("save_mapping", { 
          key: "p1_mapping", 
          value: `${bindingAction},${inputId}` 
        });
        setBindingAction(null);
        onMappingSaved();
      } catch (err) {
        console.error("Failed to save mapping", err);
      }
    };

    window.addEventListener("keydown", handleKey);
    return () => window.removeEventListener("keydown", handleKey);
  }, [bindingAction, onMappingSaved]);

  const Button = ({ action, x, y, r = 25, color = "var(--accent-cyan)" }: { action: Action, x: number, y: number, r?: number, color?: string }) => (
    <g 
      style={{ cursor: 'pointer' }}
      onMouseEnter={() => setHoveredAction(action)}
      onMouseLeave={() => setHoveredAction(null)}
      onClick={() => setBindingAction(action)}
    >
      <circle 
        cx={x} cy={y} r={r} 
        fill={bindingAction === action ? color : "rgba(255,255,255,0.05)"}
        stroke={hoveredAction === action ? color : "rgba(255,255,255,0.2)"}
        strokeWidth="2"
        style={{ transition: 'all 0.2s ease' }}
      />
      <text 
        x={x} y={y + 5} 
        textAnchor="middle" 
        fill={bindingAction === action ? "#000" : "#fff"}
        style={{ fontSize: 12, fontWeight: 900, pointerEvents: 'none' }}
      >
        {action}
      </text>
      <text 
        x={x} y={y + r + 15} 
        textAnchor="middle" 
        fill="var(--text-secondary)"
        style={{ fontSize: 10, fontFamily: 'var(--font-mono)' }}
      >
        {getMappedValue(action)}
      </text>
    </g>
  );

  return (
    <div className="remapper-container glass" style={{ padding: 40, marginTop: 20, position: 'relative' }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 30 }}>
        <div>
          <h3 style={{ color: 'var(--accent-cyan)' }}>Visual Mapping Tool</h3>
          <p style={{ fontSize: 12, opacity: 0.6 }}>Click a button to start binding. Press any key to assign.</p>
        </div>
        {bindingAction && (
          <div className="text-gradient" style={{ fontSize: 14, fontWeight: 900, animation: 'pulse 1s infinite' }}>
            WAITING FOR INPUT: {bindingAction.toUpperCase()} ...
          </div>
        )}
      </div>

      <svg width="100%" height="300" viewBox="0 0 600 300" style={{ filter: 'drop-shadow(0 0 10px rgba(0,0,0,0.5))' }}>
        {/* 🕹️ D-Pad / Stick Area */}
        <g transform="translate(100, 150)">
          <line x1="-50" y1="0" x2="50" y2="0" stroke="rgba(255,255,255,0.1)" strokeWidth="1" />
          <line x1="0" y1="-50" x2="0" y2="50" stroke="rgba(255,255,255,0.1)" strokeWidth="1" />
          <Button action="Up" x={0} y={-50} r={20} />
          <Button action="Down" x={0} y={50} r={20} />
          <Button action="Left" x={-50} y={0} r={20} />
          <Button action="Right" x={50} y={0} r={20} />
          <circle cx="0" cy="0" r="10" fill="var(--accent-magenta)" opacity="0.3" />
        </g>

        {/* 🔘 Action Buttons (Arcade Layout) */}
        <g transform="translate(350, 120)">
          {/* Top Row */}
          <Button action="LP" x={0} y={0} color="var(--accent-cyan)" />
          <Button action="MP" x={70} y={-10} color="var(--accent-cyan)" />
          <Button action="HP" x={140} y={-15} color="var(--accent-cyan)" />
          
          {/* Bottom Row */}
          <Button action="LK" x={10} y={80} color="var(--accent-magenta)" />
          <Button action="MK" x={80} y={70} color="var(--accent-magenta)" />
          <Button action="HK" x={150} y={65} color="var(--accent-magenta)" />
        </g>

        {/* ⚙️ Meta Buttons */}
        <g transform="translate(520, 40)">
          <Button action="Start" x={0} y={0} r={18} color="var(--accent-yellow)" />
          <Button action="Select" x={0} y={50} r={18} color="rgba(255,255,255,0.5)" />
        </g>
      </svg>

      <style>{`
        @keyframes pulse {
          0% { opacity: 0.5; }
          50% { opacity: 1; }
          100% { opacity: 0.5; }
        }
      `}</style>
    </div>
  );
}
