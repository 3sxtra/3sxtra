#!/usr/bin/env node
/**
 * seed_leaderboard.js — Populate the lobby-server SQLite DB with synthetic
 * leaderboard data based on legendary SF3: Third Strike players.
 *
 * Usage:
 *   node seed_leaderboard.js [path/to/lobby.db]
 *
 * If no path given, defaults to ./lobby.db (same dir as lobby-server.js).
 *
 * Character index mapping (from rmlui_leaderboard.cpp):
 *   0=Alex  1=Ryu  2=Yun  3=Dudley  4=Necro  5=Hugo  6=Ibuki  7=Elena
 *   8=Oro  9=Yang  10=Ken  11=Sean  12=Urien  13=Gill  14=Chun-Li
 *   15=Makoto  16=Q  17=Twelve  18=Treize  19=Remy
 */

const path = require('path');
const Database = require('better-sqlite3');

const dbPath = process.argv[2] || path.join(__dirname, 'lobby.db');
console.log(`Seeding database: ${dbPath}`);

const db = new Database(dbPath);

// Ensure tables exist
db.exec(`
    CREATE TABLE IF NOT EXISTS players_db (
        player_id TEXT PRIMARY KEY,
        display_name TEXT,
        wins INTEGER DEFAULT 0,
        losses INTEGER DEFAULT 0,
        disconnects INTEGER DEFAULT 0,
        rating REAL DEFAULT 1500.0,
        rd REAL DEFAULT 350.0,
        volatility REAL DEFAULT 0.06,
        country TEXT DEFAULT '',
        last_match TEXT,
        created_at TEXT DEFAULT (datetime('now'))
    );
    CREATE TABLE IF NOT EXISTS matches (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        p1_id TEXT NOT NULL,
        p2_id TEXT NOT NULL,
        winner_id TEXT NOT NULL,
        p1_char INTEGER,
        p2_char INTEGER,
        rounds INTEGER,
        created_at TEXT DEFAULT (datetime('now')),
        has_replay INTEGER DEFAULT 0
    );
    CREATE INDEX IF NOT EXISTS idx_matches_p1 ON matches(p1_id);
    CREATE INDEX IF NOT EXISTS idx_matches_p2 ON matches(p2_id);
`);

// Migration columns
try { db.exec('ALTER TABLE players_db ADD COLUMN disconnects INTEGER DEFAULT 0;'); } catch { }
try { db.exec("ALTER TABLE players_db ADD COLUMN country TEXT DEFAULT '';"); } catch { }

