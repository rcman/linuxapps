// public/main.js
import * as THREE from 'https://cdn.jsdelivr.net/npm/three@0.157.0/build/three.module.js';
import { OrbitControls } from 'https://cdn.jsdelivr.net/npm/three@0.157.0/examples/jsm/controls/OrbitControls.js';
import { Water } from 'https://cdn.jsdelivr.net/npm/three@0.157.0/examples/jsm/objects/Water.js';
import { Sky } from 'https://cdn.jsdelivr.net/npm/three@0.157.0/examples/jsm/objects/Sky.js';

// --- Basic app state ---
const worldSize = 1000;
const treeCount = 1100;
const rockCount = 1100;

let scene, camera, renderer, controls;
let localPlayer = null;
let otherPlayers = new Map();
let models = {};
let mixers = [];
let clock = new THREE.Clock();

const socket = new WebSocket((location.protocol === 'https:' ? 'wss' : 'ws') + '://' + location.host);

let sessionId = localStorage.getItem('sessionId') || null;

// --- UI elements ---
const hint = document.getElementById('hint');
const inventoryList = document.getElementById('inventoryList');
const buildModeSpan = document.getElementById('buildMode');

init();
animate();

function init() {
  scene = new THREE.Scene();
  scene.background = new THREE.Color(0x9fbfff);

  camera = new THREE.PerspectiveCamera(75, innerWidth/innerHeight, 0.1, 5000);
  camera.position.set(0, 10, 20);

  renderer = new THREE.WebGLRenderer({ antialias:true });
  renderer.setSize(innerWidth, innerHeight);
  document.body.appendChild(renderer.domElement);

  // Light
  const hemi = new THREE.HemisphereLight(0xffffff, 0x444455, 1.0);
  hemi.position.set(0,200,0); scene.add(hemi);
  const dir = new THREE.DirectionalLight(0xffffff, 0.6); dir.position.set(-1,1,-1); scene.add(dir);

  // Controls (for debug; player camera will be linked below)
  controls = new OrbitControls(camera, renderer.domElement);
  controls.target.set(0,5,0); controls.update();

  // Ground (height map simple)
  const ground = generateTerrain();
  scene.add(ground);

  // Water using Water.js
  const waterGeometry = new THREE.PlaneGeometry(worldSize, worldSize);
  const water = new Water(waterGeometry, {
    color: '#001e3c',
    scale: 4,
    flowDirection: new THREE.Vector2(1,1),
    textureWidth: 1024,
    textureHeight: 1024
  });
  water.rotation.x = -Math.PI/2;
  water.position.y = -2.2;
  scene.add(water);

  // Load models (ObjectLoader JSON)
  loadModels([
    'appletree.json','barrel.json','storagebox.json','craftingtable.json','forge.json',
    'rabbit.json','wolf.json','bear.json','building1.json','human.json',
    'foundation.json','wall.json','wallwithwindow.json','wallwithdoor.json','ceiling.json','door.json'
  ]).then(()=> {
    // Scatter objects
    scatterObjects();
    spawnAnimals();
    setupLocalPlayer();
    hint.innerText = 'Loaded. Use WASD to move, mouse to look. E to interact.';
  });

  // Resize
  window.addEventListener('resize', () => {
    camera.aspect = innerWidth/innerHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(innerWidth, innerHeight);
  });

  // Input
  setupInput();

  // WebSocket handlers
  socket.addEventListener('open', () => {
    socket.send(JSON.stringify({ type: 'join', sessionId }));
  });

  socket.addEventListener('message', (ev) => {
    const data = JSON.parse(ev.data);
    if (data.type === 'joined') {
      if (data.player && data.player.id) {
        sessionId = data.player.id;
        localStorage.setItem('sessionId', sessionId);
        localPlayer = data.player;
        updateInventoryUI();
        // spawn other players
        for (const o of data.others) {
          if (o.id === localPlayer.id) continue;
          spawnRemotePlayer(o);
        }
      }
    } else if (data.type === 'playerJoined') {
      if (!localPlayer || data.id === localPlayer.id) return;
      spawnRemotePlayer({ id: data.id, name: data.name, pos: data.pos, rot: data.rot });
    } else if (data.type === 'playerUpdate') {
      if (data.id === localPlayer?.id) return;
      const p = otherPlayers.get(data.id);
      if (p) {
        p.object.position.set(data.pos.x, data.pos.y, data.pos.z);
        p.object.rotation.y = data.rot;
      }
    } else if (data.type === 'playerLeft') {
      removeRemotePlayer(data.id);
    } else if (data.type === 'playerSaved') {
      // server persisted player
    }
  });
}

