"use strict";
const message = document.getElementById("message");
async function refresh() {
  try {
    const response = await fetch("/status.json", {cache: "no-store"});
    if (!response.ok) throw new Error("Status unavailable");
    const data = await response.json();
    const status = document.getElementById("status");
    status.replaceChildren();
    for (const [key, value] of Object.entries(data)) {
      const label = document.createElement("dt");
      const text = document.createElement("dd");
      label.textContent = key; text.textContent = String(value);
      status.append(label, text);
    }
    const queries = document.getElementById("queries");
    queries.replaceChildren();
    if (data.query_logging) {
      const r = await fetch("/querylog.json", {cache: "no-store"});
      if (!r.ok) throw new Error("Query log unavailable");
      const entries = await r.json();
      for (const entry of entries.slice(-100).reverse()) {
        const item = document.createElement("li");
        item.textContent = (entry.blocked ? "Blocked: " : "Allowed: ") + String(entry.domain) + " (type " + String(entry.type) + ")";
        queries.append(item);
      }
    } else {
      const item = document.createElement("li"); item.textContent = "Query logging is disabled."; queries.append(item);
    }
    message.textContent = "Read-only status";
  } catch (error) { message.textContent = "Cannot load diagnostics. Check the LAN connection."; }
  setTimeout(refresh, 10000); // never overlap requests
}
refresh();
