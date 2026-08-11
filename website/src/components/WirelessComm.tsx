"use client";

import { motion } from "framer-motion";
import DeviceFrame from "./DeviceFrame";
import { asset } from "@/lib/basePath";

const features = [
  { name: "Radio Chat", desc: "A handheld LoRa messenger with live peer discovery." },
  { name: "Morse Mode", desc: "Manual CW send/receive layered on the same link." },
  { name: "Reliable Delivery", desc: "Acknowledged sends with automatic retry on loss." },
  { name: "Shared Link Layer", desc: "One protocol underlies chat, gameplay, and telemetry." },
];

function PacketLink() {
  return (
    <div className="relative flex h-16 items-center justify-center">
      <div className="h-px w-full max-w-xs bg-gradient-to-r from-transparent via-white/20 to-transparent" />
      {[0, 1, 2].map((i) => (
        <motion.span
          key={i}
          className="absolute h-1.5 w-1.5 rounded-full bg-technical shadow-[0_0_8px_rgba(0,212,255,0.8)]"
          initial={{ x: "-140px", opacity: 0 }}
          animate={{ x: "140px", opacity: [0, 1, 1, 0] }}
          transition={{
            duration: 2.2,
            repeat: Infinity,
            delay: i * 0.75,
            ease: "linear",
          }}
        />
      ))}
    </div>
  );
}

export default function WirelessComm() {
  return (
    <section id="wireless" className="relative border-t border-border py-24 lg:py-32">
      <div className="mx-auto max-w-7xl px-6 lg:px-12">
        <div className="grid grid-cols-1 items-center gap-16 lg:grid-cols-2">
          <motion.div
            initial={{ opacity: 0, y: 24 }}
            whileInView={{ opacity: 1, y: 0 }}
            viewport={{ once: true, margin: "-100px" }}
            transition={{ duration: 0.6 }}
          >
            <span className="mono text-xs tracking-[0.2em] text-technical">
              WIRELESS COMMUNICATION
            </span>
            <h2 className="mt-4 text-3xl font-semibold tracking-tight text-white text-balance lg:text-4xl">
              LoRa communication between badges
            </h2>
            <p className="mt-5 max-w-lg text-base leading-relaxed text-white/60">
              A shared LoRa link layer moves messages, game state, and
              telemetry between badges — a working long-range radio stack to
              use, study, or build new tools on top of.
            </p>

            <PacketLink />

            <div className="mt-8 grid grid-cols-1 gap-4 sm:grid-cols-2">
              {features.map((f) => (
                <div
                  key={f.name}
                  className="rounded-md border border-border p-4"
                >
                  <h3 className="text-sm font-semibold text-white">{f.name}</h3>
                  <p className="mt-2 text-xs leading-relaxed text-white/50">
                    {f.desc}
                  </p>
                </div>
              ))}
            </div>
          </motion.div>

          <DeviceFrame
            src={asset("/images/radio-chat.webp")}
            alt="Vanguard Radio Chat screen"
          />
        </div>
      </div>
    </section>
  );
}
