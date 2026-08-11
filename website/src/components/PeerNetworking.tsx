"use client";

import ScreenSection from "./ScreenSection";
import { asset } from "@/lib/basePath";

export default function PeerNetworking() {
  return (
    <ScreenSection
      id="peerdrop"
      kicker="PEER NETWORKING"
      title="Direct device-to-device exchange"
      description="PeerDrop moves identity and contact data between two nearby badges over Bluetooth Low Energy — proximity-gated, with an explicit confirm step on both sides before anything is exchanged."
      points={[
        "RSSI-gated discovery — closer badges surface first",
        "Explicit Send/Receive roles, not silent auto-pairing",
        "Exchanged contacts persist locally on-device",
        "Built on NimBLE, documented in the architecture reference",
      ]}
      image={asset("/images/peerdrop.webp")}
      imageAlt="Vanguard PeerDrop screen scanning for nearby badges"
      reverse
    />
  );
}