// ─── Legendary Players ──────────────────────────────────────────
// { id, name, country, rating, wins, losses, dc, char_idx }
const legends = [
    // === JAPAN — The Motherland ===
    { id: 'mov_3s',        name: 'MOV',        country: 'JP', rating: 2350, wins: 892, losses: 198, dc: 2,  char: 14 }, // Chun-Li
    { id: 'kuroda_3s',     name: 'Kuroda',     country: 'JP', rating: 2320, wins: 1205, losses: 310, dc: 0, char: 16 }, // Q (also plays everyone)
    { id: 'ko_3s',         name: 'KO',         country: 'JP', rating: 2280, wins: 756, losses: 201, dc: 1,  char: 2  }, // Yun
    { id: 'nuki_3s',       name: 'Nuki',       country: 'JP', rating: 2250, wins: 830, losses: 270, dc: 3,  char: 14 }, // Chun-Li
    { id: 'issei_3s',      name: 'Issei',      country: 'JP', rating: 2230, wins: 612, losses: 188, dc: 1,  char: 14 }, // Chun-Li
    { id: 'deshiken_3s',   name: 'Deshiken',   country: 'JP', rating: 2210, wins: 940, losses: 350, dc: 5,  char: 10 }, // Ken
    { id: 'sho_3s',        name: 'SHO',        country: 'JP', rating: 2190, wins: 580, losses: 210, dc: 2,  char: 1  }, // Ryu
    { id: 'hayao_3s',      name: 'Hayao',      country: 'JP', rating: 2170, wins: 720, losses: 300, dc: 4,  char: 5  }, // Hugo
    { id: 'daigo_3s',      name: 'Daigo',      country: 'JP', rating: 2150, wins: 650, losses: 280, dc: 1,  char: 10 }, // Ken
    { id: 'rx_3s',         name: 'RX',         country: 'JP', rating: 2140, wins: 810, losses: 370, dc: 3,  char: 12 }, // Urien
    { id: 'tokido_3s',     name: 'Tokido',     country: 'JP', rating: 2130, wins: 490, losses: 195, dc: 0,  char: 14 }, // Chun-Li
    { id: 'nitto_3s',      name: 'Nitto',      country: 'JP', rating: 2120, wins: 530, losses: 220, dc: 2,  char: 10 }, // Ken
    { id: 'tominaga_3s',   name: 'Tominaga',   country: 'JP', rating: 2100, wins: 670, losses: 310, dc: 6,  char: 15 }, // Makoto
    { id: 'mimora_3s',     name: 'Mimora',     country: 'JP', rating: 2080, wins: 590, losses: 280, dc: 2,  char: 15 }, // Makoto
    { id: 'rb_3s',         name: 'RB',         country: 'JP', rating: 2060, wins: 480, losses: 230, dc: 8,  char: 12 }, // Urien
    { id: 'kuni_3s',       name: 'Kuni',       country: 'JP', rating: 2040, wins: 610, losses: 310, dc: 4,  char: 1  }, // Ryu
    { id: 'boss_3s',       name: 'Boss',       country: 'JP', rating: 2020, wins: 440, losses: 220, dc: 3,  char: 10 }, // Ken
    { id: 'genki_3s',      name: 'Genki',      country: 'JP', rating: 2000, wins: 550, losses: 290, dc: 5,  char: 0  }, // Alex
    { id: 'pierrot_3s',    name: 'Pierrot',    country: 'JP', rating: 1980, wins: 420, losses: 230, dc: 1,  char: 19 }, // Remy
    { id: 'munakata_3s',   name: 'Munakata',   country: 'JP', rating: 1960, wins: 380, losses: 210, dc: 2,  char: 8  }, // Oro
    { id: 'kokujin_3s',    name: 'Kokujin',    country: 'JP', rating: 1940, wins: 510, losses: 310, dc: 7,  char: 3  }, // Dudley
    { id: 'sugiyama_3s',   name: 'Sugiyama',   country: 'JP', rating: 1920, wins: 390, losses: 240, dc: 3,  char: 14 }, // Chun-Li
    { id: 'ino_3s',        name: 'Ino',        country: 'JP', rating: 1900, wins: 360, losses: 220, dc: 2,  char: 15 }, // Makoto
    { id: 'ysb_3s',        name: 'YSB',        country: 'JP', rating: 1880, wins: 340, losses: 230, dc: 4,  char: 9  }, // Yang
    { id: 'pachikasu_3s',  name: 'Pachikasu',  country: 'JP', rating: 1850, wins: 410, losses: 310, dc: 6,  char: 11 }, // Sean

    // === INTERNATIONAL ===
    { id: 'jwong_3s',      name: 'Justin Wong', country: 'US', rating: 2160, wins: 580, losses: 250, dc: 2, char: 14 }, // Chun-Li
    { id: 'chirity_3s',    name: 'Chi-Rithy',   country: 'CA', rating: 2070, wins: 420, losses: 200, dc: 1, char: 10 }, // Ken
    { id: 'nica_ko_3s',    name: 'Nica KO',     country: 'US', rating: 1990, wins: 310, losses: 170, dc: 0, char: 2  }, // Yun
    { id: 'vanao_3s',      name: 'Vanao',       country: 'KR', rating: 1950, wins: 350, losses: 210, dc: 3, char: 6  }, // Ibuki
    { id: 'pyrolee_3s',    name: 'Pyrolee',     country: 'US', rating: 1910, wins: 290, losses: 190, dc: 1, char: 9  }, // Yang
    { id: 'ricky_3s',      name: 'Ricky Ortiz', country: 'US', rating: 2050, wins: 460, losses: 230, dc: 2, char: 14 }, // Chun-Li
    { id: 'valle_3s',      name: 'Alex Valle',  country: 'US', rating: 1970, wins: 380, losses: 220, dc: 1, char: 1  }, // Ryu
    { id: 'amir_3s',       name: 'Amir',        country: 'US', rating: 1930, wins: 320, losses: 200, dc: 0, char: 14 }, // Chun-Li
    { id: 'nychrisg_3s',   name: 'NYChrisG',    country: 'US', rating: 1870, wins: 270, losses: 190, dc: 2, char: 15 }, // Makoto
    { id: 'frankiebfg_3s', name: 'FrankieBFG',  country: 'US', rating: 1840, wins: 240, losses: 180, dc: 3, char: 10 }, // Ken

    // === FRANCE ===
    { id: 'thanatos_3s',   name: 'Thanatos',    country: 'FR', rating: 1960, wins: 370, losses: 220, dc: 2, char: 10 }, // Ken
    { id: 'steff_3s',      name: 'Steff',       country: 'FR', rating: 1890, wins: 300, losses: 210, dc: 4, char: 19 }, // Remy
    { id: 'otana_3s',      name: 'Otana',       country: 'FR', rating: 1940, wins: 340, losses: 200, dc: 1, char: 10 }, // Ken
    { id: 'billykane_3s',  name: 'Billykane',   country: 'FR', rating: 2010, wins: 430, losses: 210, dc: 2, char: 2  }, // Yun

    // === UNITED KINGDOM ===
    { id: 'ryanhart_3s',   name: 'Ryan Hart',   country: 'GB', rating: 2030, wins: 440, losses: 220, dc: 1, char: 2  }, // Yun
];