// --- Terrain generation: procedural height + mesh ---
function generateTerrain() {
  const size = 256;
  const geometry = new THREE.PlaneGeometry(worldSize, worldSize, size - 1, size - 1);
  geometry.rotateX(-Math.PI/2);
  // Simple perlin-like heights using sin waves (fast)
  for (let i=0;i<geometry.attributes.position.count;i++) {
    const x = geometry.attributes.position.getX(i);
    const z = geometry.attributes.position.getZ(i);
    const height = Math.sin(x*0.005)*4 + Math.cos(z*0.008)*3 + Math.sin((x+z)*0.003)*2;
    geometry.attributes.position.setY(i, height);
  }
  geometry.computeVertexNormals();
  const mat = new THREE.MeshStandardMaterial({ color: 0x556b2f });
  const mesh = new THREE.Mesh(geometry, mat);
  mesh.receiveShadow = true;
  return mesh;
}

// --- Model loader helper ---
async function loadModels(list) {
  const loader = new THREE.ObjectLoader();
  for (const name of list) {
    const url = `./models/${name}`;
    try {
      const obj = await new Promise((res, rej) => {
        loader.load(url, res, null, rej);
      });
      models[name] = obj;
      console.log('Loaded', name);
    } catch (e) {
      console.warn('Model not found or failed to load:', name, e);
    }
  }
}

// --- Scatter trees/rocks/barrels ---
function scatterObjects() {
  // trees
  const treeNames = ['appletree.json'];
  for (let i=0;i<treeCount;i++) {
    const nx = (Math.random()-0.5)*worldSize;
    const nz = (Math.random()-0.5)*worldSize;
    const y = getHeightAt(nx, nz);
    if (y < -1) continue; // avoid underwater for trees
    const name = treeNames[Math.floor(Math.random()*treeNames.length)];
    const model = models[name] ? models[name].clone() : new THREE.Mesh(new THREE.ConeGeometry(0.8,2,6), new THREE.MeshStandardMaterial({color:0x228833}));
    model.position.set(nx, y, nz);
    model.scale.setScalar(1 + Math.random()*0.8);
    scene.add(model);
  }
  // rocks same approach using barrel as placeholder (or rock model if you have)
  for (let i=0;i<rockCount;i++) {
    const nx = (Math.random()-0.5)*worldSize;
    const nz = (Math.random()-0.5)*worldSize;
    const y = getHeightAt(nx, nz);
    if (y < -1) continue;
    const rock = new THREE.Mesh(new THREE.DodecahedronGeometry(0.5 + Math.random()*1.2), new THREE.MeshStandardMaterial({color:0x888888}));
    rock.position.set(nx,y,nz);
    rock.userData.resource = 'stone';
    scene.add(rock);
  }
  // barrels (loot)
  for (let i=0;i<200;i++) {
    const nx = (Math.random()-0.5)*worldSize;
    const nz = (Math.random()-0.5)*worldSize;
    const y = getHeightAt(nx,nz);
    if (y < -1) continue;
    const model = models['barrel.json'] ? models['barrel.json'].clone() : new THREE.Mesh(new THREE.CylinderGeometry(0.4,0.4,1), new THREE.MeshStandardMaterial({color:0x804000}));
    model.position.set(nx,y,nz);
    model.userData.loot = true;
    scene.add(model);
  }
}

