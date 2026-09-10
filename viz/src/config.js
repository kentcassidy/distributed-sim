// Central knobs. Desmos-clean, Blender-tinted.

export const PALETTE = {
  background: 0xf7f7f5, // soft off-white ground
  walls: 0xeceae3,      // worldspace room walls (subtle)
  grid: 0xdedcd4,       // (reserved) floor gridlines
  axisX: 0xdb5a4b,      // red
  axisY: 0x5aa02c,      // green
  axisZ: 0x3b7fd4,      // blue
}

// Stable per-federate colors, assigned in file order (F1, F2, ...).
const FEDERATE_COLORS = [0xe08a3c, 0x3b82c4, 0x8e5bd0, 0x2fa37a, 0xd45087, 0xc9a227]
export function federateColor(index = 0) {
  return FEDERATE_COLORS[index % FEDERATE_COLORS.length]
}

// Aircraft are markers, sized in WORLD UNITS (metres). Real aircraft are tiny next
// to a multi-kilometre worldspace; this is the default marker LENGTH, tweakable live.
export const DEFAULT_AIRCRAFT_SIZE = 40
export const AIRCRAFT_SIZE_RANGE = { min: 5, max: 300 }

export const hexToCss = (hex) => '#' + hex.toString(16).padStart(6, '0')