// ─── Insert players ─────────────────────────────────────────────
const upsert = db.prepare(`
    INSERT INTO players_db (player_id, display_name, wins, losses, disconnects, rating, rd, volatility, country, created_at)
    VALUES (?, ?, ?, ?, ?, ?, 60.0, 0.06, ?, datetime('now', '-' || abs(random() % 90) || ' days'))
    ON CONFLICT(player_id) DO UPDATE SET
        display_name = excluded.display_name,
        wins = excluded.wins,
        losses = excluded.losses,
        disconnects = excluded.disconnects,
        rating = excluded.rating,
        rd = excluded.rd,
        country = excluded.country
`);

const insertMatch = db.prepare(`
    INSERT INTO matches (p1_id, p2_id, winner_id, p1_char, p2_char, rounds, created_at)
    VALUES (?, ?, ?, ?, ?, ?, datetime('now', '-' || ? || ' days'))
`);

const insertAll = db.transaction(() => {
    // Insert all players
    for (const p of legends) {
        upsert.run(p.id, p.name, p.wins, p.losses, p.dc, p.rating, p.country);
        console.log(`  [+] ${p.name.padEnd(14)} ${p.country} Rating:${p.rating} ${p.wins}W/${p.losses}L Char:${p.char}`);
    }

    // Generate synthetic match history so most_played_char works
    // For each player, create N matches where they used their main character
    console.log('\nGenerating match history...');
    for (const p of legends) {
        // Create 5-15 matches for each player using their main
        const matchCount = 5 + Math.floor(Math.random() * 11);
        for (let m = 0; m < matchCount; m++) {
            // Pick a random opponent
            let opp;
            do {
                opp = legends[Math.floor(Math.random() * legends.length)];
            } while (opp.id === p.id);

            const won = Math.random() < (p.rating / (p.rating + opp.rating));
            const winner = won ? p.id : opp.id;
            const daysAgo = Math.floor(Math.random() * 60);
            const rounds = 2 + Math.floor(Math.random() * 3);

            insertMatch.run(p.id, opp.id, winner, p.char, opp.char, rounds, daysAgo);
        }
    }
});

insertAll();

// Verify
const count = db.prepare('SELECT COUNT(*) as cnt FROM players_db').get().cnt;
const matchCount = db.prepare('SELECT COUNT(*) as cnt FROM matches').get().cnt;
console.log(`\n✅ Done! ${count} players, ${matchCount} matches in ${dbPath}`);

db.close();
