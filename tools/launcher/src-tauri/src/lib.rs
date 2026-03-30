use std::process::Command;
use std::path::{Path, PathBuf};
use directories::UserDirs;
use ini::Ini;
use serde::{Serialize, Deserialize};
use sha2::{Sha256, Digest};
use std::io::{Read, BufReader};
use std::fs::File;

#[derive(Serialize, Deserialize, Clone)]
pub struct GameConfig {
    pub key: String,
    pub value: String,
}

#[derive(Serialize, Deserialize)]
pub struct ManifestItem {
    pub path: String,
    pub hash: String,
    pub url: String,
}

#[derive(Serialize, Deserialize)]
pub struct UpdateManifest {
    pub version: String,
    pub files: Vec<ManifestItem>,
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
            // During development the CWD is tools/launcher, game root is ../../
            // During release the launcher sits next to the game exe
            let dev_root = exe_dir.join("..").join("..");
            let marker = dev_root.join("assets").join("ASSET_VERSION");
            if marker.exists() {
                return std::fs::canonicalize(dev_root).unwrap_or_else(|_| exe_dir.to_path_buf());
            }
            // Release layout: launcher is in game root
            let release_marker = exe_dir.join("assets").join("ASSET_VERSION");
            if release_marker.exists() {
                return exe_dir.to_path_buf();
            }
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
// Update System
// ────────────────────────────────────────────────────────────

#[tauri::command]
async fn check_updates() -> Result<Option<UpdateManifest>, String> {
    let manifest_url = "https://raw.githubusercontent.com/3sxtra/3sxtra/main/tools/launcher/remote_manifest.json";
    let client = reqwest::Client::builder()
        .timeout(std::time::Duration::from_secs(10))
        .build()
        .map_err(|e| e.to_string())?;

    match client.get(manifest_url).send().await {
        Ok(resp) => {
            let manifest: UpdateManifest = resp.json().await.map_err(|e| e.to_string())?;
            Ok(Some(manifest))
        }
        Err(_) => Ok(None), // Offline — skip update check gracefully
    }
}

#[tauri::command]
fn verify_file_hash(path: String, expected_hash: String) -> Result<bool, String> {
    let game_root = get_game_root();
    let file_path = game_root.join(&path);
    if !file_path.exists() { return Ok(false); }

    let file = File::open(&file_path).map_err(|e| e.to_string())?;
    let mut reader = BufReader::new(file);
    let mut hasher = Sha256::new();
    let mut buffer = [0u8; 8192];

    loop {
        let count = reader.read(&mut buffer).map_err(|e| e.to_string())?;
        if count == 0 { break; }
        hasher.update(&buffer[..count]);
    }

    let hash_result = format!("{:x}", hasher.finalize());
    Ok(hash_result == expected_hash)
}

#[tauri::command]
async fn download_file(url: String, path: String) -> Result<(), String> {
    let game_root = get_game_root();
    let dest_path = game_root.join(&path);

    if let Some(parent) = dest_path.parent() {
        let _ = std::fs::create_dir_all(parent);
    }

    let client = reqwest::Client::new();
    let response = client.get(&url).send().await.map_err(|e| e.to_string())?;

    if !response.status().is_success() {
        return Err(format!("Download failed with status: {}", response.status()));
    }

    let bytes = response.bytes().await.map_err(|e| e.to_string())?;
    std::fs::write(&dest_path, &bytes).map_err(|e| e.to_string())?;
    Ok(())
}

// ────────────────────────────────────────────────────────────
// App Entry
// ────────────────────────────────────────────────────────────

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
            verify_file_hash,
            download_file
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
