-- FBNeo emu compatibility layer for 3SX
-- Provides callback registration and basic emulator state functions.

local emu = {}

-- Callback storage
local callbacks = {
    start = {},
    before = {},
    after = {},
}

--- Register a function called at match start.
--- @param fn function
function emu.registerstart(fn)
    callbacks.start[#callbacks.start + 1] = fn
end

--- Register a function called before each frame update.
--- This is the primary per-frame hook for training logic.
--- @param fn function
function emu.registerbefore(fn)
    callbacks.before[#callbacks.before + 1] = fn
end

--- Register a function called after each frame update.
--- @param fn function
function emu.registerafter(fn)
    callbacks.after[#callbacks.after + 1] = fn
end

--- Get current frame count.
--- @return integer
function emu.framecount()
    return engine.get_frame_number()
end

--- Speed mode (no-op in decomp — no fast-forward via emu API).
--- @param mode string "normal", "turbo", "nothrottle"
function emu.speedmode(mode)
    -- No-op: speed is controlled via the game's own fast-forward flag
end

--- ROM name (hardcoded for 3SX decomp).
--- @return string
function emu.romname()
    return "sfiii3nr1"
end

-- Called by the engine bridge per-frame tick
function emu._fire_before()
    for _, fn in ipairs(callbacks.before) do fn() end
end

function emu._fire_after()
    for _, fn in ipairs(callbacks.after) do fn() end
end

function emu._fire_start()
    for _, fn in ipairs(callbacks.start) do fn() end
end

--- Clear all registered callbacks (used on mode exit).
function emu._clear()
    callbacks.start = {}
    callbacks.before = {}
    callbacks.after = {}
end

return emu
