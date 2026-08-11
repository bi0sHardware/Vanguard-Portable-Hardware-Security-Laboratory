"use client";

import ScreenSection from "./ScreenSection";
import { asset } from "@/lib/basePath";

export default function Diagnostics() {
  return (
    <ScreenSection
      id="diagnostics"
      kicker="SETTINGS & DIAGNOSTICS"
      title="A professional diagnostics surface, not a hidden menu"
      description="Sound, WiFi profile setup, contact management, challenge state, and a dedicated hardware test suite are all reachable from one screen — along with a live status readout of badge ID, uptime, and battery."
      points={[
        "On-device hardware test suite",
        "Live badge ID, uptime, and battery readout",
        "Challenge progress reset for re-provisioning",
        "WiFi profile setup via captive portal, QR-assisted",
      ]}
      image={asset("/images/settings.webp")}
      imageAlt="Vanguard Settings and Diagnostics screen"
    />
  );
}
