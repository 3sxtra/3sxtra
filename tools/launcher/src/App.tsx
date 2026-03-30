import { useState, useEffect, useCallback } from "react";
import { invoke } from "@tauri-apps/api/core";
import { getCurrentWindow } from "@tauri-apps/api/window";
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
  const [activeSettingsCat, setActiveSettingsCat] = useState(0);
  const [isUpdating, setIsUpdating] = useState(false);
  const [progress, setProgress] = useState(0);
  const [status, setStatus] = useState("CHECKING SYSTEM...");
  const [configs, setConfigs] = useState<GameConfig[]>([]);
  const [mappings, setMappings] = useState<GameConfig[]>([]);

  const performUpdate = async () => {
    setStatus("CHECKING FOR UPDATES...");
    setIsUpdating(true);
    try {
      const manifest = await invoke("check_updates") as any;
      if (manifest && manifest.files) {
        let needsUpdate = false;
        for (const file of manifest.files) {
          const valid = await invoke("verify_file_hash", { path: file.path, expectedHash: file.hash });
          if (!valid) { needsUpdate = true; break; }
        }

        if (needsUpdate) {
          setStatus("DOWNLOADING UPDATES...");
          const total = manifest.files.length;
          let done = 0;
          for (const file of manifest.files) {
            const valid = await invoke("verify_file_hash", { path: file.path, expectedHash: file.hash });
            if (!valid) {
              await invoke("download_file", { url: file.url, path: file.path });
            }
            done++;
            setProgress(Math.round((done / total) * 100));
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

      if (!installed) {
        setStatus("FIRST RUN - DOWNLOAD REQUIRED");
        return; // Wait for user to click button
      }

      // 3. Auto-patch if installed
      await performUpdate();
    }
    init();
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

    if (status === "FIRST RUN - DOWNLOAD REQUIRED") {
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
      <div key={s.key} style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '14px 0', borderBottom: '1px solid rgba(255,255,255,0.04)' }}>
        <span style={{ fontSize: 14 }}>{s.label}</span>
        {s.type === "bool" && (
          <button
            onClick={() => toggleBool(s.key)}
            style={{
              background: isBoolEnabled(s.key) ? 'var(--accent-cyan)' : 'rgba(255,255,255,0.08)',
              color: isBoolEnabled(s.key) ? '#000' : 'var(--text-secondary)',
              border: 'none', borderRadius: 4, padding: '4px 16px',
              fontWeight: 700, fontSize: 11, cursor: 'pointer', fontFamily: 'var(--font-mono)',
              transition: 'all 0.2s ease',
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
              background: 'rgba(255,255,255,0.05)', border: '1px solid rgba(255,255,255,0.1)',
              borderRadius: 4, padding: '4px 10px', width: 80, color: 'var(--accent-cyan)',
              fontFamily: 'var(--font-mono)', fontSize: 12, textAlign: 'right',
            }}
          />
        )}
        {s.type === "string" && (
          <input
            type="text"
            value={val || ""}
            onChange={e => updateSetting(s.key, e.target.value)}
            placeholder="not set"
            style={{
              background: 'rgba(255,255,255,0.05)', border: '1px solid rgba(255,255,255,0.1)',
              borderRadius: 4, padding: '4px 10px', width: 200, color: 'var(--accent-cyan)',
              fontFamily: 'var(--font-mono)', fontSize: 12,
            }}
          />
        )}
        {s.type === "select" && s.options && (
          <select
            value={val || s.options[0].value}
            onChange={e => updateSetting(s.key, e.target.value)}
            style={{
              background: 'rgba(255,255,255,0.05)', border: '1px solid rgba(255,255,255,0.1)',
              borderRadius: 4, padding: '4px 10px', color: 'var(--accent-cyan)',
              fontFamily: 'var(--font-mono)', fontSize: 12,
            }}
          >
            {s.options.map(o => <option key={o.value} value={o.value}>{o.label}</option>)}
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


      {/* ⬅️ Sidebar */}
      <nav className="sidebar">
        <div className="nav-logo" onClick={() => setActiveTab("news")}>
          <div style={{ width: 40, height: 40, background: 'var(--accent-cyan)', borderRadius: 8, filter: 'drop-shadow(0 0 10px var(--accent-cyan))' }} />
        </div>
        <button className={`nav-item ${activeTab === 'news' ? 'active' : ''}`} onClick={() => setActiveTab("news")} title="News">
          <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><path d="M19 20H5a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h10l4 4v10a2 2 0 0 1-2 2z"></path><polyline points="14 4 14 8 18 8"></polyline><line x1="16" y1="13" x2="8" y2="13"></line><line x1="16" y1="17" x2="8" y2="17"></line></svg>
        </button>
        <button className={`nav-item ${activeTab === 'settings' ? 'active' : ''}`} onClick={() => setActiveTab("settings")} title="Settings">
          <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><circle cx="12" cy="12" r="3"></circle><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"></path></svg>
        </button>
        <button className={`nav-item ${activeTab === 'controls' ? 'active' : ''}`} onClick={() => setActiveTab("controls")} title="Controls">
          <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><rect x="2" y="6" width="20" height="12" rx="2"></rect><circle cx="12" cy="12" r="2"></circle><path d="M6 12h.01M18 12h.01"></path></svg>
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
                  <div key={news.id} className="news-card glass">
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
              <h2 style={{ marginBottom: 20 }}>System Settings</h2>
              {/* Category Tabs */}
              <div style={{ display: 'flex', gap: 8, marginBottom: 20 }}>
                {SETTING_CATEGORIES.map((cat, i) => (
                  <button
                    key={cat.name}
                    className={`glass-card ${activeSettingsCat === i ? 'active' : ''}`}
                    onClick={() => setActiveSettingsCat(i)}
                    style={{
                      padding: '8px 18px', cursor: 'pointer', border: 'none',
                      background: activeSettingsCat === i ? 'rgba(0,242,255,0.15)' : 'rgba(255,255,255,0.03)',
                      color: activeSettingsCat === i ? 'var(--accent-cyan)' : 'var(--text-secondary)',
                      borderRadius: 8, fontSize: 12, fontWeight: 700, fontFamily: 'var(--font-main)',
                      borderBottom: activeSettingsCat === i ? '2px solid var(--accent-cyan)' : '2px solid transparent',
                      transition: 'all 0.2s ease',
                    }}
                  >
                    {cat.icon} {cat.name}
                  </button>
                ))}
              </div>
              {/* Settings List */}
              <div className="glass" style={{ padding: 24 }}>
                {configs.length === 0 ? (
                  <p style={{ opacity: 0.6 }}>No config found. Run the game once to generate defaults, or settings will be created as you change them.</p>
                ) : null}
                {SETTING_CATEGORIES[activeSettingsCat].settings.map(renderSettingRow)}
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
                    <button key={preset} className="glass-card" onClick={() => applyPreset(preset)}
                      style={{ fontSize: 11, padding: '6px 16px', cursor: 'pointer', border: 'none', color: 'var(--text-secondary)', fontWeight: 700, fontFamily: 'var(--font-mono)', borderRadius: 6 }}>
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