// Helper: approximate height (sample scene ground)
function getHeightAt(x,z) {
  // approximate by sampling plane function used earlier
  const height = Math.sin(x*0.005)*4 + Math.cos(z*0.008)*3 + Math.sin((x+z)*0.003)*2;
  return height;
}

// --- Animals --- simple entity system
const animals = [];
function spawnAnimals() {
  const animalSpecs = [
    { name:'rabbit', modelName:'rabbit.json', count:80, speed:0.8, health: 10 },
    { name:'chicken', modelName:null, count:120, speed:1.0, health: 8 },
    { name:'wolf', modelName:'wolf.json', count:30, speed:1.6, health: 40 },
    { name:'bear', modelName:'bear.json', count:12, speed:1.0, health: 120 },
  ];
  for (const s of animalSpecs) {
    for (let i=0;i<s.count;i++) {
      const x = (Math.random()-0.5)*worldSize;
      const z = (Math.random()-0.5)*worldSize;
      const y = getHeightAt(x,z);
      const obj = s.modelName && models[s.modelName] ? models[s.modelName].clone() : new THREE.Mesh(new THREE.BoxGeometry(1,1,1), new THREE.MeshStandardMaterial({color:0xffaa00}));
      obj.position.set(x,y,z);
      obj.userData = {
        type: 'animal', species: s.name, speed: s.speed, health: s.health, state: 'idle', target: null
      };
      scene.add(obj);
      animals.push(obj);
    }
  }
}

// --- Player setup ---
function setupLocalPlayer() {
  if (!models['human.json']) {
    // placeholder capsule
    const g = new THREE.CapsuleGeometry(0.5, 1.2, 4, 8);
    const m = new THREE.MeshStandardMaterial({ color: 0x6699ff });
    const obj = new THREE.Mesh(g, m);
    obj.position.set(0, 2, 0);
    localPlayer = localPlayer || { id: null, pos: { x:0,y:2,z:0 }, rot:0, inventory:{ wood:0, stone:0 }, quickbar:[] };
    obj.name = 'LocalPlayer';
    scene.add(obj);
    camera.position.set(0,5,15);
    camera.lookAt(obj.position);
    // link main camera to follow player
    // we'll update camera each frame
    localPlayer.obj = obj;
  } else {
    const human = models['human.json'].clone();
    human.position.set(0,2,0);
    scene.add(human);
    localPlayer.obj = human;
  }
}

// --- Remote players ---
function spawnRemotePlayer(info) {
  const mesh = new THREE.Mesh(new THREE.CapsuleGeometry(0.5,1.2,4,8), new THREE.MeshStandardMaterial({color:0xffcc66}));
  mesh.position.set(info.pos.x, info.pos.y, info.pos.z);
  mesh.name = `Remote_${info.id}`;
  scene.add(mesh);
  otherPlayers.set(info.id, { object: mesh, name: info.name });
}
function removeRemotePlayer(id) {
  const p = otherPlayers.get(id);
  if (!p) return;
  scene.remove(p.object);
  otherPlayers.delete(id);
}

// --- Input & movement ---
const keys = {};
let yaw = 0, pitch = 0;
let pointerLocked = false;
let buildingMode = false;
let buildRotation = 0;

