# AmbiDesk Micro-Aggregator (Cloudflare Worker)

This edge worker acts as a Backend-For-Frontend (BFF) proxy between the **Open-Meteo REST API** and the **Arduino UNO R4 WiFi**.

## Why this exists
- Downsizes upstream Open-Meteo responses from ~15 KB down to ~60 bytes.
- Standardizes weather conditions into simple category IDs (`0`: Sun, `1`: Cloud, `2`: Rain, `3`: Thunderstorm).
- Shields Arduino firmware from upstream API schema changes and heavy JSON deserialization overhead.

## API Endpoint Contract
- **Method:** `GET`
- **Query Parameters (optional):**
  - `lat`: Latitude (default: `13.72`)
  - `lon`: Longitude (default: `100.77`)
- **Response (`application/json`):**
  ```json
  {
    "cur_t": 31,
    "cur_icon": 0,
    "nxt_icon": 2,
    "pop": 75
  }
  ```

## Deployment Options

### Option A: Cloudflare Dashboard (Quickest - No CLI needed)
1. Log in to [Cloudflare Dashboard](https://dash.cloudflare.com/) and go to **Workers & Pages**.
2. Click **Create Application** > **Create Worker**.
3. Name it `ambidesk-aggregator` and click **Deploy**.
4. Click **Edit code**, replace the code in `worker.js` with the contents of `src/index.js`, and click **Save and Deploy**.
5. Copy your worker's public URL (e.g. `https://ambidesk-aggregator.<subdomain>.workers.dev`).

### Option B: Wrangler CLI
```bash
cd worker
npx wrangler login
npx wrangler deploy
```
