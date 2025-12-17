// server.js
const express = require('express');
const { WebSocketServer } = require('ws');
const http = require('http');
const fs = require('fs');
const path = require('path');
const { v4: uuidv4 } = require('uuid');

const PORT = 3000;
const app = express();
const server = http.createServer(app);

// Serve static files from public
app.use(express.static(path.join(__dirname, 'public')));

// Simple persistent storage of player states
const PLAYERS_FILE = path.join(__dirname, 'players.json');
let savedPlayers = {};
try {
  if (fs.existsSync(PLAYERS_FILE)) {
    savedPlayers = JSON.parse(fs.readFileSync(PLAYERS_FILE));
  }
} catch (e) {
  console.error('Failed to load players.json', e);
}

// In-memory map of connections -> player
const wss = new WebSocketServer({ server });
const clients = new Map();

function saveAll() {
  // Save only minimal needed persistent state
  const out = {};
  for (const [id, data] of Object.entries(savedPlayers)) out[id] = data;
  try {
    fs.writeFileSync(PLAYERS_FILE, JSON.stringify(out, null, 2));
  } catch (e) {
    console.error('Failed to save players.json', e);
  }
}

setInterval(saveAll, 30_000);

// Create a fresh session ID and return previous saved data if exists
function createPlayer(sessionId) {
  const id = sessionId || uuidv4();
  const base = savedPlayers[id] || {
    id,
    name: `Player_${id.substring(0,4)}`,
    pos: { x: 0, y: 5, z: 0 },
    rot: 0,
    inventory: {},
    quickbar: [],
    buildings: []
  };
  savedPlayers[id] = base;
  return base;
}

wss.on('connection', (ws, req) => {
  let player = null;
  let playerId = null;

  ws.on('message', (msg) => {
    try {
      const data = JSON.parse(msg.toString());
      if (data.type === 'join') {
        // data: { type: 'join', sessionId?, name? }
        player = createPlayer(data.sessionId);
        if (data.name) player.name = data.name;
        playerId = player.id;
        clients.set(ws, playerId);

        // send join ack with world seed + player data + other players
        const otherPlayers = Object.values(savedPlayers).map(p => ({
          id: p.id, name: p.name, pos: p.pos, rot: p.rot
        }));
        ws.send(JSON.stringify({ type: 'joined', player, others: otherPlayers }));
        // broadcast new player to all
        broadcast({ type: 'playerJoined', id: player.id, name: player.name, pos: player.pos, rot: player.rot });
      } else if (data.type === 'update') {
        // data: { type:'update', pos:{}, rot, state, inventory }
        if (!player) return;
        player.pos = data.pos || player.pos;
        player.rot = data.rot ?? player.rot;
        if (data.inventory) player.inventory = data.inventory;
        if (data.quickbar) player.quickbar = data.quickbar;
        // broadcast to other clients player position update
        broadcastExcept(ws, { type: 'playerUpdate', id: player.id, pos: player.pos, rot: player.rot });
      } else if (data.type === 'action') {
        // actions like gather, craft, build -- persist if needed then broadcast
        handleAction(player, data);
      }
    } catch (e) {
      console.error('Invalid message', e);
    }
  });

  ws.on('close', () => {
    if (playerId) {
      // Save to disk immediately
      saveAll();
      clients.delete(ws);
      broadcast({ type: 'playerLeft', id: playerId });
    }
  });

  ws.on('error', (err) => console.error('WS error', err));
});

function broadcast(message) {
  const s = JSON.stringify(message);
  for (const client of wss.clients) {
    if (client.readyState === 1) client.send(s);
  }
}

function broadcastExcept(senderWs, message) {
  const s = JSON.stringify(message);
  for (const client of wss.clients) {
    if (client !== senderWs && client.readyState === 1) client.send(s);
  }
}

function handleAction(player, data) {
  // Data contains: action: 'gather'|'craft'|'placeBuilding', etc.
  if (!player) return;
  if (data.action === 'gather') {
    // e.g. { action:'gather', resource:'wood', amount:5 }
    player.inventory[data.resource] = (player.inventory[data.resource] || 0) + (data.amount || 1);
  } else if (data.action === 'craft') {
    // apply simple crafting: reduce resources, add crafted item
    // for now assume client validated resource availability; server will trust but persist
    player.inventory[data.item] = (player.inventory[data.item] || 0) + (data.amount || 1);
  } else if (data.action === 'placeBuilding') {
    player.buildings = player.buildings || [];
    player.buildings.push(data.building);
  }
  // persist immediate
  savedPlayers[player.id] = player;
  broadcast({ type: 'playerSaved', id: player.id, inventory: player.inventory, buildings: player.buildings });
}

server.listen(PORT, () => {
  console.log(`Server running on http://localhost:${PORT}`);
});
