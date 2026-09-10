// Central knobs. Desmos-clean, Blender-tinted.

// Theme palettes for the 3D scene (the DOM/panel theme lives in App.vue's CSS vars).
// Grid + tick contrast bumped up a touch from the first pass for legibility.
export const THEMES = {
  light: {
    background: 0xf4f3ef,
    walls: 0xe7e5dd,
    gridMinor: 0xcbc8be,
    gridMajor: 0xa9a79d,
    tick: '#6d6b64',
    axisX: 0xcf4f41,
    axisY: 0x4f9128,
    axisZ: 0x2f74c8,
  },
  dark: {
    background: 0x1b1b1d,
    walls: 0x27272a,
    gridMinor: 0x3b3b41,
    gridMajor: 0x56565d,
    tick: '#b7b5ad',
    axisX: 0xe8695a,
    axisY: 0x74c04a,
    axisZ: 0x5aa0ea,
  },
}

// Stable per-federate colors, assigned in file order (F1, F2, ...).
const FEDERATE_COLORS = [0xe08a3c, 0x3b82c4, 0x8e5bd0, 0x2fa37a, 0xd45087, 0xc9a227]
export function federateColor(index = 0) {
  return FEDERATE_COLORS[index % FEDERATE_COLORS.length]
}

// Aircraft are markers, sized in WORLD UNITS (metres). Real aircraft are tiny next
// to a multi-kilometre worldspace; this is the default marker LENGTH, tweakable live.
export const DEFAULT_AIRCRAFT_SIZE = 40
export const AIRCRAFT_SIZE_RANGE = { min: 5, max: 400 }

export const hexToCss = (hex) => '#' + hex.toString(16).padStart(6, '0')
