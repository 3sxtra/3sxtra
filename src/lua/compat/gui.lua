-- FBNeo gui compatibility layer for 3SX
-- Stubs for gui.box(), gui.text(), gui.register() etc.
-- In 3SX, all overlay rendering goes through RmlUi — these are no-ops
-- that prevent errors when loading unmodified FBNeo Lua scripts.

local gui = {}

--- Draw a filled rectangle (no-op — use RmlUi for overlays).
function gui.box(x1, y1, x2, y2, fillcolor, outlinecolor) end

--- Draw text (no-op — use RmlUi for overlays).
function gui.text(x, y, text, color) end

--- Draw a line (no-op).
function gui.line(x1, y1, x2, y2, color) end

--- Draw a pixel (no-op).
function gui.pixel(x, y, color) end

--- Register a drawing callback (no-op — RmlUi handles rendering).
function gui.register(fn) end

--- Clear the overlay (no-op).
function gui.clearuncommitted() end

return gui
