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
      <rect 
        x={x - r} y={y - r} width={r * 2} height={r * 2} rx="12"
        fill={bindingAction === action ? color : "rgba(0,0,0,0.8)"}
        stroke={hoveredAction === action ? color : "rgba(255,255,255,0.2)"}
        strokeWidth="6"
        style={{ transition: 'all 0.1s ease', filter: 'drop-shadow(4px 4px 0 var(--accent-red))' }}
      />
      <text 
        x={x} y={y} 
        textAnchor="middle" 
        alignmentBaseline="middle"
        fill={bindingAction === action ? "#000" : "#fff"}
        style={{ fontSize: 22, fontFamily: 'var(--font-main)', fontWeight: 700, pointerEvents: 'none' }}
      >
        {action}
      </text>
      <text 
        x={x}
        y={y + r + 30} 
        textAnchor="middle" 
        fill="var(--accent-yellow)"
        style={{ fontSize: 18, fontFamily: 'var(--font-mono)', fontWeight: 700 }}
      >
        {getMappedValue(action).replace('Key_', '')}
      </text>
    </g>
  );

  return (
    <div className="remapper-container arcade-box" style={{ padding: 40, marginTop: 20, position: 'relative' }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 30 }}>
        <div>
          <h3 style={{ color: 'var(--accent-red)' }}>Visual Mapping Tool</h3>
          <p style={{ fontSize: 12, opacity: 0.6 }}>Click a button to start binding. Press any key to assign.</p>
        </div>
        {bindingAction && (
          <div className="text-gradient" style={{ fontSize: 24, fontFamily: 'var(--font-main)', fontWeight: 900, animation: 'pulse 1s infinite' }}>
            BINDING: {bindingAction.toUpperCase()}
          </div>
        )}
      </div>

      <svg width="100%" height="auto" viewBox="0 0 1300 400" preserveAspectRatio="xMidYMin meet" style={{ filter: 'drop-shadow(0 0 10px rgba(0,0,0,0.5))' }}>
        {/* 🕹️ D-Pad / Stick Area */}
        <g transform="translate(240, 180)">
          <line x1="-130" y1="0" x2="130" y2="0" stroke="rgba(255,255,255,0.1)" strokeWidth="12" />
          <line x1="0" y1="-130" x2="0" y2="130" stroke="rgba(255,255,255,0.1)" strokeWidth="12" />
          <Button action="Up" x={0} y={-130} r={50} />
          <Button action="Down" x={0} y={130} r={50} />
          <Button action="Left" x={-130} y={0} r={50} />
          <Button action="Right" x={130} y={0} r={50} />
        </g>

        {/* 🔘 Action Buttons (Arcade Layout - Vewlix style) */}
        <g transform="translate(600, 60)">
          {/* Top Row */}
          <Button action="LP" x={0} y={60} color="var(--accent-yellow)" r={50} />
          <Button action="MP" x={200} y={20} color="var(--accent-yellow)" r={50} />
          <Button action="HP" x={400} y={20} color="var(--accent-yellow)" r={50} />
          
          {/* Bottom Row */}
          <Button action="LK" x={0} y={200} color="var(--accent-red)" r={50} />
          <Button action="MK" x={200} y={160} color="var(--accent-red)" r={50} />
          <Button action="HK" x={400} y={160} color="var(--accent-red)" r={50} />
        </g>

        {/* ⚙️ Meta Buttons */}
        <g transform="translate(1160, 60)">
          <Button action="Start" x={0} y={0} r={32} color="var(--accent-yellow)" />
          <Button action="Select" x={0} y={140} r={32} color="#fff" />
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
