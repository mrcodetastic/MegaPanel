const state = {
  config: null,
  outputs: [],
  status: null,
  previewTimer: null,
};

const $ = (id) => document.getElementById(id);

async function getJson(path) {
  const response = await fetch(path, { cache: "no-store" });
  if (!response.ok) throw new Error(`${path}: ${response.status}`);
  return response.json();
}

async function postJson(path, body) {
  const response = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body ?? {}),
  });
  const text = await response.text();
  let data;
  try { data = text ? JSON.parse(text) : {}; } catch { data = { error: text }; }
  if (!response.ok) throw new Error(data.error || `${path}: ${response.status}`);
  return data;
}

function setMessage(id, text, ok = false) {
  const el = $(id);
  if (!el) return;
  el.textContent = text || "";
  el.className = `message ${ok ? "ok" : text ? "bad" : ""}`;
}

function badge(id, text, kind = "") {
  const el = $(id);
  if (!el) return;
  el.textContent = text;
  el.className = `badge ${kind}`;
}

// Built from char codes so the entity text can never be mangled by an editor
// or by a write that decodes HTML entities.
const AMP = String.fromCharCode(38);

function escapeHtml(text) {
  return String(text ?? "")
    .replaceAll(AMP, AMP + "amp;")
    .replaceAll("<", AMP + "lt;")
    .replaceAll(">", AMP + "gt;")
    .replaceAll('"', AMP + "quot;")
    .replaceAll("'", AMP + "#39;");
}

function fillSelect(select, includeNone) {
  const previous = select.value;
  select.innerHTML = "";
  if (includeNone) {
    const option = document.createElement("option");
    option.value = "";
    option.textContent = "None";
    select.appendChild(option);
  }
  const groups = new Map();
  for (const output of state.outputs) {
    if (!groups.has(output.category)) groups.set(output.category, []);
    groups.get(output.category).push(output);
  }
  for (const [category, outputs] of groups) {
    const group = document.createElement("optgroup");
    group.label = category;
    for (const output of outputs) {
      const option = document.createElement("option");
      option.value = output.id;
      option.textContent = `${output.name} (${output.id})`;
      group.appendChild(option);
    }
    select.appendChild(group);
  }
  if (previous && [...select.options].some((option) => option.value === previous)) {
    select.value = previous;
  }
}

function outputConfig(outputId) {
  if (!state.config) state.config = {};
  if (!state.config.outputs) state.config.outputs = {};
  if (!state.config.outputs[outputId]) state.config.outputs[outputId] = {};
  return state.config.outputs[outputId];
}

function renderField(output, key, spec) {
  const values = outputConfig(output.id);
  const value = values[key] ?? spec.default ?? "";
  const attributes = `data-output="${escapeHtml(output.id)}" data-key="${escapeHtml(key)}"`;

  if (spec.type === "bool") {
    return `<label class="check"><input type="checkbox" ${attributes} ${value ? "checked" : ""}> ${escapeHtml(key)}</label>`;
  }
  if (spec.type === "color") {
    return `<label>${escapeHtml(key)}</label><input type="color" ${attributes} value="${escapeHtml(value || "#FFFFFF")}">`;
  }
  if (spec.type === "select") {
    const options = (spec.options || []).map((option) => {
      const selected = String(option) === String(value) ? " selected" : "";
      return `<option value="${escapeHtml(option)}"${selected}>${escapeHtml(option)}</option>`;
    }).join("");
    return `<label>${escapeHtml(key)}</label><select ${attributes}>${options}</select>`;
  }
  if (spec.type === "range" || spec.type === "number") {
    const min = spec.min ?? "";
    const max = spec.max ?? "";
    const step = spec.step ?? 1;
    return `<label>${escapeHtml(key)}</label><input type="number" ${attributes} min="${min}" max="${max}" step="${step}" value="${escapeHtml(value)}">`;
  }
  return `<label>${escapeHtml(key)}</label><input type="text" ${attributes} value="${escapeHtml(value)}">`;
}

