import { useState, useEffect } from "react";
import { invoke } from "@tauri-apps/api/core";
import { getCurrentWindow } from "@tauri-apps/api/window";
import { listen } from "@tauri-apps/api/event";
import { openUrl } from "@tauri-apps/plugin-opener";
import { Remapper } from "./components/Remapper";
import "./App.css";


type Tab = "news" | "settings" | "controls";

interface NewsItem {
  id: number;
  tag: string;
  title: string;
  summary?: string;
  url?: string;
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
        { label: "Nearest", value: "nearest" }, 
        { label: "Linear", value: "linear" },
        { label: "Soft Linear", value: "soft-linear" },
        { label: "Integer", value: "integer" },
        { label: "Square Pixels", value: "square-pixels" }
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

function App() {
  const [activeTab, setActiveTab] = useState<Tab>("news");
  const [openCategories, setOpenCategories] = useState<Record<string, boolean>>({
    Window: true, Rendering: false, Netplay: false, Training: false, Mods: false
  });
  const [newsFeed, setNewsFeed] = useState<NewsItem[]>([]);
  const [isUpdating, setIsUpdating] = useState(false);
  const [progress, setProgress] = useState(0);
  const [status, setStatus] = useState("CHECKING SYSTEM...");
  const [configs, setConfigs] = useState<GameConfig[]>([]);
  const [mappings, setMappings] = useState<GameConfig[]>([]);
  const [isGameInstalled, setIsGameInstalled] = useState(true);
  const [buildDate, setBuildDate] = useState("UNKNOWN");
  const [launcherDate, setLauncherDate] = useState("UNKNOWN");

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
      const installed = await invoke("is_game_installed").catch(() => false);
      setStatus(installed ? "OFFLINE — PLAY AVAILABLE" : "OFFLINE — INSTALL REQUIRED");
    }

    const nowInstalled = await invoke("is_game_installed").catch(() => false) as boolean;
    setIsGameInstalled(nowInstalled);

    // Refresh build date after download may have written launcher_version.txt
    try {
      const localVersion = await invoke("get_local_version") as string;
      if (localVersion && localVersion !== "UNKNOWN") {
        const date = new Date(localVersion);
        if (!isNaN(date.getTime())) {
          setBuildDate(date.toISOString().split('T')[0]);
        } else {
          setBuildDate(localVersion);
        }
      }
    } catch {}