function setupInput() {
  window.addEventListener('keydown', (e)=> {
    keys[e.key.toLowerCase()] = true;
    if (e.key === 'i' || e.key === 'I') toggleInventory();
    if (e.key === 'b' || e.key === 'B') toggleBuildMode();
    if (e.key === 'e' || e.key === 'E') interact();
  });
  window.addEventListener('keyup', (e)=> keys[e.key.toLowerCase()] = false);

  // pointer lock for look
  renderer.domElement.addEventListener('click', () => {
    renderer.domElement.requestPointerLock();
  });
  document.addEventListener('pointerlockchange', ()=> {
    pointerLocked = !!document.pointerLockElement;
  });
  document.addEventListener('mousemove', (e) => {
    if (!pointerLocked) return;
    yaw -= e.movementX * 0.002;
    pitch -= e.movementY * 0.002;
    pitch = Math.max(-1.2, Math.min(1.2, pitch));
  });

  // build rotation via wheel
  window.addEventListener('wheel', (e) => {
    if (buildingMode) {
      buildRotation += (e.deltaY > 0 ? 0.1 : -0.1);
    }
  });

  // crafting UI
  document.getElementById('craftAxe').addEventListener('click', ()=> craft('axe', { wood:10 }));
  document.getElementById('craftPick').addEventListener('click', ()=> craft('pickaxe', { stone:10 }));
  document.getElementById('toggleBuild').addEventListener('click', toggleBuildMode);
}

// Toggle inventory UI (simple)
let inventoryOpen = false;
function toggleInventory() {
  inventoryOpen = !inventoryOpen;
  document.getElementById('inventoryPanel').style.display = inventoryOpen ? 'block' : 'none';
}

// Build mode
function toggleBuildMode() {
  buildingMode = !buildingMode;
  buildModeSpan.innerText = buildingMode ? 'ON' : 'OFF';
}

// Interact (gather / open barrel / place building etc)
function interact() {
  // simple raycast from player forward
  const origin = localPlayer.obj.position.clone();
  const forward = new THREE.Vector3(0,0,-1).applyAxisAngle(new THREE.Vector3(0,1,0), yaw).normalize();
  const ray = new THREE.Raycaster(origin, forward, 0, 3);
  const hits = ray.intersectObjects(scene.children, true);
  for (const h of hits) {
    const o = h.object;
    if (o.userData && o.userData.resource) {
      gatherResource(o.userData.resource, 1, o);
      scene.remove(o);
      break;
    } else if (o.userData && o.userData.loot) {
      // open barrel
      hint.innerText = 'You searched the barrel and found 1 scrap metal!';
      addToInventory('scrap', 1);
      scene.remove(o);
      break;
    }
  }
}

function gatherResource(type, amount, obj) {
  addToInventory(type, amount);
  hint.innerText = `Gathered ${amount} ${type}`;
}

function addToInventory(item, amount) {
  localPlayer.inventory[item] = (localPlayer.inventory[item] || 0) + amount;
  updateInventoryUI();
  sendUpdateToServer();
}

function updateInventoryUI() {
  const parts = [];
  for (const [k,v] of Object.entries(localPlayer.inventory || {})) parts.push(`${k}: ${v}`);
  inventoryList.innerText = parts.length ? parts.join('\n') : 'empty';
}

// Crafting function
function craft(item, cost) {
  // check resources
  for (const [k,v] of Object.entries(cost)) {
    if ((localPlayer.inventory[k]||0) < v) {
      hint.innerText = `Not enough ${k} to craft ${item}`;
      return;
    }
  }
  for (const [k,v] of Object.entries(cost)) localPlayer.inventory[k] -= v;
  localPlayer.inventory[item] = (localPlayer.inventory[item]||0) + 1;
  hint.innerText = `Crafted ${item}`;
  updateInventoryUI();
  sendActionToServer({ action:'craft', item, amount:1 });
  sendUpdateToServer();
}

// Send update to server
function sendUpdateToServer() {
  if (!socket || socket.readyState !== 1) return;
  const payload = { type: 'update', sessionId, pos: localPlayer.obj.position, rot: yaw, inventory: localPlayer.inventory };
  // can't send Vector3 directly; pack numbers
  payload.pos = { x: localPlayer.obj.position.x, y: localPlayer.obj.position.y, z: localPlayer.obj.position.z };
  socket.send(JSON.stringify(payload));
}
function sendActionToServer(action) {
  if (!socket || socket.readyState !== 1) return;
  socket.send(JSON.stringify({ type: 'action', ...action }));
}

