fn main() {
    // Embed build date as compile-time env var for the launcher header
    let now = chrono::Utc::now().format("%Y-%m-%d").to_string();
    println!("cargo:rustc-env=LAUNCHER_BUILD_DATE={}", now);
    tauri_build::build()
}
