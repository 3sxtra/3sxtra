use std::process::Command;
use std::path::PathBuf;
use tauri::Emitter;
use directories::UserDirs;
use ini::Ini;
use serde::{Serialize, Deserialize};

#[derive(Serialize, Deserialize, Clone)]
pub struct GameConfig {
    pub key: String,
    pub value: String,
}


#[derive(Deserialize, Debug)]
struct GitHubAsset {
    name: String,
    browser_download_url: String,
}

#[derive(Deserialize, Debug)]
#[allow(dead_code)]
struct GitHubRelease {
    name: String,
    published_at: String,
    assets: Vec<GitHubAsset>,
}

#[derive(Deserialize, Serialize, Clone)]
#[serde(rename_all = "camelCase")]
struct ArchiveTask {
    name: String,
    url: String,
    extract_path: String,
    marker_file: String,
    strip_root: bool,
    force_update: bool,
    version_id: Option<String>,
}

#[derive(Deserialize, Serialize, Clone)]
struct UpdateManifest {
    version: String,
    archives: Option<Vec<ArchiveTask>>,
}

// ────────────────────────────────────────────────────────────
// Path Resolution — mirrors the engine's paths.c logic exactly
// ────────────────────────────────────────────────────────────

/// Returns the game's install directory (parent of the launcher exe).
/// The expected layout is:
///   <game_root>/tools/launcher/3sx-launcher.exe
///   <game_root>/3sx.exe
fn get_game_root() -> PathBuf {
    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(exe_dir) = exe_path.parent() {
            // Release layout: launcher is in game root directly
            let release_marker = exe_dir.join("assets").join("ASSET_VERSION");
            if release_marker.exists() {
                return exe_dir.to_path_buf();
            }

            // Dev layout: Tauri 2 places exes deep in target/debug/.
            // Walk up directories (up to 6 levels mapping back to repo root)
            let mut current = exe_dir.to_path_buf();
            for _ in 0..6 {
                if current.join("assets").join("ASSET_VERSION").exists() {
                    return std::fs::canonicalize(&current).unwrap_or(current.clone());
                }
                if let Some(parent) = current.parent() {
                    current = parent.to_path_buf();
                } else {
                    break;
                }
            }
            
            // Fallback
            return exe_dir.to_path_buf();
        }
    }
    PathBuf::from(".")
}

/// Preference path — checks for Portable Mode first (sibling "config/" folder
/// next to game root), then falls back to the standard AppData path that the
/// engine uses: %APPDATA%/CrowdedStreet/3SX/
fn get_pref_path() -> PathBuf {
    let game_root = get_game_root();

    // 1. Portable Mode
    let portable_path = game_root.join("config");
    if portable_path.is_dir() {
        return portable_path;
    }

    // 2. Standard Mode — matches SDL_GetPrefPath("CrowdedStreet", "3SX")
    if let Some(user_dirs) = UserDirs::new() {
        #[cfg(target_os = "windows")]
        let base_path = user_dirs.home_dir().join("AppData").join("Roaming");

        #[cfg(target_os = "macos")]
        let base_path = user_dirs.home_dir().join("Library").join("Application Support");

        #[cfg(target_os = "linux")]
        let base_path = user_dirs.home_dir().join(".local").join("share");

        // Fallback for any other obscure OS
        #[cfg(not(any(target_os = "windows", target_os = "macos", target_os = "linux")))]
        let base_path = user_dirs.home_dir().join(".config");

        let path = base_path.join("CrowdedStreet").join("3SX");

        if !path.exists() {
            let _ = std::fs::create_dir_all(&path);
        }
        return path;
    }

    PathBuf::from(".")
}

fn get_config_file_path() -> PathBuf {
    get_pref_path().join("config")
}

fn get_mappings_file_path() -> PathBuf {
    get_pref_path().join("mappings.ini")
}

// ────────────────────────────────────────────────────────────
// Game Launch
// ────────────────────────────────────────────────────────────

#[tauri::command]
fn is_game_installed() -> Result<bool, String> {
    let game_root = get_game_root();
    let exe_name = format!("3sx{}", std::env::consts::EXE_SUFFIX);
    let exe_path = game_root.join(exe_name);
    Ok(exe_path.exists())
}

#[tauri::command]
fn launch_game() -> Result<String, String> {
    let game_root = get_game_root();
    let exe_name = format!("3sx{}", std::env::consts::EXE_SUFFIX);
    let exe_path = game_root.join(exe_name);

    if !exe_path.exists() {
        return Err(format!("Game executable not found at: {}", exe_path.display()));
    }

    Command::new(&exe_path)
        .current_dir(&game_root)
        .spawn()
        .map(|_| "Game launched successfully".to_string())
        .map_err(|e| format!("Failed to launch game: {}", e))
}

// ────────────────────────────────────────────────────────────
// Config Management
// The game's config file is a FLAT key=value format (no sections).
// We must use the "General" / None section in rust-ini to match this.
// ────────────────────────────────────────────────────────────