// --- Animation loop ---
function animate() {
  requestAnimationFrame(animate);
  const dt = clock.getDelta();

  // update animals
  updateAnimals(dt);

  // player movement
  updatePlayer(dt);

  // mixers (for animated models)
  mixers.forEach(m => m.update(dt));

  renderer.render(scene, camera);
  // update water if present
  scene.traverse(obj => {
    if (obj.material && obj.material.isShaderMaterial && obj.userData && obj.userData.isWater) {
      // placeholder; Water object internally updates via its own material if properly used
    }
  });
}

// Simple animal AI: wander, chase player if close, drown if in water (y < -1.5)
function updateAnimals(dt) {
  for (const a of animals) {
    if (a.userData.health <= 0) continue;
    const p = localPlayer.obj.position;
    const dist = a.position.distanceTo(p);
    if (dist < 12 && (a.userData.species === 'wolf' || a.userData.species === 'bear')) {
      // chase
      const dir = p.clone().sub(a.position).normalize();
      a.position.add(dir.multiplyScalar(a.userData.speed * dt * 2.0));
      a.lookAt(p.x, p.y, p.z);
      a.userData.state = 'chase';
      if (a.position.y < -1 && a.userData.species !== 'rabbit' && a.userData.species !== 'chicken') {
        // drown in water
        a.userData.health = 0;
        scene.remove(a);
      }
      // if close to player -> attack (deal damage omitted for brevity)
    } else {
      // wander
      if (!a.userData.wanderTarget || a.position.distanceTo(a.userData.wanderTarget) < 2) {
        a.userData.wanderTarget = new THREE.Vector3((Math.random()-0.5)*worldSize, 0, (Math.random()-0.5)*worldSize);
        a.userData.wanderTarget.y = getHeightAt(a.userData.wanderTarget.x, a.userData.wanderTarget.z);
      }
      const dir = a.userData.wanderTarget.clone().sub(a.position).normalize();
      a.position.add(dir.multiplyScalar(a.userData.speed * dt));
      a.lookAt(a.userData.wanderTarget);
    }
  }
}

// --- Player update: movement and collision avoidance ---
function updatePlayer(dt) {
  if (!localPlayer || !localPlayer.obj) return;
  const speed = keys['shift'] ? 8 : 4;
  const forward = new THREE.Vector3(0,0,-1).applyAxisAngle(new THREE.Vector3(0,1,0), yaw).normalize();
  const right = new THREE.Vector3(1,0,0).applyAxisAngle(new THREE.Vector3(0,1,0), yaw).normalize();

  let move = new THREE.Vector3();
  if (keys['w']) move.add(forward);
  if (keys['s']) move.sub(forward);
  if (keys['a']) move.sub(right);
  if (keys['d']) move.add(right);
  if (move.length() > 0) {
    move.normalize();
    // apply to player position
    const pos = localPlayer.obj.position;
    pos.add(move.multiplyScalar(speed * dt));
    // adjust y to terrain
    pos.y = getHeightAt(pos.x, pos.z) + 1.0;
    // simple collision: avoid too deep water
    if (pos.y < -1.8) pos.y = -1.8;
    // rotate to face movement
    if (move.length() > 0.01) {
      localPlayer.obj.rotation.y = Math.atan2(move.x, move.z);
    }
    // camera follow
    camera.position.lerp(new THREE.Vector3(pos.x + Math.sin(localPlayer.obj.rotation.y)*15, pos.y + 8, pos.z + Math.cos(localPlayer.obj.rotation.y)*15), 0.1);
    camera.lookAt(pos.x, pos.y + 2, pos.z);
    sendUpdateToServer();
  } else {
    // still, keep camera behind player
    const pos = localPlayer.obj.position;
    camera.position.lerp(new THREE.Vector3(pos.x + Math.sin(localPlayer.obj.rotation.y)*15, pos.y + 8, pos.z + Math.cos(localPlayer.obj.rotation.y)*15), 0.05);
    camera.lookAt(pos.x, pos.y + 2, pos.z);
  }
}
