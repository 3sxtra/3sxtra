#Requires AutoHotkey v2.0
#SingleInstance Force

; ============================================================
;  Input Mirror — Forward keyboard to all 3sx.exe windows
; ============================================================
;  Usage:
;    1. Launch your game instances first
;    2. Run this script (double-click or right-click → Run Script)
;    3. Press Ctrl+Alt+M to toggle mirroring ON/OFF
;    4. When ON, every key you press is sent to ALL game windows
;    5. Press Ctrl+Alt+Q to quit the script
; ============================================================

; --- Configuration ---
WINDOW_MATCH := "ahk_exe 3sx.exe"   ; Match all windows of 3sx.exe
global MirrorActive := true

; --- Tray icon setup ---
A_IconTip := "Input Mirror (ON)"
TraySetIcon("shell32.dll", 172)
InstallKeybdHook()

; --- Toggle hotkey: Ctrl+Alt+M ---
^!m:: {
    global MirrorActive
    MirrorActive := !MirrorActive
    if MirrorActive {
        A_IconTip := "Input Mirror (ON)"
        TraySetIcon("shell32.dll", 172)
        ToolTip("Input Mirror: ON`nAll keys → all 3sx.exe windows")
        SetTimer(() => ToolTip(), -2000)
        InstallKeybdHook()
    } else {
        A_IconTip := "Input Mirror (OFF)"
        TraySetIcon("shell32.dll", 44)
        ToolTip("Input Mirror: OFF")
        SetTimer(() => ToolTip(), -2000)
    }
}

; --- Quit hotkey: Ctrl+Alt+Q ---
^!q:: ExitApp()

; --- The core: intercept keys and send to all game windows ---
; We use an Input Hook to watch for all key activity, then
; use ControlSend to forward to unfocused windows.

; List of keys your game uses (add/remove as needed)
GAME_KEYS := [
    "w", "a", "s", "d",           ; Movement (WASD)
    "u", "i", "j", "k",           ; Buttons (West, North, South, East)
    "o", "p",                      ; Shoulders (R.Shoulder, L.Shoulder)
    "l", "`;",                     ; Triggers (R.Trigger, L.Trigger)
    "9", "0",                      ; Sticks (L.Stick, R.Stick)
    "Enter", "Backspace",          ; Start, Back/Select
    "1", "2", "3", "4", "5",      ; Number keys (coins, etc.)
    "Escape",                      ; Escape
]

; Register hotkeys for key-down and key-up of each game key
for key in GAME_KEYS {
    ; Key down
    Hotkey("*" key, SendToAllWindows.Bind(key, "down"))
    ; Key up
    Hotkey("*" key " Up", SendToAllWindows.Bind(key, "up"))
}

SendToAllWindows(key, direction, thisHotkey) {
    global MirrorActive, WINDOW_MATCH

    if !MirrorActive {
        ; Not active — let the key pass through normally
        if direction = "down"
            Send("{Blind}{" key "}")
        ; Up events pass through automatically
        return
    }

    ; Get all matching window IDs
    wins := WinGetList(WINDOW_MATCH)

    if wins.Length = 0 {
        ; No game windows found, pass through
        if direction = "down"
            Send("{Blind}{" key "}")
        return
    }

    ; Build the key string for ControlSend
    if direction = "down"
        keyStr := "{" key " down}"
    else
        keyStr := "{" key " up}"

    ; Send to EVERY game window (including focused one)
    for hwnd in wins {
        try {
            ControlSend(keyStr,, "ahk_id " hwnd)
        }
    }
}
