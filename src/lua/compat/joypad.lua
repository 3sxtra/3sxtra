-- FBNeo joypad compatibility layer for 3SX
-- Translates joypad.get()/joypad.set() to engine.get_input()/engine.set_lever_buff()

local joypad = {}

-- Button bitmasks matching CPS3 input encoding
local BUTTONS = {
    up    = 0x0001, down  = 0x0002,
    left  = 0x0004, right = 0x0008,
    LP    = 0x0010, MP    = 0x0020, HP    = 0x0040,
    LK    = 0x0100, MK    = 0x0200, HK    = 0x0400,
    start = 0x0800, coin  = 0x1000,
}

--- Get current input state as a table of button names -> booleans.
--- Matches FBNeo's joypad.get() format.
--- @param player_id number 1 or 2 (FBNeo uses "P1" table key)
--- @return table
function joypad.get(player_id)
    local id = player_id or 1
    local raw = engine.get_input(id)
    local result = {}
    for name, mask in pairs(BUTTONS) do
        result[name] = (raw & mask) ~= 0
    end
    return result
end

--- Set input state. Translates button table to Lever_Buff value.
--- @param inputs table {up=bool, LP=bool, ...}
--- @param player_id number 1 or 2
function joypad.set(inputs, player_id)
    local id = (player_id or 1) - 1  -- engine uses 0-indexed
    local val = 0
    if inputs then
        for name, mask in pairs(BUTTONS) do
            if inputs[name] then
                val = val | mask
            end
        end
    end
    engine.set_lever_buff(id, val)
end

return joypad
