/**
 * AmbiDesk Micro-Aggregator (Cloudflare Worker)
 * 
 * Intercepts Open-Meteo API, filters and downsizes multi-KB forecasts
 * into a minimal ~60-byte JSON payload for the Arduino UNO R4 WiFi.
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

      const upstreamUrl = `https://api.open-meteo.com/v1/forecast?latitude=${encodeURIComponent(lat)}&longitude=${encodeURIComponent(lon)}&current=temperature_2m,weather_code&hourly=weather_code,precipitation_probability&forecast_hours=2`;

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

      // Hourly outlook index 1 (+1 hour forecast)
      const hourlyCodes = data.hourly?.weather_code || [];
      const hourlyPop = data.hourly?.precipitation_probability || [];

      const nextWmo = hourlyCodes.length > 1 ? hourlyCodes[1] : currentWmo;
      const nxtIcon = mapWmoToCategory(nextWmo);
      const pop = hourlyPop.length > 1 ? Math.round(hourlyPop[1]) : 0;

      const payload = {
        cur_t: currentTemp,
        cur_icon: curIcon,
        nxt_icon: nxtIcon,
        pop: pop
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
