import { useState, useEffect } from "react";
import { invoke } from "@tauri-apps/api/core";
import { getCurrentWindow } from "@tauri-apps/api/window";
import { listen } from "@tauri-apps/api/event";
import { Remapper } from "./components/Remapper";
import "./App.css";


type Tab = "news" | "settings" | "controls";

interface NewsItem {
  id: number;
  tag: string;
  title: string;
  date: string;
  image: string;
}

interface GameConfig {
  key: string;
  value: string;
}

// ── Settings Categories (matching config.h exactly) ────────────────
interface SettingDef {
  key: string;
  label: string;
  type: "bool" | "int" | "string" | "select";
  options?: { label: string; value: string }[];
}

interface SettingCategory {
  name: string;
  icon: string;
  settings: SettingDef[];
}

const SETTING_CATEGORIES: SettingCategory[] = [
  {
    name: "Window",
    icon: "🖥️",
    settings: [
      { key: "fullscreen", label: "Fullscreen", type: "bool" },
      { key: "window-width", label: "Window Width", type: "int" },
      { key: "window-height", label: "Window Height", type: "int" },
      { key: "vsync", label: "VSync", type: "bool" },
      { key: "skip-intro", label: "Skip Intro", type: "bool" },
    ]
  },
  {
    name: "Rendering",
    icon: "🎨",
    settings: [
      { key: "scale-mode", label: "Scale Mode", type: "select", options: [
        { label: "Nearest", value: "nearest" }, { label: "Linear", value: "linear" }
      ]},
      { key: "hd-stages", label: "HD Stages", type: "bool" },
      { key: "bezel-enabled", label: "Bezel", type: "bool" },
      { key: "shader-path", label: "Shader Path", type: "string" },
      { key: "draw-players-above-hud", label: "Players Above HUD", type: "bool" },
    ]
  },
  {
    name: "Netplay",
    icon: "🌐",
    settings: [
      { key: "lobby-display-name", label: "Display Name", type: "string" },
      { key: "netplay-ft", label: "First To (FT)", type: "int" },
      { key: "netplay-region-lock", label: "Region Lock", type: "bool" },
      { key: "netplay-max-ping", label: "Max Ping", type: "int" },
      { key: "netplay-block-wifi", label: "Block WiFi", type: "bool" },
      { key: "lobby-auto-connect", label: "Auto Connect Lobby", type: "bool" },
    ]
  },
  {
    name: "Training",
    icon: "🥊",
    settings: [
      { key: "training-hitboxes", label: "Hitboxes", type: "bool" },
      { key: "training-pushboxes", label: "Pushboxes", type: "bool" },
      { key: "training-hurtboxes", label: "Hurtboxes", type: "bool" },
      { key: "training-advantage", label: "Advantage Data", type: "bool" },
      { key: "training-stun", label: "Stun Info", type: "bool" },
      { key: "training-frame-meter", label: "Frame Meter", type: "bool" },
      { key: "training-inputs", label: "Input Display", type: "bool" },
    ]
  },
  {
    name: "Mods",
    icon: "🔧",
    settings: [
      { key: "modded-bgm-enabled", label: "Modded BGM", type: "bool" },
      { key: "modded-voice-enabled", label: "Modded Voice", type: "bool" },
      { key: "arcade-balance", label: "Arcade Balance", type: "bool" },
    ]
  },
];

const MOCK_NEWS: NewsItem[] = [
  { id: 1, tag: "PATCH NOTES", title: "v1.4.2: Netplay Stability & PS3 Fixes", date: "MAR 30, 2026", image: "https://images.unsplash.com/photo-1542751371-adc38448a05e?auto=format&fit=crop&q=80&w=800" },
  { id: 2, tag: "TOURNAMENT", title: "Friday Night FT3 - Registration Open", date: "APR 02, 2026", image: "https://images.unsplash.com/photo-1511512578047-dfb367046420?auto=format&fit=crop&q=80&w=800" },
  { id: 3, tag: "MOD PACK", title: "SF3 3SX: HD Stage Pack Vol. 1", date: "MAR 25, 2026", image: "https://images.unsplash.com/photo-1550745165-9bc0b252726f?auto=format&fit=crop&q=80&w=800" },
];

