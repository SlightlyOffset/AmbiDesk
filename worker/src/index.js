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

export default {
  async fetch(request, env, ctx) {
    // Enable CORS for testing from browser / local tools
    const corsHeaders = {
      "Access-Control-Allow-Origin": "*",
      "Access-Control-Allow-Methods": "GET, OPTIONS",
      "Access-Control-Allow-Headers": "Content-Type",
      "Content-Type": "application/json; charset=utf-8",
      "Cache-Control": "public, max-age=300" // Cache 5 minutes
    };

    if (request.method === "OPTIONS") {
      return new Response(null, { headers: corsHeaders });
    }

    try {
      const url = new URL(request.url);
      const lat = url.searchParams.get("lat") || "13.72";
      const lon = url.searchParams.get("lon") || "100.77";

      const upstreamUrl = `https://api.open-meteo.com/v1/forecast?latitude=${encodeURIComponent(lat)}&longitude=${encodeURIComponent(lon)}&current=temperature_2m,weather_code&hourly=weather_code,temperature_2m,precipitation_probability&forecast_days=5&timezone=Asia%2FBangkok`;

      const response = await fetch(upstreamUrl, {
        headers: {
          "User-Agent": "AmbiDesk-Aggregator/1.0"
        }
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

      const hourlyTimes = data.hourly?.time || [];
      const hourlyTemps = data.hourly?.temperature_2m || [];
      const hourlyCodes = data.hourly?.weather_code || [];
      const hourlyPops = data.hourly?.precipitation_probability || [];

      // Open-Meteo current.time has minutes (e.g. 2026-10-22T11:15) while hourly is on the hour (2026-10-22T11:00)
      const currentHour = curTime ? curTime.substring(0, 13) + ":00" : "";
      let curIdx = hourlyTimes.indexOf(currentHour);
      if (curIdx === -1) {
        curIdx = 0;
      }

      // Definition of 9 time offsets:
      // 0: Now
      // 1: +1 hour
      // 2: +3 hours
      // 3: +5 hours
      // 4: +12 hours
      // 5: +1 day (24h)
      // 6: +2 days (48h)
      // 7: +3 days (72h)
      // 8: +4 days (96h)
      const offsetDefs = [
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

      const curTimeParts = curTime.split("T");
      const curClock = curTimeParts[1] || "12:00";

      const slots = offsetDefs.map(def => {
        const idx = Math.min(curIdx + def.offset, hourlyTimes.length - 1);
        const rawTime = hourlyTimes[idx] || curTime;
        const temp = (idx >= 0 && idx < hourlyTemps.length) ? Math.round(hourlyTemps[idx]) : currentTemp;
        const wmoCode = (idx >= 0 && idx < hourlyCodes.length) ? hourlyCodes[idx] : currentWmo;
        const pop = (idx >= 0 && idx < hourlyPops.length) ? Math.round(hourlyPops[idx]) : 0;

        let lbl = "";
        const parts = (rawTime || "").split("T");
        const datePart = parts[0] || "";
        const timePart = parts[1] || "";
        const dateSplit = datePart.split("-");
        const month = dateSplit[1] || "01";
        const day = dateSplit[2] || "01";

        if (def.offset === 0) {
          lbl = `Now ${curClock}`;
        } else if (!def.isDaily) {
          lbl = `${def.prefix} ${timePart || "12:00"}`;
        } else {
          lbl = `${def.prefix} ${day}/${month}`;
        }

        return {
          lbl: lbl.substring(0, 10),
          t: temp,
          ico: mapWmoToCategory(wmoCode),
          pop: pop
        };
      });

      const nextIcon = slots.length > 1 ? slots[1].ico : curIcon;
      const curPop = slots.length > 0 ? slots[0].pop : 0;

      const payload = {
        cur_t: currentTemp,
        cur_icon: curIcon,
        nxt_icon: nextIcon,
        pop: curPop,
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