function renderOutputs() {
  const list = $("outputList");
  list.innerHTML = "";
  for (const output of state.outputs) {
    const card = document.createElement("div");
    card.className = "output";
    const schema = output.schema || {};
    const fields = Object.entries(schema)
      .map(([key, spec]) => renderField(output, key, spec))
      .join("");
    card.innerHTML = `
      <h3>${escapeHtml(output.name)}</h3>
      <p><code>${escapeHtml(output.id)}</code></p>
      <p>${escapeHtml(output.author || "")}</p>
      <div class="tags">
        <span class="tag">${escapeHtml(output.category)}</span>
        <span class="tag">${output.defaultFps} fps</span>
        <span class="tag">${output.designWidth ? `${output.designWidth}x${output.designHeight}` : "any size"}</span>
      </div>
      <div class="output-form">${fields || "<p class=\"muted\">No settings.</p>"}</div>
      <div class="row">
        <button class="button secondary" data-foreground="${escapeHtml(output.id)}">Use foreground</button>
        <button class="button secondary" data-background="${escapeHtml(output.id)}">Use background</button>
        <button class="button" data-save-output="${escapeHtml(output.id)}">Save output</button>
      </div>
    `;
    list.appendChild(card);
  }

  list.querySelectorAll("[data-foreground]").forEach((button) => {
    button.addEventListener("click", () => {
      $("foregroundSelect").value = button.dataset.foreground;
      applyScene();
    });
  });
  list.querySelectorAll("[data-background]").forEach((button) => {
    button.addEventListener("click", () => {
      $("backgroundSelect").value = button.dataset.background;
      applyScene();
    });
  });
  list.querySelectorAll("[data-save-output]").forEach((button) => {
    button.addEventListener("click", () => saveOutputConfig(button.dataset.saveOutput, button));
  });
}

function collectOutputFields(outputId, container) {
  const values = outputConfig(outputId);
  container.querySelectorAll(`[data-output="${CSS.escape(outputId)}"]`).forEach((element) => {
    const key = element.dataset.key;
    if (element.type === "checkbox") values[key] = element.checked;
    else if (element.type === "number" || element.type === "range") values[key] = Number(element.value);
    else values[key] = element.value;
  });
}

async function saveOutputConfig(outputId, button) {
  try {
    const card = button.closest(".output");
    collectOutputFields(outputId, card);
    await saveConfigObject();
    setMessage("configMessage", `Saved ${outputId}`, true);
  } catch (error) {
    setMessage("configMessage", error.message);
  }
}

async function saveConfigObject() {
  $("configJson").value = JSON.stringify(state.config, null, 2);
  await postJson("/api/config", state.config);
  await loadConfig();
  await loadStatus();
}

const PLAYLIST_ROWS = 4;

function renderPlaylistRows() {
  const host = $("playlistRows");
  if (!host) return;
  const playlist = state.config?.playlist || {};
  const entries = playlist.entries || [];
  host.innerHTML = "";
  for (let i = 0; i < PLAYLIST_ROWS; i += 1) {
    const entry = entries[i] || {};
    const row = document.createElement("div");
    row.className = "playlist-row";
    row.innerHTML = `
      <select data-pl-fg="${i}"></select>
      <select data-pl-bg="${i}"></select>
      <input type="number" min="3" max="3600" data-pl-dwell="${i}" value="${entry.dwellSec ?? 20}">
    `;
    host.appendChild(row);
    const foreground = row.querySelector(`[data-pl-fg="${i}"]`);
    const background = row.querySelector(`[data-pl-bg="${i}"]`);
    fillSelect(foreground, false);
    fillSelect(background, true);
    foreground.value = entry.foregroundId || "";
    background.value = entry.backgroundId || "";
  }
}

const DATA_ROWS = 4;

