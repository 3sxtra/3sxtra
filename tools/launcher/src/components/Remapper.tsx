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

      <svg width="100%" height="300" viewBox="0 0 650 350" style={{ filter: 'drop-shadow(0 0 10px rgba(0,0,0,0.5))' }}>
        {/* 🕹️ D-Pad / Stick Area */}
        <g transform="translate(140, 180)">
          <line x1="-60" y1="0" x2="60" y2="0" stroke="rgba(255,255,255,0.1)" strokeWidth="1" />
          <line x1="0" y1="-60" x2="0" y2="60" stroke="rgba(255,255,255,0.1)" strokeWidth="1" />
          <Button action="Up" x={0} y={-60} r={22} />
          <Button action="Down" x={0} y={60} r={22} />
          <Button action="Left" x={-60} y={0} r={22} />
          <Button action="Right" x={60} y={0} r={22} />
          <circle cx="0" cy="0" r="12" fill="var(--accent-magenta)" opacity="0.3" />
        </g>

        {/* 🔘 Action Buttons (Arcade Layout - Vewlix style) */}
        <g transform="translate(320, 130)">
          {/* Top Row */}
          <Button action="LP" x={0} y={30} color="var(--accent-cyan)" r={24} />
          <Button action="MP" x={70} y={15} color="var(--accent-cyan)" r={24} />
          <Button action="HP" x={140} y={15} color="var(--accent-cyan)" r={24} />
          
          {/* Bottom Row */}
          <Button action="LK" x={0} y={100} color="var(--accent-magenta)" r={24} />
          <Button action="MK" x={70} y={85} color="var(--accent-magenta)" r={24} />
          <Button action="HK" x={140} y={85} color="var(--accent-magenta)" r={24} />
        </g>

        {/* ⚙️ Meta Buttons */}
        <g transform="translate(560, 60)">
          <Button action="Start" x={0} y={0} r={16} color="var(--accent-yellow)" />
          <Button action="Select" x={0} y={60} r={16} color="rgba(255,255,255,0.5)" />
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
