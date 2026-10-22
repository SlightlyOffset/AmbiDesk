/**
 * AmbiDesk Micro-Aggregator (Cloudflare Worker)
 * 
 * Intercepts Open-Meteo API, filters and downsizes multi-KB forecasts
 * into a minimal compact JSON payload for the Arduino UNO R4 WiFi.
 */

// Mapping Open-Meteo WMO weather codes to standardized AmbiDesk category IDs:
// 0: Clear / Sunny
// 1: Cloudy / Fog
// 2: Rain / Drizzle
// 3: Thunderstorm / Severe
function mapWmoToCategory(wmoCode) {
  if (wmoCode === 0 || wmoCode === 1) {
    return 0; // Clear / Sunny
  }
  if (wmoCode === 2 || wmoCode === 3 || wmoCode === 45 || wmoCode === 48) {
    return 1; // Cloudy / Overcast / Fog
  }
  if (
    (wmoCode >= 51 && wmoCode <= 67) ||
    (wmoCode >= 80 && wmoCode <= 82)
  ) {
    return 2; // Rain / Drizzle / Showers
  }
  if (
    (wmoCode >= 95 && wmoCode <= 99) ||
    (wmoCode >= 71 && wmoCode <= 77) ||
    (wmoCode >= 85 && wmoCode <= 86)
  ) {
    return 3; // Thunderstorm / Severe / Snow
  }
  return 1; // Default fallback to Cloudy
}

function formatSlotLabel(def, rawTime, curClock) {
  if (def.offset === 0) {
    return `Now ${curClock}`.substring(0, 10);
  }
  if (!rawTime) {
    return def.prefix.substring(0, 10);
  }
  const parts = rawTime.split("T");
  const datePart = parts[0] || "";
  const timePart = parts[1] || "";
  if (!def.isDaily) {
    return `${def.prefix} ${timePart || "12:00"}`.substring(0, 10);
  }
  const dateSplit = datePart.split("-");
  const month = dateSplit[1] || "01";
  const day = dateSplit[2] || "01";
  return `${def.prefix} ${day}/${month}`.substring(0, 10);
}

const OFFSET_DEFS = [
  { prefix: "Now",  offset: 0,  isDaily: false },
  { prefix: "+1h",  offset: 1,  isDaily: false },
  { prefix: "+3h",  offset: 3,  isDaily: false },
  { prefix: "+5h",  offset: 5,  isDaily: false },
  { prefix: "+12h", offset: 12, isDaily: false },
  { prefix: "+1d",  offset: 24, isDaily: true },
  { prefix: "+2d",  offset: 48, isDaily: true },
  { prefix: "+3d",  offset: 72, isDaily: true },
  { prefix: "+4d",  offset: 96, isDaily: true }
];

function buildForecastSlots(data, currentTemp, currentWmo, curTime, curClock) {
  const hourlyTimes = Array.isArray(data.hourly?.time) ? data.hourly.time : [];
  const hourlyTemps = Array.isArray(data.hourly?.temperature_2m) ? data.hourly.temperature_2m : [];
  const hourlyCodes = Array.isArray(data.hourly?.weather_code) ? data.hourly.weather_code : [];
  const hourlyPops = Array.isArray(data.hourly?.precipitation_probability) ? data.hourly.precipitation_probability : [];

  if (hourlyTimes.length === 0) {
    return [{
      lbl: `Now ${curClock}`.substring(0, 10),
      t: currentTemp,
      ico: mapWmoToCategory(currentWmo),
      pop: 0
    }];
  }

  const currentHour = curTime ? curTime.substring(0, 13) + ":00" : "";
  let curIdx = hourlyTimes.indexOf(currentHour);
  if (curIdx < 0) {
    curIdx = 0;
  }

  return OFFSET_DEFS.map(def => {
    const targetIdx = curIdx + def.offset;
    const idx = Math.min(Math.max(targetIdx, 0), hourlyTimes.length - 1);
    const rawTime = hourlyTimes[idx] || curTime;
    const temp = (idx >= 0 && idx < hourlyTemps.length && hourlyTemps[idx] != null)
      ? Math.round(hourlyTemps[idx])
      : currentTemp;
    const wmoCode = (idx >= 0 && idx < hourlyCodes.length && hourlyCodes[idx] != null)
      ? hourlyCodes[idx]
      : currentWmo;
    const pop = (idx >= 0 && idx < hourlyPops.length && hourlyPops[idx] != null)
      ? Math.round(hourlyPops[idx])
      : 0;

    return {
      lbl: formatSlotLabel(def, rawTime, curClock),
      t: temp,
      ico: mapWmoToCategory(wmoCode),
      pop: Math.min(Math.max(pop, 0), 100)
    };
  });
}

export default {
  async fetch(request, env, ctx) {
    const corsHeaders = {
      "Access-Control-Allow-Origin": "*",
      "Access-Control-Allow-Methods": "GET, OPTIONS",
      "Access-Control-Allow-Headers": "Content-Type",
      "Content-Type": "application/json; charset=utf-8",
      "Cache-Control": "public, max-age=300"
    };

    if (request.method === "OPTIONS") {
      return new Response(null, { headers: corsHeaders });
    }

    try {
      const url = new URL(request.url);
      const lat = url.searchParams.get("lat") || "13.72";
      const lon = url.searchParams.get("lon") || "100.77";

      const upstreamUrl = `https://api.open-meteo.com/v1/forecast?latitude=${encodeURIComponent(lat)}&longitude=${encodeURIComponent(lon)}&current=temperature_2m,weather_code&hourly=weather_code,temperature_2m,precipitation_probability&forecast_days=5&timezone=auto`;

      const response = await fetch(upstreamUrl, {
        headers: { "User-Agent": "AmbiDesk-Aggregator/1.0" }
      });

      if (!response.ok) {
        return new Response(
          JSON.stringify({ error: `Upstream error: ${response.status}` }),
          { status: 502, headers: corsHeaders }
        );
      }

      const data = await response.json();
      const currentTemp = Math.round(data.current?.temperature_2m ?? 25);
      const currentWmo = data.current?.weather_code ?? 0;
      const curIcon = mapWmoToCategory(currentWmo);
      const curTime = data.current?.time || "";
      const curClock = curTime.includes("T") ? curTime.split("T")[1] : "12:00";

      const slots = buildForecastSlots(data, currentTemp, currentWmo, curTime, curClock);
      const nextIcon = slots.length > 1 ? slots[1].ico : curIcon;
      // Sync legacy pop with +1h forecast (slots[1]) to match next-hour rain warning display
      const nextPop = slots.length > 1 ? slots[1].pop : (slots[0]?.pop ?? 0);

      const payload = {
        cur_t: currentTemp,
        cur_icon: curIcon,
        nxt_icon: nextIcon,
        pop: nextPop,
        slots: slots
      };

      return new Response(JSON.stringify(payload), {
        status: 200,
        headers: corsHeaders
      });
    } catch (err) {
      return new Response(
        JSON.stringify({ error: err.message || "Internal server error" }),
        { status: 500, headers: corsHeaders }
      );
    }
  }
};