function renderDataRows() {
  const host = $("dataRows");
  if (!host) return;
  const sources = state.config?.data?.sources || [];
  host.innerHTML = "";
  for (let i = 0; i < DATA_ROWS; i += 1) {
    const source = sources[i] || {};
    const row = document.createElement("div");
    row.className = "data-row";
    row.innerHTML = `
      <label class="check"><input type="checkbox" data-data-enabled="${i}" ${source.enabled === false ? "" : "checked"}></label>
      <input data-data-label="${i}" placeholder="label" value="${escapeHtml(source.label || "")}">
      <input data-data-url="${i}" placeholder="https://api.example.com/price" value="${escapeHtml(source.url || "")}">
      <input data-data-name="${i}" placeholder="name field" value="${escapeHtml(source.nameField || "")}">
      <input data-data-price="${i}" placeholder="price field" value="${escapeHtml(source.priceField || "")}">
      <input data-data-change="${i}" placeholder="change field" value="${escapeHtml(source.changeField || "")}">
      <input data-data-percent="${i}" placeholder="percent field" value="${escapeHtml(source.changePercentField || "")}">
      <input type="number" min="10" max="3600" data-data-refresh="${i}" value="${source.refreshSec ?? 120}">
    `;
    host.appendChild(row);
  }
}

function applyNewsControls() {
  const news = state.config?.news || {};
  $("newsEnabled").checked = Boolean(news.enabled);
  $("newsUrl").value = news.url || "";
  $("newsItemsField").value = news.itemsField || "";
  $("newsTitleField").value = news.titleField || "title";
  $("newsLimit").value = news.limit ?? 6;
  $("newsRefresh").value = news.refreshMin ?? 10;
}

async function saveNews() {
  try {
    state.config.news = {
      enabled: $("newsEnabled").checked,
      url: ($("newsUrl").value || "").trim(),
      itemsField: ($("newsItemsField").value || "").trim(),
      titleField: ($("newsTitleField").value || "").trim(),
      limit: Number($("newsLimit").value || 6),
      refreshMin: Number($("newsRefresh").value || 10),
    };
    await saveConfigObject();
    setMessage("newsMessage", "News settings saved", true);
    await loadNews();
  } catch (error) {
    setMessage("newsMessage", error.message);
  }
}

async function loadNews() {
  const status = $("newsStatus");
  if (!status) return;
  try {
    const data = await getJson("/api/news");
    if (!data.available) {
      status.textContent = "service off";
      return;
    }
    if (!data.configured) {
      status.textContent = "set an endpoint";
      return;
    }
    if (!data.valid) {
      status.textContent = data.fetching ? "fetching" : (data.error || "no data");
      return;
    }
    const first = data.items && data.items.length ? ` — ${data.items[0]}` : "";
    status.textContent = `${data.count} headlines${data.stale ? " (stale)" : ""}${first}`;
  } catch {
    status.textContent = "news unavailable";
  }
}

async function refreshNews() {
  try {
    await postJson("/api/news", {});
    setMessage("newsMessage", "Refresh requested", true);
    setTimeout(loadNews, 3000);
  } catch (error) {
    setMessage("newsMessage", error.message);
  }
}

function applyDataControls() {
  const data = state.config?.data || {};
  $("dataEnabled").checked = Boolean(data.enabled);
  renderDataRows();
}

async function saveData() {
  try {
    const sources = [];
    for (let i = 0; i < DATA_ROWS; i += 1) {
      const fields = {
        enabled: document.querySelector(`[data-data-enabled="${i}"]`),
        label: document.querySelector(`[data-data-label="${i}"]`),
        url: document.querySelector(`[data-data-url="${i}"]`),
        nameField: document.querySelector(`[data-data-name="${i}"]`),
        priceField: document.querySelector(`[data-data-price="${i}"]`),
        changeField: document.querySelector(`[data-data-change="${i}"]`),
        changePercentField: document.querySelector(`[data-data-percent="${i}"]`),
        refreshSec: document.querySelector(`[data-data-refresh="${i}"]`),
      };
      const url = (fields.url?.value || "").trim();
      if (!url) continue;
      sources.push({
        enabled: Boolean(fields.enabled?.checked),
        label: (fields.label?.value || "").trim(),
        url,
        nameField: (fields.nameField?.value || "").trim(),
        priceField: (fields.priceField?.value || "").trim(),
        changeField: (fields.changeField?.value || "").trim(),
        changePercentField: (fields.changePercentField?.value || "").trim(),
        refreshSec: Number(fields.refreshSec?.value || 120),
      });
    }
    state.config.data = { enabled: $("dataEnabled").checked, sources };
    await saveConfigObject();
    setMessage("dataMessage", `Saved ${sources.length} sources`, true);
    await loadData();
  } catch (error) {
    setMessage("dataMessage", error.message);
  }
}

