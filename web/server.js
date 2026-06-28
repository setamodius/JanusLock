const express = require("express");
const mqtt = require("mqtt");
const path = require("path");

const app = express();
app.use(express.json());

const MQTT_URL  = process.env.MQTT_URL  || "";
const MQTT_USER = process.env.MQTT_USER || "";
const MQTT_PASS = process.env.MQTT_PASS || "";
const PORT      = process.env.PORT      || 3000;

const brokerState = { connected: false };
const prefixState = {};

function getPrefix(raw) {
  return raw.trim().replace(/^\/+|\/+$/g, "");
}

function stateFor(prefix) {
  if (!prefixState[prefix]) {
    prefixState[prefix] = { device: "unknown", door: { 1: "unavailable", 2: "unavailable" } };
  }
  return prefixState[prefix];
}

let mqttClient = null;

function connectMqtt() {
  if (!MQTT_URL) {
    console.warn("MQTT_URL not set — REST API will run but MQTT is disabled.");
    return;
  }

  const options = { clientId: "janus-api-" + Math.random().toString(16).slice(2), clean: true, reconnectPeriod: 3000 };
  if (MQTT_USER) { options.username = MQTT_USER; options.password = MQTT_PASS; }

  mqttClient = mqtt.connect(MQTT_URL, options);

  mqttClient.on("connect", () => {
    brokerState.connected = true;
    console.log(`MQTT connected: ${MQTT_URL}`);
    mqttClient.subscribe(["+/device/status", "+/door1/status", "+/door2/status"]);
  });

  mqttClient.on("reconnect", () => { brokerState.connected = false; });
  mqttClient.on("close",     () => { brokerState.connected = false; });
  mqttClient.on("error",     (err) => console.error("MQTT error:", err.message));

  mqttClient.on("message", (t, payload) => {
    const msg = payload.toString();
    const parts = t.split("/");
    const prefix = parts[0];
    const rest   = parts.slice(1).join("/");
    const s = stateFor(prefix);
    if (rest === "device/status") s.device  = msg;
    if (rest === "door1/status")  s.door[1] = msg;
    if (rest === "door2/status")  s.door[2] = msg;
  });
}

// --- REST API ---

app.get("/api/:prefix/status", (req, res) => {
  const prefix = getPrefix(req.params.prefix);
  const s = stateFor(prefix);
  res.json({
    broker: brokerState.connected ? "online" : "offline",
    device: s.device,
    doors: { 1: s.door[1], 2: s.door[2] }
  });
});

app.post("/api/:prefix/door/:id/open", (req, res) => {
  const prefix = getPrefix(req.params.prefix);
  const id     = parseInt(req.params.id, 10);

  if (id !== 1 && id !== 2) {
    return res.status(400).json({ error: "Invalid door id. Use 1 or 2." });
  }

  if (!mqttClient || !mqttClient.connected) {
    return res.status(503).json({ error: "MQTT broker not connected." });
  }

  mqttClient.publish(`${prefix}/door${id}/command`, "open", { qos: 0, retain: false }, (err) => {
    if (err) return res.status(500).json({ error: err.message });
    res.json({ ok: true, prefix, door: id, command: "open" });
  });
});

// --- Static ---

app.get("/", (req, res) => {
  res.sendFile(path.join(__dirname, "index.html"));
});

app.listen(PORT, () => {
  console.log(`Janus API listening on port ${PORT}`);
  connectMqtt();
});