function App() {
  const [activeTab, setActiveTab] = useState<Tab>("news");
  const [isUpdating, setIsUpdating] = useState(false);
  const [progress, setProgress] = useState(0);
  const [status, setStatus] = useState("CHECKING SYSTEM...");
  const [configs, setConfigs] = useState<GameConfig[]>([]);
  const [mappings, setMappings] = useState<GameConfig[]>([]);
  const [isGameInstalled, setIsGameInstalled] = useState(true);

  const performUpdate = async () => {
    setStatus("CHECKING FOR UPDATES...");
    setIsUpdating(true);
    try {
      const manifest = await invoke("check_updates") as any;
      if (manifest && manifest.archives) {
        for (const arc of manifest.archives) {
          const exists = await invoke("check_file_exists", { path: arc.markerFile });
          
          if (arc.forceUpdate || !exists) {
            setProgress(0);
            setStatus(`DOWNLOADING ${arc.name.toUpperCase()}...`);
            await invoke("download_and_extract_archive", { 
              url: arc.url, 
              extractPath: arc.extractPath, 
              markerFile: arc.markerFile,
              stripRoot: arc.stripRoot,
              versionId: arc.versionId
            });
          }
        }

        setStatus("GAME UP TO DATE");
      } else {
        setStatus("SYSTEM READY");
      }
    } catch {
      setStatus("OFFLINE — PLAY AVAILABLE");
    }
    setIsUpdating(false);
    setProgress(100);
  };

  useEffect(() => {
    const unlistenProgress = listen<number>('download-progress', (event) => {
      setProgress(Math.round(event.payload));
    });

    async function init() {
      // 1. Load local settings
      try {
        const [loadedConfigs, loadedMappings] = await Promise.all([
          invoke("get_config") as Promise<GameConfig[]>,
          invoke("get_mappings") as Promise<GameConfig[]>,
        ]);
        setConfigs(loadedConfigs);
        setMappings(loadedMappings);
      } catch {
        // First run — no config exists yet
      }

      // 2. Check if game is installed
      let installed = true;
      try {
        installed = await invoke("is_game_installed");
      } catch {}

      setIsGameInstalled(installed);

      if (!installed) {
        setStatus("FIRST RUN - DOWNLOAD REQUIRED");
        return; // Wait for user to click button
      }

      // 3. Auto-patch if installed
      await performUpdate();
    }
    init();

    return () => {
      unlistenProgress.then(unlisten => unlisten());
    };
  }, []);



  // ── Config helpers ─────────────────────────────────────────────
  const getConfigValue = (key: string): string => {
    return configs.find(c => c.key === key)?.value ?? "";
  };

  const updateSetting = async (key: string, value: string) => {
    try {
      await invoke("save_config", { key, value });
      setConfigs(prev => {
        const exists = prev.some(c => c.key === key);
        if (exists) return prev.map(c => c.key === key ? { ...c, value } : c);
        return [...prev, { key, value }];
      });
    } catch (err) {
      console.error("Failed to save config", err);
    }
  };

  const toggleBool = (key: string) => {
    const current = getConfigValue(key);
    updateSetting(key, current === "1" || current === "true" ? "0" : "1");
  };

  const isBoolEnabled = (key: string): boolean => {
    const v = getConfigValue(key);
    return v === "1" || v === "true";
  };

  // ── Mappings helpers ───────────────────────────────────────────
  const updateMapping = async () => {
    try {
      const loaded = await invoke("get_mappings") as GameConfig[];
      setMappings(loaded);
    } catch (err) {
      console.error("Failed to reload mappings", err);
    }
  };

  const applyPreset = async (type: "XBOX" | "PS5" | "STICKS") => {
    setStatus(`APPLYING ${type} PRESET...`);

    const presets: Record<string, Record<string, string>> = {
      XBOX:   { "p1_mapping_up": "Up,DPad Up", "p1_mapping_down": "Down,DPad Down", "p1_mapping_left": "Left,DPad Left", "p1_mapping_right": "Right,DPad Right", "p1_mapping_lp": "LP,Button West",  "p1_mapping_mp": "MP,Button North",  "p1_mapping_hp": "HP,Right Shoulder",  "p1_mapping_lk": "LK,Button South", "p1_mapping_mk": "MK,Button East", "p1_mapping_hk": "HK,Right Trigger" },
      PS5:    { "p1_mapping_up": "Up,DPad Up", "p1_mapping_down": "Down,DPad Down", "p1_mapping_left": "Left,DPad Left", "p1_mapping_right": "Right,DPad Right", "p1_mapping_lp": "LP,Button West",  "p1_mapping_mp": "MP,Button North",  "p1_mapping_hp": "HP,Right Shoulder",  "p1_mapping_lk": "LK,Button South", "p1_mapping_mk": "MK,Button East", "p1_mapping_hk": "HK,Right Trigger" },
      STICKS: { "p1_mapping_up": "Up,DPad Up", "p1_mapping_down": "Down,DPad Down", "p1_mapping_left": "Left,DPad Left", "p1_mapping_right": "Right,DPad Right", "p1_mapping_lp": "LP,Button West",  "p1_mapping_mp": "MP,Button North",  "p1_mapping_hp": "HP,Button East",     "p1_mapping_lk": "LK,Left Shoulder","p1_mapping_mk": "MK,Right Shoulder","p1_mapping_hk": "HK,Right Trigger" },
    };

    try {
      const entries = Object.entries(presets[type]);
      for (const [key, value] of entries) {
        await invoke("save_mapping", { key, value });
      }
      await updateMapping();
      setStatus("PRESET APPLIED");
    } catch (err) {
      setStatus("PRESET FAILED");
      console.error(err);
    }
  };

  // ── Play / Update ──────────────────────────────────────────────
  const handlePlay = async () => {
    if (isUpdating) return; // Don't allow launch during update

    if (!isGameInstalled) {
      await performUpdate();
      return;
    }

    setStatus("LAUNCHING 3SX...");
    try {
      await invoke("launch_game");
      // Minimize the launcher after successful launch
      getCurrentWindow().minimize();
    } catch (err: any) {
      setStatus(`LAUNCH FAILED: ${err}`);
    }
  };


  // ── Render ─────────────────────────────────────────────────────
  const renderSettingRow = (s: SettingDef) => {
    const val = getConfigValue(s.key);
    return (
      <div key={s.key} style={{ display: 'flex', flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', padding: '2px 8px', borderBottom: '2px solid rgba(255,255,255,0.1)' }}>
        <span style={{ fontSize: 20, color: 'var(--text-unselected)', fontFamily: 'var(--font-main)', textTransform: 'uppercase' }}>{s.label}</span>
        {s.type === "bool" && (
          <button
            onClick={() => toggleBool(s.key)}
            style={{
              background: 'transparent',
              color: isBoolEnabled(s.key) ? 'var(--accent-yellow)' : 'var(--text-secondary)',
              border: 'none', padding: '0',
              fontWeight: 700, fontSize: 20, cursor: 'pointer', fontFamily: 'var(--font-main)',
              transition: 'all 0.1s ease', textTransform: 'uppercase'
            }}
          >
            {isBoolEnabled(s.key) ? "ON" : "OFF"}
          </button>
        )}
        {s.type === "int" && (
          <input
            type="number"
            value={val || "0"}
            onChange={e => updateSetting(s.key, e.target.value)}
            style={{
              background: 'transparent', border: 'none',
              width: '60px', color: 'var(--accent-yellow)', textAlign: 'right',
              fontFamily: 'var(--font-main)', fontSize: 20, boxSizing: 'border-box'
            }}
          />
        )}
        {s.type === "string" && (
          <input
            type="text"
            value={val || ""}
            onChange={e => updateSetting(s.key, e.target.value)}
            placeholder="-"
            style={{
              background: 'transparent', border: 'none',
              width: '100px', color: 'var(--accent-yellow)', textAlign: 'right',
              fontFamily: 'var(--font-main)', fontSize: 20, boxSizing: 'border-box'
            }}
          />
        )}
        {s.type === "select" && s.options && (
          <select
            value={val || s.options[0].value}
            onChange={e => updateSetting(s.key, e.target.value)}
            style={{
              background: 'transparent', border: 'none',
              color: 'var(--accent-yellow)', textAlign: 'right',
              fontFamily: 'var(--font-main)', fontSize: 20, boxSizing: 'border-box',
              appearance: 'none', paddingRight: '20px', cursor: 'pointer'
            }}
          >
            {s.options.map(o => <option key={o.value} value={o.value} style={{background: '#000'}}>{o.label.toUpperCase()}</option>)}
          </select>
        )}
      </div>
    );
  };

  return (
    <div className="app-container">
      <div className="app-overlay" />
      <div className="crt-overlay" />
      <div className="scanline" />

      {/* ── Frameless Window Controls ──────── */}
      <div data-tauri-drag-region className="window-controls">
        <button className="window-btn" onClick={() => getCurrentWindow().minimize()} title="Minimize">
          <svg width="12" height="12" viewBox="0 0 12 12"><rect y="5" width="12" height="1.5" fill="currentColor" /></svg>
        </button>
        <button className="window-btn" onClick={() => getCurrentWindow().toggleMaximize()} title="Maximize">
          <svg width="12" height="12" viewBox="0 0 12 12"><rect x="1" y="1" width="10" height="10" stroke="currentColor" strokeWidth="1.5" fill="none" /></svg>
        </button>
        <button className="window-btn close" onClick={() => getCurrentWindow().close()} title="Close">
          <svg width="12" height="12" viewBox="0 0 12 12"><path d="M1 1L11 11M11 1L1 11" stroke="currentColor" strokeWidth="1.5" /></svg>
        </button>
      </div>


      {/* ⬅️ Arcade Menu Sidebar */}
      <nav className="arcade-sidebar">
        <h1 className="system-direction-title">SYSTEM<br/>DIRECTION</h1>
        
        <button 
          className={`nav-item ${activeTab === 'news' ? 'active' : ''}`} 
          onClick={() => setActiveTab("news")}
        >
          LATEST NEWS
        </button>
        <button 
          className={`nav-item ${activeTab === 'settings' ? 'active' : ''}`} 
          onClick={() => setActiveTab("settings")}
        >
          GAME OPTION
        </button>
        <button 
          className={`nav-item ${activeTab === 'controls' ? 'active' : ''}`} 
          onClick={() => setActiveTab("controls")}
        >
          BUTTON CONFIG
        </button>
      </nav>

      {/* 📄 Main Content */}
      <main className="main-content">
        <header className="app-header" data-tauri-drag-region>
          <h1 className="app-title text-gradient">3rd Strike 3SXtra</h1>
          <div className="version">STABLE BUILD · V2.4.0-BETA</div>
        </header>

        <section className="content-panel">
          {/* ── News ───────────────────────────────── */}
          {activeTab === "news" && (
            <div className="news-feed">
              <h2 style={{ marginBottom: 20 }}>Latest News</h2>
              <div className="news-grid">
                {MOCK_NEWS.map(news => (
                  <div key={news.id} className="news-card arcade-box">
                    <img src={news.image} className="news-card-img" alt={news.title} />
                    <div className="news-card-tag">{news.tag}</div>
                    <h3 style={{ fontSize: 18 }}>{news.title}</h3>
                    <p style={{ fontSize: 12, opacity: 0.6, marginTop: 8 }}>{news.date}</p>
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* ── Settings ──────────────────────────── */}
          {activeTab === "settings" && (
            <div className="settings-panel">
              <h2 style={{ marginBottom: 20, color: 'var(--accent-red)' }}>Game Option</h2>
              <div className="arcade-box settings-list" style={{ padding: 24 }}>
                {configs.length === 0 ? (
                  <p style={{ opacity: 0.6 }}>No config found. Run the game once to generate defaults, or settings will be created as you change them.</p>
                ) : null}
                
                {SETTING_CATEGORIES.map((cat) => (
                  <div key={cat.name} style={{ marginBottom: 20 }}>
                    <h3 style={{ 
                      fontSize: 14, 
                      color: 'var(--accent-cyan)', 
                      borderBottom: '1px solid rgba(255,255,255,0.1)', 
                      paddingBottom: 4, 
                      marginBottom: 8 
                    }}>
                      {cat.icon} {cat.name}
                    </h3>
                    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))', gap: '8px' }}>
                      {cat.settings.map(renderSettingRow)}
                    </div>
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* ── Controls ──────────────────────────── */}
          {activeTab === "controls" && (
            <div className="controls-panel">
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 20 }}>
                <h2>Input Mapping</h2>
                <div style={{ display: 'flex', gap: 10 }}>
                  {(["XBOX", "PS5", "STICKS"] as const).map(preset => (
                    <button key={preset} className="arcade-card" onClick={() => applyPreset(preset)}
                      style={{ fontSize: 16, padding: '6px 16px', cursor: 'pointer', color: 'var(--text-primary)', fontWeight: 700, fontFamily: 'var(--font-main)' }}>
                      {preset}
                    </button>
                  ))}
                </div>
              </div>
              <Remapper onMappingSaved={updateMapping} currentMappings={mappings} />
            </div>
          )}
        </section>

        {/* 🕹️ Bottom Bar */}
        <footer className="bottom-bar">
          <div className="status-container">
            <div className="status-text">{status}</div>
            {isUpdating && (
              <div className="update-progress">
                <div className="progress-bar" style={{ width: `${progress}%` }} />
              </div>
            )}
          </div>
          <div className="play-button-container">
            <button className="btn-primary" onClick={handlePlay} disabled={isUpdating}>
              {status === "FIRST RUN - DOWNLOAD REQUIRED" ? "INSTALL GAME" : 
               (status === "GAME UP TO DATE" || status === "SYSTEM READY" || status === "PRESET APPLIED" || status === "OFFLINE — PLAY AVAILABLE" ? "PLAY" : "UPDATE")}
            </button>
          </div>
        </footer>
      </main>
    </div>
  );
}

export default App;