async function loadData() {
  const status = $("dataStatus");
  if (!status) return;
  try {
    const data = await getJson("/api/data");
    if (!data.available) {
      status.textContent = "service off";
      return;
    }
    const parts = (data.sources || []).map((source) => {
      const label = source.label || source.name || `source ${source.index}`;
      if (!source.valid) return `${label}: ${source.error || "no data"}`;
      const movement = typeof source.changePercent === "number" ? source.changePercent : 0;
      const sign = movement > 0 ? "+" : "";
      return `${label}: ${source.price} (${sign}${movement.toFixed(2)}%)${source.stale ? " stale" : ""}`;
    });
    status.textContent = parts.length
      ? parts.join(" · ")
      : (data.enabled ? "no sources configured" : "disabled");
  } catch {
    status.textContent = "data unavailable";
  }
}

async function refreshData() {
  try {
    await postJson("/api/data", {});
    setMessage("dataMessage", "Refresh requested", true);
    setTimeout(loadData, 3000);
  } catch (error) {
    setMessage("dataMessage", error.message);
  }
}

function applyPlaylistControls() {
  const playlist = state.config?.playlist || {};
  $("playlistEnabled").checked = Boolean(playlist.enabled);
  $("playlistIdle").value = playlist.idleStartSec ?? 30;
  $("playlistTransition").value = String(playlist.transition ?? 1);
  renderPlaylistRows();
  $("playlistNow").textContent = playlist.count ? `${playlist.count} entries` : "no entries";
}

async function savePlaylist() {
  try {
    const entries = [];
    for (let i = 0; i < PLAYLIST_ROWS; i += 1) {
      const foreground = document.querySelector(`[data-pl-fg="${i}"]`);
      const background = document.querySelector(`[data-pl-bg="${i}"]`);
      const dwell = document.querySelector(`[data-pl-dwell="${i}"]`);
      if (!foreground || !foreground.value) continue;
      entries.push({
        foregroundId: foreground.value,
        backgroundId: background ? background.value : "",
        dwellSec: Number(dwell ? dwell.value : 20) || 20,
      });
    }
    state.config.playlist = {
      enabled: $("playlistEnabled").checked,
      idleStartSec: Number($("playlistIdle").value || 0),
      transition: Number($("playlistTransition").value || 0),
      entries,
    };
    await saveConfigObject();
    setMessage("playlistMessage", `Saved ${entries.length} entries`, true);
  } catch (error) {
    setMessage("playlistMessage", error.message);
  }
}

function applyWeatherControls() {
  const weather = state.config?.weather || {};
  $("wxEnabled").checked = Boolean(weather.enabled);
  $("wxLat").value = weather.latitude ?? 0;
  $("wxLon").value = weather.longitude ?? 0;
  $("wxRefresh").value = weather.refreshMin ?? 15;
  $("wxFahrenheit").checked = Boolean(weather.fahrenheit);
}

async function saveWeather() {
  try {
    state.config.weather = {
      enabled: $("wxEnabled").checked,
      latitude: Number($("wxLat").value || 0),
      longitude: Number($("wxLon").value || 0),
      refreshMin: Number($("wxRefresh").value || 15),
      fahrenheit: $("wxFahrenheit").checked,
    };
    await saveConfigObject();
    setMessage("weatherMessage", "Weather settings saved", true);
    await loadWeather();
  } catch (error) {
    setMessage("weatherMessage", error.message);
  }
}

async function loadWeather() {
  const status = $("weatherStatus");
  if (!status) return;
  try {
    const data = await getJson("/api/weather");
    const bits = [];
    if (!data.available) {
      bits.push("service off");
    } else {
      bits.push(data.enabled ? "enabled" : "disabled");
      if (!data.configured) bits.push("set location");
      else if (data.valid) {
        bits.push(`${data.temp}° ${data.icon || "?"}`);
        if (data.stale) bits.push("stale");
      } else {
        bits.push(data.fetching ? "fetching" : "no data");
      }
      if (data.error) bits.push(data.error);
    }
    status.textContent = bits.join(" · ");
  } catch {
    status.textContent = "weather unavailable";
  }
}

