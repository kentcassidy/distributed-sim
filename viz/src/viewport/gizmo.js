import * as THREE from 'three'
import { hexToCss } from '../config.js'

// A small Blender-style orientation gizmo for the top-right corner: three labelled
// axes that mirror the main camera's orientation. Rendered by SceneController into a
// scissored corner viewport with its own tiny scene + camera.

export function buildGizmo(palette) {
  const scene = new THREE.Scene()
  const camera = new THREE.PerspectiveCamera(40, 1, 0.1, 100)
  camera.up.set(0, 0, 1)

  addArm(scene, new THREE.Vector3(1, 0, 0), palette.axisX, 'X')
  addArm(scene, new THREE.Vector3(0, 1, 0), palette.axisY, 'Y')
  addArm(scene, new THREE.Vector3(0, 0, 1), palette.axisZ, 'Z')

  const dist = 3.4
  const fwd = new THREE.Vector3()
  function update(mainCamera) {
    // same orientation as the main camera, backed off so the origin is centered
    camera.quaternion.copy(mainCamera.quaternion)
    fwd.set(0, 0, -1).applyQuaternion(mainCamera.quaternion) // main view direction
    camera.position.copy(fwd).multiplyScalar(-dist)
    camera.updateMatrixWorld()
  }

  return { scene, camera, update }
}

function addArm(scene, dir, color, letter) {
  // colored arm from origin to the + tip
  const geom = new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(0, 0, 0), dir.clone()])
  scene.add(new THREE.Line(geom, new THREE.LineBasicMaterial({ color, linewidth: 2 })))

  // knob at the tip
  const knob = new THREE.Mesh(
    new THREE.SphereGeometry(0.16, 16, 12),
    new THREE.MeshBasicMaterial({ color }),
  )
  knob.position.copy(dir)
  scene.add(knob)

  // letter on the knob
  const label = makeLabel(letter, hexToCss(color))
  label.position.copy(dir).multiplyScalar(1.0)
  scene.add(label)
}

function makeLabel(text, cssColor) {
  const canvas = document.createElement('canvas')
  canvas.width = 64
  canvas.height = 64
  const ctx = canvas.getContext('2d')
  ctx.font = 'bold 40px system-ui, sans-serif'
  ctx.fillStyle = '#ffffff'
  ctx.strokeStyle = cssColor
  ctx.lineWidth = 4
  ctx.textAlign = 'center'
  ctx.textBaseline = 'middle'
  ctx.strokeText(text, 32, 34)
  ctx.fillText(text, 32, 34)

  const tex = new THREE.CanvasTexture(canvas)
  tex.minFilter = THREE.LinearFilter
  const spr = new THREE.Sprite(new THREE.SpriteMaterial({ map: tex, transparent: true, depthTest: false }))
  spr.scale.set(0.42, 0.42, 1)
  return spr
}
