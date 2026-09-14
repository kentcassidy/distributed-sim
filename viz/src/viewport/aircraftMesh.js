import * as THREE from 'three'
import { LineSegments2 } from 'three/addons/lines/LineSegments2.js'
import { LineSegmentsGeometry } from 'three/addons/lines/LineSegmentsGeometry.js'
import { LineMaterial } from 'three/addons/lines/LineMaterial.js'

// A minimal "paper airplane": nose forward along +X (the sim's downrange axis),
// wings spread in +/-Y, and a central keel folded down in -Z so it reads as a 3D,
// directional shape from any camera angle -- not just a flat triangle you lose edge-on.
//
// This is deliberately a marker, not a model: it shows POSITION and HEADING and
// nothing else. Orientation comes from the aircraft's quaternion at render time
// (mesh.quaternion.copy(...)), so the shape's only job is to point sensibly at +X.
export function makeAircraftMesh(color = 0xe08a3c) {
  const nose  = [ 1.0,  0.0,  0.0]
  const tailL = [-1.0,  0.8,  0.0]
  const tailR = [-1.0, -0.8,  0.0]
  const keel  = [-0.7,  0.0, -0.35]

  // Two triangles meeting along the nose->keel fold: a folded-paper silhouette.
  const positions = new Float32Array([
    ...nose, ...tailL, ...keel,   // left wing
    ...nose, ...keel,  ...tailR,  // right wing
  ])

  const geom = new THREE.BufferGeometry()
  geom.setAttribute('position', new THREE.BufferAttribute(positions, 3))
  geom.computeVertexNormals()

  const mat = new THREE.MeshStandardMaterial({
    color,
    roughness: 0.6,
    metalness: 0.0,
    side: THREE.DoubleSide,   // visible from above and below
    flatShading: true,        // crisp facets -- reads clean, no smoothing artifacts
    transparent: true,        // opacity is driven live (run crossfade); 1.0 reads as opaque
  })

  const mesh = new THREE.Mesh(geom, mat)

  // Selection outline: a faint red edge overlay with depthTest OFF, so it always draws on top
  // (reads from any camera projection). A fat line (LineSegments2) for a genuine pixel width --
  // WebGL ignores linewidth on basic lines. Child of the mesh, so it follows position/scale;
  // width is pixel-based so it stays constant regardless of highlight scale. SceneController
  // keeps its material.resolution set and toggles visibility for "highlight" mode.
  const edges = new THREE.EdgesGeometry(geom)
  const outlineGeo = new LineSegmentsGeometry().fromEdgesGeometry(edges)
  edges.dispose()
  const outline = new LineSegments2(
    outlineGeo,
    new LineMaterial({ color: 0xff3b30, transparent: true, opacity: 0.55, linewidth: 2, depthTest: false }),
  )
  outline.renderOrder = 5
  outline.visible = false
  mesh.add(outline)
  mesh.userData.outline = outline

  return mesh
}