    setIsUpdating(false);
    setProgress(100);
  };

  const toggleCategory = (name: string) => {
    setOpenCategories(prev => ({ ...prev, [name]: !prev[name] }));
  };

  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (document.activeElement?.tagName === 'INPUT' || document.activeElement?.tagName === 'SELECT') return;

      setActiveTab(prev => {
        const tabs: Tab[] = ["news", "settings", "controls"];
        const idx = tabs.indexOf(prev);
        if (e.key === 'q' || e.key === 'Q' || e.key === 'ArrowLeft') {
          return tabs[(idx - 1 + tabs.length) % tabs.length];
        }
        if (e.key === 'e' || e.key === 'E' || e.key === 'ArrowRight') {
          return tabs[(idx + 1) % tabs.length];
        }
        return prev;
      });
    };
    window.addEventListener('keydown', handleKeyDown);

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

      // Fetch live news feeds
      try {
        const fetchNews = async () => {
          const res = await fetch("https://api.github.com/repos/3sxtra/3sxtra/commits?per_page=3");
          if (res.ok) {
            const commits = await res.json();
            const bgImages = [
              "https://images.unsplash.com/photo-1550745165-9bc0b252726f?auto=format&fit=crop&q=80&w=600",
              "https://images.unsplash.com/photo-1534423861386-85a16f5d13fd?auto=format&fit=crop&q=80&w=600",
              "https://images.unsplash.com/photo-1511512578047-dfb367046420?auto=format&fit=crop&q=80&w=600"
            ];

            const liveNews = commits.map((commitData: any, index: number) => {
              const messageParts = commitData.commit.message.split('\n');
              const title = messageParts[0];
              const summaryRaw = messageParts.slice(1).join(' ').replace(/\*/g, '').replace(/(\r\n|\n|\r)/gm, ' ').replace(/\s+/g, ' ').trim();
              const summary = summaryRaw.length > 130 ? summaryRaw.substring(0, 127) + "..." : summaryRaw || "Routine patches and stability improvements pushed to the 3SX engine.";
              const dateRaw = new Date(commitData.commit.author.date);
              const dateFormatted = dateRaw.toLocaleDateString('en-US', { month: 'short', day: '2-digit', year: 'numeric' }).toUpperCase();
              
              return {
                id: index + 1,
                tag: "DEV UPDATE",
                title: title.length > 50 ? title.substring(0, 47) + "..." : title,
                summary: summary,
                url: commitData.html_url,
                date: dateFormatted,
                image: bgImages[index % bgImages.length]
              };
            });
            setNewsFeed(liveNews);
          }
        };
        await fetchNews();
      } catch (err) {
        console.error("Failed to fetch live news:", err);
      }

      // Fetch build date from the engine version manifest
      try {
        const localVersion = await invoke("get_local_version") as string;
        if (localVersion && localVersion !== "UNKNOWN") {
          const date = new Date(localVersion);
          if (!isNaN(date.getTime())) {
            setBuildDate(date.toISOString().split('T')[0]); // e.g. "2026-03-30"
          } else {
            setBuildDate(localVersion);
          }
        }
      } catch (err) {
        console.error("Failed to fetch local version", err);
      }

      // Fetch launcher build date (compile-time constant)
      try {
        const lbd = await invoke("get_launcher_build_date") as string;
        if (lbd) setLauncherDate(lbd);
      } catch (err) {
        console.error("Failed to fetch launcher build date", err);
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
      window.removeEventListener('keydown', handleKeyDown);
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

  /* 
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
  */

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
      setStatus("SYSTEM READY");
    } catch (err: any) {
      setStatus(`LAUNCH FAILED: ${err}`);
    }
  };


  // ── Render ─────────────────────────────────────────────────────
  const renderSettingRow = (s: SettingDef) => {
    const val = getConfigValue(s.key);
    return (
      <div key={s.key} className="arcade-row-selectable" style={{ 
        display: 'flex', 
        flexDirection: 'row', 
        justifyContent: 'space-between', 
        alignItems: 'center', 
        padding: '8px 12px', 
        marginBottom: '6px',
        background: 'rgba(0,0,0,0.85)',
        border: '2px solid var(--accent-red)',
        boxShadow: '2px 2px 0 rgba(0, 0, 0, 0.5)',
        transition: 'all 0.1s ease',
        cursor: 'pointer'
      }}>
        <span style={{ fontSize: 24, color: '#fff', fontFamily: 'var(--font-main)', textTransform: 'uppercase', letterSpacing: '1px' }}>{s.label}</span>
        {s.type === "bool" && (
          <button
            onClick={() => toggleBool(s.key)}
            style={{
              background: 'transparent',
              color: isBoolEnabled(s.key) ? 'var(--accent-yellow)' : 'var(--text-secondary)',
              border: 'none', padding: '0',
              fontWeight: 700, fontSize: 16, cursor: 'pointer', fontFamily: 'var(--font-main)',
              transition: 'all 0.1s ease', textTransform: 'uppercase'
            }}
          >
            {isBoolEnabled(s.key) ? "ON" : "OFF"}
          </button>
        )}
        {s.type === "int" && (
          <input
            type="number"
            value={val || ""}
            onChange={e => updateSetting(s.key, e.target.value)}
            style={{
              background: 'transparent', border: 'none',
              width: '80px', color: 'var(--accent-yellow)', textAlign: 'right',
              fontFamily: 'var(--font-main)', fontSize: 24, boxSizing: 'border-box'
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
              width: '160px', color: 'var(--accent-yellow)', textAlign: 'right',
              fontFamily: 'var(--font-main)', fontSize: 24, boxSizing: 'border-box'
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
              fontFamily: 'var(--font-main)', fontSize: 24, boxSizing: 'border-box',
              appearance: 'none', paddingRight: '10px', cursor: 'pointer'
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
        <div className="sidebar-logo">
          <h1 style={{ fontSize: 48, fontFamily: 'var(--font-header)', fontStyle: 'italic', textTransform: 'uppercase', color: 'var(--accent-yellow)', textShadow: '4px 4px 0 var(--accent-red), 0 0 4px #000', margin: '0 0 50px 0', lineHeight: 0.9 }}>3rd Strike<br/><span style={{ color: '#fff' }}>3SXtra</span></h1>
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: 16 }}>
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
        </div>

        <div className="persistent-play-btn" style={{ marginTop: 'auto', marginBottom: 24, display: 'flex', flexDirection: 'column', alignItems: 'flex-start', width: 320 }}>
          <div className="status-container" style={{ marginBottom: 16, width: '100%' }}>
            <div className={`status-text ${isUpdating ? 'blink' : ''}`} style={{ color: '#fff', fontSize: 16, marginBottom: 8, textTransform: 'uppercase', letterSpacing: '1px', textShadow: '1px 1px 0 #000' }}>{status}</div>
            {isUpdating && (
              <div style={{ width: '100%', height: 6, background: '#000', border: '1px solid var(--accent-red)' }}>
                 <div className="progress-bar" style={{ height: '100%', background: 'var(--accent-yellow)', width: `${progress}%` }} />
              </div>
            )}
          </div>
          <button 
            className="btn-primary" 
            style={{ 
              width: '100%', 
              justifyContent: 'center', 
              padding: '20px 0', 
              fontSize: 36, 
              whiteSpace: 'nowrap',
              boxShadow: '6px 6px 0 rgba(0,0,0,0.8)',
              cursor: isUpdating ? 'not-allowed' : 'pointer',
              border: '4px solid #fff'
            }}
            onClick={handlePlay} 
            disabled={isUpdating}
          >
            {!isGameInstalled ? "INSTALL GAME" : (isUpdating ? "UPDATING..." : "PLAY 3SX")}
          </button>
          <span style={{ fontSize: 12, fontFamily: 'monospace', color: '#fff', textShadow: '1px 1px 0 #000', letterSpacing: '1px', opacity: 0.5, marginTop: 10, alignSelf: 'center' }}>ENGINE {buildDate} · LAUNCHER {launcherDate}</span>
        </div>

        <div style={{ marginBottom: 40, opacity: 0.6, fontSize: 14, color: '#fff', fontFamily: 'var(--font-mono)' }}>
          [Q] / [E] PREV/NEXT TAB
        </div>
      </nav>

      {/* 📄 Main Content */}
      <main className="main-content">
        <header className="app-header" data-tauri-drag-region>
        </header>

        <h2 className="page-title">{activeTab.replace('_', ' ')}</h2>

        <section className="content-panel">
          {/* ── News ───────────────────────────────── */}
          {activeTab === "news" && (
            <div className="news-feed">
              <div className="news-grid">
              {newsFeed.map((news) => (
                <div key={news.id} className="news-card" style={{ border: '1px solid rgba(255,255,255,0.1)', boxShadow: '0 8px 32px rgba(0,0,0,0.5)', background: 'transparent' }}>
                  <img src={news.image} alt={news.title} className="news-card-img" />
                  <div style={{ position: 'absolute', top: 0, left: 0, right: 0, bottom: 0, zIndex: -1, background: 'linear-gradient(to top, rgba(0,0,0,1) 0%, rgba(0,0,0,0.8) 50%, rgba(0,0,0,0.1) 100%)' }} />
                  
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
                    <span style={{ fontSize: 13, color: 'var(--accent-yellow)', textTransform: 'uppercase', letterSpacing: '2px', fontWeight: 800 }}>{news.tag}</span>
                    <span style={{ fontSize: 12, color: 'rgba(255,255,255,0.4)', fontFamily: 'var(--font-mono)', fontWeight: 700, letterSpacing: '1px' }}>{news.date}</span>
                  </div>
                  
                  <h3 style={{ margin: '0 0 12px 0', fontSize: 26, color: '#fff', fontFamily: 'var(--font-header)', fontStyle: 'italic', textTransform: 'uppercase', letterSpacing: '0.5px', textShadow: '2px 2px 0 rgba(0,0,0,0.8)', lineHeight: 1 }}>{news.title}</h3>
                  <p className="news-summary" style={{ color: 'rgba(255,255,255,0.7)', fontSize: 14, fontFamily: 'var(--font-main)', margin: '0 0 20px 0', lineHeight: 1.4, textShadow: '1px 1px 0 #000' }}>{news.summary}</p>
                  
                  <div style={{ display: 'flex', alignItems: 'center' }}>
                    <button onClick={() => news.url && openUrl(news.url).catch(console.error)} className="btn-news" style={{
                      padding: '8px 24px', fontSize: 14, fontFamily: 'var(--font-header)', fontStyle: 'italic', fontWeight: 800,
                      backgroundColor: 'transparent', color: '#fff', border: '2px solid var(--accent-red)', cursor: 'pointer', textTransform: 'uppercase', boxShadow: '0 0 0 transparent', transition: 'all 0.15s'
                    }}>
                      {news.tag.includes("PATCH") ? "PATCH NOTES" : "READ FULL"}
                    </button>
                  </div>
                </div>
                ))}
              </div>
            </div>
          )}

          {/* ── Settings ──────────────────────────── */}
          {activeTab === "settings" && (
            <div className="settings-panel">
              <div className="settings-list" style={{ paddingBottom: 40 }}>
                {configs.length === 0 ? (
                  <p style={{ opacity: 0.6 }}>No config found. Run the game once to generate defaults, or settings will be created as you change them.</p>
                ) : null}
                
                {SETTING_CATEGORIES.map((cat) => (
                  <div key={cat.name} style={{ marginBottom: openCategories[cat.name] ? 24 : 12, background: 'rgba(0,0,0,0.5)', border: `2px solid ${openCategories[cat.name] ? 'var(--accent-yellow)' : 'var(--accent-red)'}`, padding: '16px', transition: 'all 0.2s ease' }}>
                    <h3 
                      onClick={() => toggleCategory(cat.name)}
                      style={{ 
                      fontSize: 28, 
                      color: openCategories[cat.name] ? 'var(--accent-yellow)' : 'var(--text-primary)', 
                      paddingBottom: openCategories[cat.name] ? 16 : 0, 
                      marginBottom: openCategories[cat.name] ? 16 : 0, 
                      cursor: 'pointer',
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center',
                      userSelect: 'none',
                      borderBottom: openCategories[cat.name] ? '2px solid rgba(255, 255, 255, 0.1)' : 'none',
                      transition: 'all 0.2s ease'
                    }}>
                      <span>{cat.icon} {cat.name}</span>
                      <span style={{ fontSize: 18, transform: openCategories[cat.name] ? 'rotate(180deg)' : 'rotate(0deg)', transition: 'transform 0.2s', opacity: 0.8 }}>▼</span>
                    </h3>
                    {openCategories[cat.name] && (
                      <div style={{ display: 'grid', gridTemplateColumns: 'minmax(300px, 1fr) minmax(300px, 1fr)', columnGap: '40px', rowGap: '8px' }}>
                        {cat.settings.map(renderSettingRow)}
                      </div>
                    )}
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* ── Controls ──────────────────────────── */}
          {activeTab === "controls" && (
            <div className="controls-panel">

              <Remapper onMappingSaved={updateMapping} currentMappings={mappings} />
            </div>
          )}
        </section>
      </main>
    </div>
  );
}

export default App;
