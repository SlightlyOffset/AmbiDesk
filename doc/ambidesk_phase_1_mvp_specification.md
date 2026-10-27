# Project Specification: AmbiDesk (Phase 1 MVP)
**Ambient Workspace Weather & Forecast Companion**

---

## 1. Executive Summary & Design Rationale

Modern desktop workstations are saturated with screens, tabs, and notifications. While checking the weather on a computer or smartphone is trivial, it demands intentional screen interaction, context switching, and screen real estate.

**AmbiDesk** explores *glanceable ambient computing*—a dedicated, physical desktop companion that communicates real-time environmental context at a glance without interrupting user workflow. 

### Why an Arduino instead of a Laptop?
1. **Always-On, Zero Context Switch:** Runs independently of the OS, sleep states, full-screen gaming, or active work applications.
2. **Dedicated Physicality:** Offers an uninterrupted, glanceable display sitting in the user's peripheral vision.
3. **Decoupled Architecture:** Acts as an edge display node while offloading heavy networking computation to a cloud micro-aggregator.

---

## 2. The Core Problem: Microcontrollers vs. Public APIs

A common anti-pattern when using Wi-Fi-enabled microcontrollers (like the ESP32-S3 on the Arduino UNO R4 WiFi) is attempting to treat them like full-fledged desktop web clients. 

Querying raw public APIs (e.g., Open-Meteo) directly from embedded C++ introduces severe bottlenecks:
* **Large Payloads:** Public weather responses typically span several kilobytes of nested JSON arrays with hourly breakdowns, forcing heavy heap allocations (`ArduinoJson`) and risking stack overflow or memory fragmentation.
* **TLS/SSL Handshake Overhead:** Constantly renegotiating complex HTTPS certificates and parsing variable response headers consumes processing time and increases connection failure rates.
* **Firmware Fragility:** Any upstream schema change in the public API breaks the microcontroller's firmware, requiring physical reprogramming.

### Solution: The Micro-Aggregator Pattern

Phase 1 adopts a **BFF (Backend-for-Frontend) / Edge Gateway** architecture using a lightweight serverless proxy (such as a Cloudflare Worker or Vercel Serverless Function).

```
   [Open-Meteo API]
          |  (Large payload: ~5-15 KB JSON)
          v
   +------------------------------------+
   |     Cloud Micro-Aggregator         |
   | (Cloudflare Worker / Edge Runtime) |
   | - Fetch hourly & current forecast  |
   | - Filter & normalize payload       |
   | - Sanitize to single-digit codes   |
   +------------------------------------+
          |
          |  (Ultralight payload: ~60 bytes JSON)
          v  HTTP GET / MQTT
   +------------------------------------+
   |      Arduino UNO R4 WiFi           |
   | - ESP32-S3 Network Layer           |
   | - Direct Buffer Parsing            |
   | - 12x8 LED Matrix Display Engine   |
   +------------------------------------+
```

---

## 3. Phase 1 MVP Scope

The Phase 1 MVP is intentionally decoupled from external breadboard peripherals (sensors, keypads, 7-segment displays) to eliminate hardware failure points and validate the end-to-end data pipeline first.

### Functional Requirements
1. **Connectivity:** Connect to designated 2.4 GHz Wi-Fi credentials on boot with auto-reconnect handling.
2. **Periodic Polling:** Request the edge proxy once every $N$ minutes (e.g., every 10–15 minutes) to respect rate limits and keep the connection lightweight.
3. **Forecast Window:**
   * **Current State:** Temperature (°C) and current weather classification (Clear, Cloudy, Rain).
   * **Short-Term Outlook (Next Hour):** Upcoming condition change (e.g., incoming rain or persistent clear skies) and probability of precipitation ($PoP$).
4. **Onboard Presentation:**
   * Utilize the built-in **12x8 LED Matrix** on the Arduino UNO R4 WiFi.
   * Render compact $8\times8$ or full $12\times8$ pixel art/glyphs representing weather conditions (Sun, Cloud, Animated Rain).
   * Alternate or scroll the temperature and rain probability across the matrix.

---

## 4. Aggregator Data Contract (Payload Design)

The micro-aggregator intercepts Open-Meteo data and downsizes the payload to a flat, deterministic JSON object designed for instant deserialization on embedded C++:

### Upstream (Open-Meteo Query)
```
GET https://api.open-meteo.com/v1/forecast?latitude=13.72&longitude=100.77&current=temperature_2m,weather_code&hourly=weather_code,precipitation_probability&forecast_hours=2
```

### Downstream (Micro-Aggregator Output to Arduino)
```json
{
  "cur_t": 31,
  "cur_icon": 0,
  "nxt_icon": 2,
  "pop": 75
}
```

### Data Dictionary
| Field | Type | Description | Values / Mapping |
| :--- | :--- | :--- | :--- |
| `cur_t` | Integer | Current outdoor temperature in Celsius | Round integer (e.g., `31`) |
| `cur_icon` | Integer | Standardized weather category ID | `0`: Clear / Sunny<br>`1`: Cloudy / Overcast<br>`2`: Rain / Drizzle<br>`3`: Thunderstorm |
| `nxt_icon` | Integer | Forecasted weather category in +1 hour | Same icon ID mapping as above |
| `pop` | Integer | Probability of precipitation in +1 hour | Percentage `0` to `100` |

---

## 5. Hardware Firmware Architecture (UNO R4 WiFi)

### Software Layers
* **Network Driver (`WiFiS3`):** Manages connection state, DHCP, and HTTP GET execution.
* **Parser (`ArduinoJson` or lightweight parser):** Reads directly from `client` stream with zero dynamic heap allocation.
* **Frame Engine (`Arduino_LED_Matrix`):**
  * Holds pre-defined 32-bit integer array masks for matrix bitmaps.
  * Runs a non-blocking render loop driven by `millis()` to handle alternating display modes:
    * **Slot A (3s):** Current condition icon (e.g., Sun).
    * **Slot B (2s):** Temperature readout (e.g., `31C`).
    * **Slot C (3s, conditional):** Next-hour warning icon if rain is expected ($PoP > 50\%$).

---

## 6. Implementation Milestones for Phase 1

1. **Worker Deployment:** Deploy Cloudflare Worker script fetching Open-Meteo coordinates; test endpoint via `curl` or browser.
2. **Matrix Glyphs Design:** Draft $12\times8$ binary pixel representations for Sun, Cloud, and Rain frames using the official Arduino Matrix Editor tool.
3. **Firmware Network Integration:** Verify Wi-Fi stability and JSON extraction using Serial Monitor output.
4. **Display Loop Binding:** Connect parsed data variables directly to the matrix frame buffer.