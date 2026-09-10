import * as THREE from 'three'

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
  })

  return new THREE.Mesh(geom, mat)
}
