# Security notes

This project was designed for a trusted local network.

- The web UI uses HTTP, not HTTPS.
- The web UI has no built-in user authentication.
- The first-start setup access point is open.
- Wi-Fi credentials are stored in ESP32 Preferences/NVS.
- MQTT security depends on the local Venus OS / broker configuration.

Do not expose the ESP32 web interface or MQTT broker directly to the public Internet.
Use network segmentation, firewall rules, or a VPN if remote access is required.