#[tauri::command]
fn get_config() -> Result<Vec<GameConfig>, String> {
    let path = get_config_file_path();
    if !path.exists() { return Ok(vec![]); }

    let conf = Ini::load_from_file(&path).map_err(|e| e.to_string())?;
    let mut configs = Vec::new();
    for (section, prop) in conf.iter() {
        // Only read the global (sectionless) entries — the game doesn't use sections
        if section.is_some() { continue; }
        for (key, value) in prop.iter() {
            configs.push(GameConfig { key: key.to_string(), value: value.to_string() });
        }
    }
    Ok(configs)
}

#[tauri::command]
fn save_config(key: String, value: String) -> Result<(), String> {
    let path = get_config_file_path();

    // Ensure parent directory exists
    if let Some(parent) = path.parent() {
        let _ = std::fs::create_dir_all(parent);
    }

    let mut conf = if path.exists() {
        Ini::load_from_file(&path).unwrap_or_default()
    } else {
        Ini::new()
    };

    // Write to the global (sectionless) area — matches the game's flat format
    conf.with_section(None::<String>).set(&key, &value);
    conf.write_to_file(&path).map_err(|e| e.to_string())
}

// ────────────────────────────────────────────────────────────
// Mappings (mappings.ini) — also flat key=value
// ────────────────────────────────────────────────────────────

#[tauri::command]
fn get_mappings() -> Result<Vec<GameConfig>, String> {
    let path = get_mappings_file_path();
    if !path.exists() { return Ok(vec![]); }

    let conf = Ini::load_from_file(&path).map_err(|e| e.to_string())?;
    let mut mappings = Vec::new();
    for (section, prop) in conf.iter() {
        if section.is_some() { continue; }
        for (key, value) in prop.iter() {
            mappings.push(GameConfig { key: key.to_string(), value: value.to_string() });
        }
    }
    Ok(mappings)
}

#[tauri::command]
fn save_mapping(key: String, value: String) -> Result<(), String> {
    let path = get_mappings_file_path();

    if let Some(parent) = path.parent() {
        let _ = std::fs::create_dir_all(parent);
    }

    let mut conf = if path.exists() {
        Ini::load_from_file(&path).unwrap_or_default()
    } else {
        Ini::new()
    };

    conf.with_section(None::<String>).set(&key, &value);
    conf.write_to_file(&path).map_err(|e| e.to_string())
}

// ────────────────────────────────────────────────────────────
// Utilities
// ────────────────────────────────────────────────────────────

#[tauri::command]
async fn check_updates() -> Result<Option<UpdateManifest>, String> {
    let client = reqwest::Client::builder()
        .user_agent("3SX-Launcher")
        .timeout(std::time::Duration::from_secs(10))
        .build()
        .map_err(|e| e.to_string())?;

    let url = "https://api.github.com/repos/3sxtra/3sxtra/releases/tags/rolling-pre-release";
    let resp = client.get(url).send().await.map_err(|e| format!("Failed to fetch release: {}", e))?;

    if !resp.status().is_success() {
        return Ok(None);
    }

    let release: GitHubRelease = resp.json().await.map_err(|e| format!("Invalid release JSON: {}", e))?;
    
    let root = get_game_root();
    let version_file = root.join("launcher_version.txt");
    let local_version = std::fs::read_to_string(&version_file).unwrap_or_default();
    
    let mut archives = vec![];
    
    // ── Asset Pack ─────────────────────────────────────────────
    // The asset zip is named like "3SX-assets-v1-3a760a0.zip".
    // Compare the "vN" in the filename against local assets/ASSET_VERSION.
    let local_asset_version = std::fs::read_to_string(root.join("assets").join("ASSET_VERSION"))
        .unwrap_or_default()
        .trim()
        .to_string();
    
    if let Some(asset_zip) = release.assets.iter().find(|a| a.name.contains("assets") && a.name.ends_with(".zip")) {
        // Extract version from filename: "3SX-assets-v1-abc1234.zip" -> "1"
        let remote_asset_version = asset_zip.name
            .split("-v")
            .nth(1)
            .and_then(|s| s.split('-').next())
            .unwrap_or("0")
            .to_string();
        
        let needs_assets = local_asset_version.is_empty() || local_asset_version != remote_asset_version;
        
        if needs_assets {
            archives.push(ArchiveTask {
                name: "3SX Asset Pack".to_string(),
                url: asset_zip.browser_download_url.clone(),
                extract_path: ".".to_string(), // zip already contains assets/ folder
                marker_file: "assets/ASSET_VERSION".to_string(),
                strip_root: false, // zip is structured as assets/... already
                force_update: true,
                version_id: None, // version tracked by ASSET_VERSION file inside the zip
            });
        }
    }
    
    // ── Engine Binary ──────────────────────────────────────────
    if local_version.trim() != release.published_at.trim() {
        let os_str = if cfg!(target_os = "windows") {
            "windows"
        } else if cfg!(target_os = "macos") {
            "macos"
        } else {
            "linux" 
        };
        
        if let Some(asset) = release.assets.iter().find(|a| a.name.contains(os_str) && a.name.ends_with(".zip")) {
            archives.push(ArchiveTask {
                name: "3SX Core Engine".to_string(),
                url: asset.browser_download_url.clone(),
                extract_path: ".".to_string(),
                marker_file: if cfg!(target_os = "windows") { "3sx.exe".to_string() } else { "3sx".to_string() },
                strip_root: true, // The release zip has a top-level bin/ directory
                force_update: true,
                version_id: Some(release.published_at.clone()),
            });
        }
    }
    
    Ok(Some(UpdateManifest {
        version: release.published_at,
        archives: Some(archives),
    }))
}

