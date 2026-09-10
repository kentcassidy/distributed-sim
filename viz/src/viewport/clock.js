// One shared playback clock for ALL viewports, so every pane renders the exact same
// sim time (synced panes). App advances it once per frame; each SceneController reads
// `playback.t` in its own render loop.
export const playback = { t: 0, playing: false, speed: 1, duration: 0, _last: 0 }

export function advance(now) {
  if (!playback._last) playback._last = now
  const dt = (now - playback._last) / 1000
  playback._last = now
  if (playback.playing && playback.duration > 0) {
    playback.t += dt * playback.speed
    if (playback.t >= playback.duration) playback.t = 0 // loop the replay
  }
}
