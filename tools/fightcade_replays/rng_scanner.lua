local rb = memory.readbyte
local rw = memory.readword

-- We already confirmed Random_ix16 = 0x020155E8
-- Now scanning for:
--   Random_ix32    : masked 0x7F, range 0-127
--   Random_ix16_ex : masked 0x0F, range 0-15
--   Random_ix32_ex : masked 0x1F, range 0-31

local CONFIRMED_IX16 = 0x020155E8

local cand_ix32 = {}     -- 0-127
local cand_ix16ex = {}   -- 0-15
local cand_ix32ex = {}   -- 0-31

local state = 0
local frame = 0
local last_step = 0

print("========================================")
print("  RNG Scanner v2 — Finding ix32, ix16_ex, ix32_ex")
print("  Confirmed: Random_ix16 = 0x020155E8")
print("")
print("  Press 'K' to snapshot, play 1sec, press 'L' to filter")
print("  Repeat 'L' until candidates converge")
print("========================================")

emu.registerbefore(function()
    frame = frame + 1
    local keys = input.get()
    
    if keys["K"] and state == 0 then
        print("")
        print("K PRESSED! Snapshotting...")
        local c32, c16ex, c32ex = 0, 0, 0
        
        for addr = 0x02000000, 0x02080000, 2 do
            if addr ~= CONFIRMED_IX16 then
                local val = rw(addr)
                
                if val < 128 then
                    cand_ix32[addr] = { val = val, matches = 0 }
                    c32 = c32 + 1
                end
                if val < 16 then
                    cand_ix16ex[addr] = { val = val, matches = 0 }
                    c16ex = c16ex + 1
                end
                if val < 32 then
                    cand_ix32ex[addr] = { val = val, matches = 0 }
                    c32ex = c32ex + 1
                end
            end
        end
        
        print("  ix32 candidates (0-127): " .. c32)
        print("  ix16_ex candidates (0-15): " .. c16ex)
        print("  ix32_ex candidates (0-31): " .. c32ex)
        print("Play for a bit and press 'L'.")
        state = 1
    end
    
    if keys["L"] and state == 1 then
        if (frame - last_step > 30) then
            last_step = frame
            
            -- Filter ix32 (0-127, must change with positive sequential delta mod 128)
            local next32 = {}
            local count32 = 0
            for addr, data in pairs(cand_ix32) do
                local val = rw(addr)
                if val < 128 and val ~= data.val then
                    local d = (val - data.val) % 128
                    if d > 0 and d < 40 then
                        data.val = val
                        data.matches = data.matches + 1
                        next32[addr] = data
                        count32 = count32 + 1
                    end
                end
            end
            cand_ix32 = next32
            
            -- Filter ix16_ex (0-15, must change with positive delta mod 16)
            local next16ex = {}
            local count16ex = 0
            for addr, data in pairs(cand_ix16ex) do
                local val = rw(addr)
                if val < 16 and val ~= data.val then
                    local d = (val - data.val) % 16
                    if d > 0 and d < 10 then
                        data.val = val
                        data.matches = data.matches + 1
                        next16ex[addr] = data
                        count16ex = count16ex + 1
                    end
                end
            end
            cand_ix16ex = next16ex
            
            -- Filter ix32_ex (0-31, must change with positive delta mod 32)
            local next32ex = {}
            local count32ex = 0
            for addr, data in pairs(cand_ix32ex) do
                local val = rw(addr)
                if val < 32 and val ~= data.val then
                    local d = (val - data.val) % 32
                    if d > 0 and d < 20 then
                        data.val = val
                        data.matches = data.matches + 1
                        next32ex[addr] = data
                        count32ex = count32ex + 1
                    end
                end
            end
            cand_ix32ex = next32ex
            
            print("L PRESSED! Filtered.")
            print("  ix32 remaining: " .. count32)
            print("  ix16_ex remaining: " .. count16ex)
            print("  ix32_ex remaining: " .. count32ex)
            
            if count32 > 0 and count32 <= 20 then
                print("  --- ix32 (0-127) ---")
                for addr, data in pairs(cand_ix32) do
                    print(string.format("    0x%08X (val=%d, adv=%d)", addr, data.val, data.matches))
                end
            end
            
            if count16ex > 0 and count16ex <= 20 then
                print("  --- ix16_ex (0-15) ---")
                for addr, data in pairs(cand_ix16ex) do
                    print(string.format("    0x%08X (val=%d, adv=%d)", addr, data.val, data.matches))
                end
            end
            
            if count32ex > 0 and count32ex <= 20 then
                print("  --- ix32_ex (0-31) ---")
                for addr, data in pairs(cand_ix32ex) do
                    print(string.format("    0x%08X (val=%d, adv=%d)", addr, data.val, data.matches))
                end
            end
            
            if count32 == 0 and count16ex == 0 and count32ex == 0 then
                print("All filtered to 0! Press 'K' to restart.")
                state = 0
            end
        end
    end
end)
