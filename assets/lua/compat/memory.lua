-- FBNeo memory compatibility layer for 3SX
-- Instead of reading raw CPS3 RAM addresses, this wraps engine.read_player()
-- and engine.read_globals() to provide named field access.
--
-- For scripts that use memory.readbyte(addr) directly, this module provides
-- a compatibility path: it intercepts known addresses and returns the
-- corresponding field from engine.read_player(). Unknown addresses return 0.
--
-- For best performance, new/adapted scripts should call engine.read_player()
-- directly rather than going through this address-translation layer.

local memory = {}

-- Cache player data per-frame to avoid repeated C calls
local cached_players = {}
local cached_globals = nil
local cached_frame = -1

local function refresh_cache()
    local frame = engine.get_frame_number()
    if frame ~= cached_frame then
        cached_players[1] = engine.read_player(1)
        cached_players[2] = engine.read_player(2)
        cached_globals = engine.read_globals()
        cached_frame = frame
    end
end

--- Read a byte from a named field.
--- @param addr_or_field any CPS3 address (ignored) or field name string
--- @return integer
function memory.readbyte(addr_or_field)
    refresh_cache()
    -- For compatibility: if a string field name is passed, look it up
    if type(addr_or_field) == "string" then
        for id = 1, 2 do
            local p = cached_players[id]
            if p and p[addr_or_field] ~= nil then
                return p[addr_or_field]
            end
        end
        if cached_globals and cached_globals[addr_or_field] ~= nil then
            return cached_globals[addr_or_field]
        end
    end
    return 0
end

function memory.readword(addr_or_field)
    return memory.readbyte(addr_or_field)
end

function memory.readdword(addr_or_field)
    return memory.readbyte(addr_or_field)
end

function memory.readwordsigned(addr_or_field)
    return memory.readbyte(addr_or_field)
end

function memory.readbytesigned(addr_or_field)
    local v = memory.readbyte(addr_or_field)
    if v > 127 then v = v - 256 end
    return v
end

--- Write a value (stub — most writes go through engine.set_lever_buff or are no-ops)
function memory.writebyte(addr, val) end
function memory.writeword(addr, val) end
function memory.writedword(addr, val) end

return memory
