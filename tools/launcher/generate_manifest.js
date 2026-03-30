import fs from 'fs';
import path from 'path';
import crypto from 'crypto';

// The root of the game relative to this script
const GAME_ROOT = path.resolve(__dirname, '../../');
const MANIFEST_PATH = path.join(__dirname, 'remote_manifest.json');
const BASE_URL = 'https://github.com/3sxtra/3sxtra/releases/latest/download/';

// OS-agnostic executable detection
const EXE_NAME = process.platform === 'win32' ? '3sx.exe' : '3sx';

// Folders inside the game root to recursively include
const TARGET_FOLDERS = ['assets'];

// Exact files to always include at the root
const BASE_FILES = [EXE_NAME];

// Specific paths or folder names to IGNORE to prevent local mods/wip from shipping.
// Synced with the root .gitignore (excluding built assets required for distribution)
const IGNORE_LIST = [
  'assets/bgm_mod',
  'assets/voice_mod',
  'assets/sprites',
  'assets/stages',
  'assets/bezels',
  'assets/ui/assets',
  'assets/lua/sf3_3rd_trial_lua',
  'assets/lua/3rd_training_lua-main',
  '.DS_Store',
  'desktop.ini'
];

function calculateHash(filePath) {
  const fileBuffer = fs.readFileSync(filePath);
  const hashSum = crypto.createHash('sha256');
  hashSum.update(fileBuffer);
  return hashSum.digest('hex');
}

function shouldIgnore(relativePath) {
  const normalized = relativePath.replace(/\\/g, '/');
  return IGNORE_LIST.some(ignore => normalized.includes(ignore));
}

function walkDir(dir, fileList = []) {
  if (!fs.existsSync(dir)) return fileList;
  
  const files = fs.readdirSync(dir);
  for (const file of files) {
    const fullPath = path.join(dir, file);
    const relPath = path.relative(GAME_ROOT, fullPath).replace(/\\/g, '/');
    
    if (shouldIgnore(relPath)) continue;

    if (fs.statSync(fullPath).isDirectory()) {
      walkDir(fullPath, fileList);
    } else {
      fileList.push(relPath);
    }
  }
  return fileList;
}

function generateManifest() {
  const version = new Date().toISOString().split('T')[0].replace(/-/g, '.'); // E.g., 2026.03.30
  const files = [];
  const scannedFiles = [...BASE_FILES];

  // Recursively scan Target Folders
  for (const folder of TARGET_FOLDERS) {
    const fullFolderPath = path.join(GAME_ROOT, folder);
    walkDir(fullFolderPath, scannedFiles);
  }

  // Hash everything
  let fileCount = 0;
  for (const relativePath of scannedFiles) {
    const fullPath = path.join(GAME_ROOT, relativePath);
    if (fs.existsSync(fullPath)) {
      files.push({
        path: relativePath,
        hash: calculateHash(fullPath),
        url: `${BASE_URL}${path.basename(relativePath)}`
      });
      fileCount++;
    } else {
      console.warn(`⚠️ Warning: ${relativePath} not found during final hashing!`);
    }
  }

  const archives = [
    {
      name: "slang-shaders",
      url: "https://github.com/libretro/slang-shaders/archive/refs/heads/master.zip",
      extractPath: "assets/shaders/libretro",
      markerFile: "assets/shaders/libretro/COPYING"
    }
  ];

  const manifest = {
    version,
    files,
    archives
  };

  fs.writeFileSync(MANIFEST_PATH, JSON.stringify(manifest, null, 2));
  console.log(`\n🎉 Script Complete: Hashed ${fileCount} clean files!`);
  console.log(`Manifest generated at ${MANIFEST_PATH}`);
  console.log(`Ignored mods/system files based on IGNORE_LIST.`);
}

generateManifest();
