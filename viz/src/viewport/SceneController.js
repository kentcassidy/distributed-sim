import * as THREE from 'three'
import { OrbitControls } from 'three/addons/controls/OrbitControls.js'
import { makeAircraftMesh } from './aircraftMesh.js'

// SceneController -- a plain (non-Vue) class that owns one Three.js viewport:
// renderer, scene, camera, controls, and the render loop. The Vue component
// instantiates it in onMounted and calls dispose() on unmount. Keeping this
// framework-free is what lets us later run N of them (one per federate viewpoint)
// without fighting Vue's reactivity.
//
// Convention (matches the sim + Blender): the world is Z-UP. The sim's z axis is
// altitude, so "up" on screen is +Z, and the axes read X-red / Y-green / Z-blue.

// Desmos-clean, Blender-tinted palette.
const BG          = 0xf7f7f5   // soft off-white ground
const GRID_LINES  = 0xdad8d1   // quiet grid
const GRID_CENTER = 0xbdbbb2   // slightly darker center cross
const AC_ORANGE   = 0xe08a3c   // Blender-ish selection orange

export class SceneController {
  constructor(canvas) {
    this.canvas = canvas
    this._raf = null

    // --- Renderer ---
    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true })
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2))

    // --- Scene ---
    this.scene = new THREE.Scene()
    this.scene.background = new THREE.Color(BG)

    // --- Camera: isometric-ish start, Z-up ---
    this.camera = new THREE.PerspectiveCamera(50, 1, 0.1, 100000)
    this.camera.up.set(0, 0, 1)          // +Z is up (Blender / sim convention)
    this.camera.position.set(8, -8, 6)   // front-right-top, a Blender-like default
    this.camera.lookAt(0, 0, 0)

    // --- Controls: orbit about the origin; MIDDLE-drag rotates (as requested) ---
    this.controls = new OrbitControls(this.camera, this.renderer.domElement)
    this.controls.target.set(0, 0, 0)
    this.controls.enableDamping = true
    this.controls.dampingFactor = 0.08
    this.controls.mouseButtons = {
      LEFT: THREE.MOUSE.PAN,      // left-drag pans
      MIDDLE: THREE.MOUSE.ROTATE, // middle-drag orbits about center
      RIGHT: THREE.MOUSE.PAN,     // right-drag pans too
    }
    // wheel zoom is on by default (controls.enableZoom)

    // --- Ground grid on the XY plane (default GridHelper lies in XZ, so rotate it) ---
    const grid = new THREE.GridHelper(20, 20, GRID_CENTER, GRID_LINES)
    grid.rotation.x = Math.PI / 2
    this.scene.add(grid)

    // --- XYZ axes: X-red, Y-green, Z-blue (AxesHelper's default = the standard) ---
    this.scene.add(new THREE.AxesHelper(5))

    // --- Lights: subtle, just enough for the standard material to show form ---
    this.scene.add(new THREE.AmbientLight(0xffffff, 0.75))
    const key = new THREE.DirectionalLight(0xffffff, 0.9)
    key.position.set(5, -3, 8)
    this.scene.add(key)

    // --- One sample aircraft, oriented from a REAL quaternion ---
    // Proof the pipeline works end to end: a mesh placed in the world and rotated by
    // a (x, y, z, w) quaternion -- the exact shape the NDJSON carries. A small pitch
    // so it's visibly not axis-aligned. (Real frames replace this in the next step;
    // the sim-frame <-> graphics-frame axis mapping gets pinned down when data loads.)
    this.sample = makeAircraftMesh(AC_ORANGE)
    this.sample.position.set(0, 0, 1)
    this.sample.quaternion.set(0, 0.15, 0, 1).normalize()  // (x, y, z, w)
    this.scene.add(this.sample)

    // --- Resize: track the canvas's parent box, not the window ---
    this._onResize = this._onResize.bind(this)
    this._resizeObserver = new ResizeObserver(this._onResize)
    this._resizeObserver.observe(canvas.parentElement)
    this._onResize()

    // --- Render loop ---
    this._loop = this._loop.bind(this)
    this._raf = requestAnimationFrame(this._loop)
  }

  _onResize() {
    const parent = this.canvas.parentElement
    const w = parent.clientWidth
    const h = parent.clientHeight
    if (w === 0 || h === 0) return
    this.renderer.setSize(w, h, false)   // false: don't touch the canvas's CSS size
    this.camera.aspect = w / h
    this.camera.updateProjectionMatrix()
  }

  _loop() {
    this.controls.update()   // needed for damping
    this.renderer.render(this.scene, this.camera)
    this._raf = requestAnimationFrame(this._loop)
  }

  dispose() {
    if (this._raf) cancelAnimationFrame(this._raf)
    this._resizeObserver.disconnect()
    this.controls.dispose()
    this.renderer.dispose()
  }
}