#[tauri::command]
fn check_file_exists(path: String) -> Result<bool, String> {
    Ok(get_game_root().join(&path).exists())
}

#[tauri::command]
fn get_local_version() -> Result<String, String> {
    let version_file = get_game_root().join("launcher_version.txt");
    if version_file.exists() {
        if let Ok(content) = std::fs::read_to_string(&version_file) {
            return Ok(content.trim().to_string());
        }
    }
    Ok("UNKNOWN".to_string())
}

#[tauri::command]
async fn download_and_extract_archive(
    window: tauri::Window,
    url: String, 
    extract_path: String, 
    marker_file: String, 
    strip_root: bool, 
    version_id: Option<String>
) -> Result<(), String> {
    let game_root = get_game_root();
    
    // Unbound the download time limitation for poor networks
    let client = reqwest::Client::builder().build().map_err(|e| e.to_string())?;
    let resp = client.get(&url).send().await.map_err(|e| e.to_string())?;
    if !resp.status().is_success() {
        return Err(format!("Download failed with status: {}", resp.status()));
    }
    
    let total_size = resp.content_length().unwrap_or(0);
    
    use futures_util::StreamExt;
    use std::io::Write;
    
    // Use a robust temporary stream file on disk instead of blasting system RAM!
    let temp_file_name = format!("{}.zip.part", marker_file.replace("/", "_").replace("\\", "_"));
    let temp_zip_path = game_root.join(&temp_file_name);
    let mut file = std::fs::File::create(&temp_zip_path).map_err(|e| format!("Failed to create temp zip: {}", e))?;
    
    let mut downloaded: u64 = 0;
    let mut stream = resp.bytes_stream();
    
    while let Some(chunk) = stream.next().await {
        let chunk = chunk.map_err(|e| format!("Stream error: {}", e))?;
        file.write_all(&chunk).map_err(|e| format!("Write error: {}", e))?;
        downloaded += chunk.len() as u64;
        
        if total_size > 0 {
            let progress = (downloaded as f64 / total_size as f64) * 100.0;
            let _ = window.emit("download-progress", progress);
        }
    }
    
    // Explicitly flush and drop the write handle before unzipping
    file.flush().unwrap_or_default();
    drop(file);
    
    let file_reader = std::fs::File::open(&temp_zip_path).map_err(|e| format!("Failed to read temp zip: {}", e))?;
    
    let mut archive = match zip::ZipArchive::new(file_reader) {
        Ok(arc) => arc,
        Err(e) => {
            let _ = std::fs::remove_file(&temp_zip_path);
            return Err(format!("Invalid ZIP: {}", e));
        }
    };
    
    let extract_dir = game_root.join(&extract_path);
    
    for i in 0..archive.len() {
        let mut file = match archive.by_index(i) {
            Ok(f) => f,
            Err(e) => {
                let _ = std::fs::remove_file(&temp_zip_path);
                return Err(format!("Error reading ZIP file {}: {}", i, e));
            }
        };
        let out_path = match file.enclosed_name() {
            Some(path) => path.to_owned(),
            None => continue,
        };
        
        let stripped_path = if strip_root {
            let mut components = out_path.components();
            let _ = components.next(); // Skip the root dir
            components.collect::<std::path::PathBuf>()
        } else {
            out_path
        };
        
        if stripped_path.as_os_str().is_empty() { 
            continue; 
        }
        
        let target_path = extract_dir.join(stripped_path);
        
        if file.name().ends_with('/') || file.is_dir() {
            std::fs::create_dir_all(&target_path).unwrap_or_default();
        } else {
            if let Some(p) = target_path.parent() {
                std::fs::create_dir_all(p).unwrap_or_default();
            }
            if let Ok(mut out_file) = std::fs::File::create(&target_path) {
                let _ = std::io::copy(&mut file, &mut out_file);
            }
        }
    }
    
    // Perform cleanup of the massive temp partial zip!
    let _ = std::fs::remove_file(&temp_zip_path);
    
    if !game_root.join(&marker_file).exists() {
        return Err(format!("Archive extracted but marker file {} was not found", marker_file));
    }
    
    if let Some(vid) = version_id {
        let version_file = game_root.join("launcher_version.txt");
        let _ = std::fs::write(version_file, vid);
    }
    
    Ok(())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_shell::init())
        .invoke_handler(tauri::generate_handler![
            is_game_installed,
            launch_game,
            get_config,
            save_config,
            get_mappings,
            save_mapping,
            check_updates,
            download_and_extract_archive,
            check_file_exists,
            get_local_version
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
