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
          player: "p1",
          action: bindingAction, 
          input: inputId 
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

  const formatKeyName = (val: string) => {
    return val
      .replace('Key_', '')
      .replace('Button ', 'Btn ')
      .replace('Shoulder', 'Bump')
      .replace('Trigger', 'Trig')
      .replace('DPad ', 'D-')
      .replace('Left Stick', 'L-Stick')
      .replace('Right Stick', 'R-Stick');
  };

  const Button = ({ action, x, y, w = 100, h = 100, color = "var(--accent-cyan)" }: { action: Action, x: number, y: number, w?: number, h?: number, color?: string }) => {
    const isBinding = bindingAction === action;
    return (
    <g 
      style={{ cursor: 'pointer' }}
      onMouseEnter={() => setHoveredAction(action)}
      onMouseLeave={() => setHoveredAction(null)}
      onClick={() => setBindingAction(action)}
    >
      <rect 
        x={x - w/2} y={y - h/2} width={w} height={h} rx="16"
        fill={isBinding ? color : "rgba(0,0,0,0.8)"}
        stroke={hoveredAction === action ? color : "rgba(255,255,255,0.2)"}
        strokeWidth="6"
        style={{ transition: 'all 0.1s ease', filter: 'drop-shadow(4px 4px 0 var(--accent-red))' }}
      />
      <text 
        x={x} y={y - Math.min(12, h/6)} 
        textAnchor="middle" 
        alignmentBaseline="middle"
        fill={isBinding ? "#000" : "#fff"}
        style={{ fontSize: Math.min(26, w/3), fontFamily: 'var(--font-main)', fontWeight: 800, pointerEvents: 'none' }}
      >
        {action}
      </text>
      <text 
        x={x}
        y={y + Math.min(16, h/5)} 
        textAnchor="middle" 
        alignmentBaseline="middle"
        fill={isBinding ? "#000" : "var(--accent-yellow)"}
        style={{ fontSize: 13, fontFamily: 'var(--font-mono)', fontWeight: 700, pointerEvents: 'none' }}
      >
        {formatKeyName(getMappedValue(action))}
      </text>
    </g>
  )};

  return (
    <div className="remapper-container arcade-box" style={{ padding: '40px', marginTop: '20px', position: 'relative' }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '40px', alignItems: 'center' }}>
        <div>
          <h3 style={{ color: 'var(--accent-red)', fontSize: '32px', textTransform: 'uppercase', fontStyle: 'italic', textShadow: '2px 2px 0 #000' }}>Visual Mapping Tool</h3>
          <p style={{ fontSize: '16px', color: '#ccc', textTransform: 'uppercase', letterSpacing: '2px', fontWeight: 600, marginTop: '8px' }}>
            Click a button to start binding. Press any key to assign.
          </p>
        </div>
        {bindingAction && (
          <div className="text-gradient" style={{ fontSize: 32, fontFamily: 'var(--font-main)', fontWeight: 900, animation: 'pulse 1s infinite' }}>
            BINDING: {bindingAction.toUpperCase()}
          </div>
        )}
      </div>

      <svg width="100%" height="auto" viewBox="0 0 1100 450" preserveAspectRatio="xMidYMin meet" style={{ filter: 'drop-shadow(0 0 15px rgba(0,0,0,0.8))' }}>
        {/* 🕹️ D-Pad / Stick Area */}
        <g transform="translate(220, 240)">
          <line x1="-120" y1="0" x2="120" y2="0" stroke="rgba(255,255,255,0.08)" strokeWidth="16" strokeLinecap="round" />
          <line x1="0" y1="-120" x2="0" y2="120" stroke="rgba(255,255,255,0.08)" strokeWidth="16" strokeLinecap="round" />
          <Button action="Up" x={0} y={-130} w={100} h={100} />
          <Button action="Down" x={0} y={130} w={100} h={100} />
          <Button action="Left" x={-130} y={0} w={100} h={100} />
          <Button action="Right" x={130} y={0} w={100} h={100} />
        </g>

        {/* 🔘 Action Buttons (Arcade Layout - Vewlix style) */}
        <g transform="translate(560, 100)">
          {/* Top Row */}
          <Button action="LP" x={0} y={50} color="var(--accent-yellow)" w={110} h={110} />
          <Button action="MP" x={160} y={0} color="var(--accent-yellow)" w={110} h={110} />
          <Button action="HP" x={320} y={0} color="var(--accent-yellow)" w={110} h={110} />
          
          {/* Bottom Row */}
          <Button action="LK" x={0} y={190} color="var(--accent-red)" w={110} h={110} />
          <Button action="MK" x={160} y={140} color="var(--accent-red)" w={110} h={110} />
          <Button action="HK" x={320} y={140} color="var(--accent-red)" w={110} h={110} />
        </g>

        {/* ⚙️ Meta Buttons (Start / Select placed distinctly) */}
        <g transform="translate(1000, 100)">
          <text x={0} y={-30} fill="rgba(255,255,255,0.2)" fontSize="16" fontWeight="800" textAnchor="middle" style={{fontFamily: 'var(--font-main)'}}>SYSTEM</text>
          <Button action="Start" x={0} y={20} w={100} h={80} color="var(--accent-yellow)" />
          <Button action="Select" x={0} y={140} w={100} h={80} color="#fff" />
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