async function refreshWeather() {
  try {
    await postJson("/api/weather", {});
    setMessage("weatherMessage", "Refresh requested", true);
    setTimeout(loadWeather, 3000);
  } catch (error) {
    setMessage("weatherMessage", error.message);
  }
}

function applySceneControls() {
  const scene = state.config?.scene || {};
  $("foregroundSelect").value = scene.foregroundId || "";
  $("backgroundSelect").value = scene.backgroundId || "";
  $("foregroundAlpha").value = scene.foregroundAlpha ?? 255;
  $("backgroundAlpha").value = scene.backgroundAlpha ?? 255;
  $("foregroundAlphaOut").textContent = $("foregroundAlpha").value;
  $("backgroundAlphaOut").textContent = $("backgroundAlpha").value;
  $("statusCorners").checked = Boolean(scene.statusCorners);
}

async function loadConfig() {
  state.config = await getJson("/api/config");
  $("configJson").value = JSON.stringify(state.config, null, 2);
  applySceneControls();
  applyPlaylistControls();
  applyWeatherControls();
  applyDataControls();
  applyNewsControls();
  renderOutputs();
  const brightness = state.config.brightness?.manual ?? 128;
  $("brightness").value = brightness;
  $("brightnessOut").textContent = brightness;
}

async function loadOutputs() {
  state.outputs = await getJson("/api/outputs");
  fillSelect($("foregroundSelect"), false);
  fillSelect($("backgroundSelect"), true);
  applySceneControls();
  renderOutputs();
}

async function loadStatus() {
  state.status = await getJson("/api/status");
  const status = state.status;
  const wifi = status.wifi || {};
  badge("wifiBadge", wifi.connected ? `wifi ${wifi.rssi} dBm` : "wifi offline",
        wifi.connected ? "ok" : "bad");
  const panel = status.panel || {};
  badge("panelBadge", panel.ready ? `${panel.width}x${panel.height}` : "panel offline",
        panel.ready ? "ok" : "bad");
  badge("fpsBadge", `${status.diagnostics?.fps ?? 0}/${status.scene?.targetFps ?? 0} fps`, "ok");
  $("sceneLabel").textContent = `${status.scene?.foregroundId || "--"} + ${status.scene?.backgroundId || "none"}`;
  $("uptime").textContent = `uptime ${Math.round((status.diagnostics?.uptimeMs ?? 0) / 1000)} s`;

  const sensors = [
    ["PIR present", status.presence?.present ? "yes" : "no"],
    ["PIR motion", status.presence?.motion ? "yes" : "no"],
    ["LDR", status.light?.valid ? `${status.light.percent}%` : "n/a"],
    ["LDR raw", status.light?.raw ?? "--"],
    ["Brightness", `${status.brightness?.target ?? "--"} (${status.brightness?.reason ?? "--"})`],
    ["Heap", `${Math.round((status.diagnostics?.freeHeap ?? 0) / 1024)} KB`],
    ["PSRAM", `${Math.round((status.diagnostics?.freePsram ?? 0) / 1024)} KB`],
    ["Render", `${status.scene?.renderUs ?? 0} us`],
  ];
  $("sensorGrid").innerHTML = sensors.map(([key, value]) => `
    <div class="sensor"><div class="k">${escapeHtml(key)}</div><div class="v">${escapeHtml(value)}</div></div>
  `).join("");
}

async function applyScene() {
  try {
    const body = {
      foregroundId: $("foregroundSelect").value,
      backgroundId: $("backgroundSelect").value,
      foregroundAlpha: Number($("foregroundAlpha").value),
      backgroundAlpha: Number($("backgroundAlpha").value),
      statusCorners: $("statusCorners").checked,
    };
    await postJson("/api/scene", body);
    setMessage("sceneMessage", "Scene applied", true);
    await loadStatus();
    await refreshPreview();
  } catch (error) {
    setMessage("sceneMessage", error.message);
  }
}

async function saveConfig() {
  try {
    state.config = JSON.parse($("configJson").value);
    await saveConfigObject();
    setMessage("configMessage", "Configuration saved", true);
  } catch (error) {
    setMessage("configMessage", error.message);
  }
}

