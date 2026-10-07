#pragma once

// Copy this file to secrets.h and populate with your credentials
#define WIFI_SSID         "Your_WiFi_SSID"
#define WIFI_PASSWORD     "Your_WiFi_Password"

// Micro-Aggregator Host (Cloudflare Worker or local mock)
// E.g. "ambidesk-aggregator.<subdomain>.workers.dev" or local IP "192.168.1.100"
#define AGGREGATOR_HOST   "ambidesk-aggregator.workers.dev"
#define AGGREGATOR_PORT   443
#define AGGREGATOR_PATH   "/"
#define AGGREGATOR_USE_SSL true