async function setBrightness(on = true) {
  try {
    const value = Number($("brightness").value);
    await postJson("/api/display", { on, brightness: value });
    setMessage("displayMessage", on ? "Brightness updated" : "Display off", true);
    await loadStatus();
  } catch (error) {
    setMessage("displayMessage", error.message);
  }
}

async function sendNotification() {
  try {
    await postJson("/api/notify", {
      text: $("notifyText").value,
      icon: $("notifyIcon").value,
      position: $("notifyPosition").value,
      color: "#FFFFFF",
      durationMs: 5000,
    });
    setMessage("notifyMessage", "Notification sent", true);
  } catch (error) {
    setMessage("notifyMessage", error.message);
  }
}

async function refreshPreview() {
  try {
    const response = await fetch("/api/preview", { cache: "no-store" });
    if (!response.ok) throw new Error("preview unavailable");
    const width = Number(response.headers.get("X-Width") || 0);
    const height = Number(response.headers.get("X-Height") || 0);
    const buffer = await response.arrayBuffer();
    if (!width || !height || buffer.byteLength < width * height * 3) return;

    const canvas = $("preview");
    canvas.width = width;
    canvas.height = height;
    const context = canvas.getContext("2d");
    const image = context.createImageData(width, height);
    const source = new Uint8Array(buffer);
    for (let i = 0, p = 0; i < width * height; i += 1, p += 3) {
      const q = i * 4;
      image.data[q] = source[p];
      image.data[q + 1] = source[p + 1];
      image.data[q + 2] = source[p + 2];
      image.data[q + 3] = 255;
    }
    context.putImageData(image, 0, 0);
    $("previewMeta").textContent = `${width}x${height} @ ${new Date().toLocaleTimeString()}`;
  } catch {
    $("previewMeta").textContent = "preview unavailable";
  }
}

function schedulePreview() {
  if (state.previewTimer) clearInterval(state.previewTimer);
  state.previewTimer = setInterval(() => {
    if (!document.hidden) refreshPreview();
  }, 750);
}

async function boot() {
  try {
    await loadOutputs();
    await loadConfig();
    await loadStatus();
    await refreshPreview();
    await loadWeather();
    await loadData();
    await loadNews();
    schedulePreview();
    setInterval(loadWeather, 30000);
    setInterval(loadData, 30000);
    setInterval(loadNews, 60000);
  } catch (error) {
    setMessage("configMessage", `Boot failed: ${error.message}`);
  }
}

$("applyScene").addEventListener("click", applyScene);
$("reloadScene").addEventListener("click", loadConfig);
$("foregroundAlpha").addEventListener("input", () => {
  $("foregroundAlphaOut").textContent = $("foregroundAlpha").value;
});
$("backgroundAlpha").addEventListener("input", () => {
  $("backgroundAlphaOut").textContent = $("backgroundAlpha").value;
});
$("brightness").addEventListener("input", () => {
  $("brightnessOut").textContent = $("brightness").value;
});
$("applyBrightness").addEventListener("click", () => setBrightness(true));
$("displayOff").addEventListener("click", () => setBrightness(false));
$("displayOn").addEventListener("click", () => setBrightness(true));
$("sendNotify").addEventListener("click", sendNotification);
$("savePlaylist").addEventListener("click", savePlaylist);
$("saveWeather").addEventListener("click", saveWeather);
$("refreshWeather").addEventListener("click", refreshWeather);
$("saveData").addEventListener("click", saveData);
$("refreshData").addEventListener("click", refreshData);
$("saveNews").addEventListener("click", saveNews);
$("refreshNews").addEventListener("click", refreshNews);
$("saveConfig").addEventListener("click", saveConfig);
$("reloadConfig").addEventListener("click", loadConfig);
$("refreshPreview").addEventListener("click", refreshPreview);
$("reboot").addEventListener("click", async () => {
  if (!confirm("Reboot the panel?")) return;
  try { await postJson("/api/reboot", {}); } catch { /* device reboots */ }
  setMessage("configMessage", "Rebooting...", true);
});

boot();
