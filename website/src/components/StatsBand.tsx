"use client";

import { motion } from "framer-motion";

const rows = [
  { key: "CHALLENGE_LEVELS", value: "4" },
  { key: "RADIO_PROTOCOLS", value: "LoRa / BLE" },
  { key: "MCU", value: "ESP32-S3" },
  { key: "FIRMWARE_SOURCE", value: "OPEN" },
];

export default function StatsBand() {
  return (
    <section className="relative border-t border-b border-border bg-[#050505] py-10">
      <div className="mx-auto max-w-4xl px-6 lg:px-12">
        <motion.div
          initial={{ opacity: 0 }}
          whileInView={{ opacity: 1 }}
          viewport={{ once: true, margin: "-60px" }}
          transition={{ duration: 0.5 }}
          className="mono overflow-x-auto rounded-md border border-white/10 bg-black/60 p-5 text-xs sm:text-sm"
        >
          <div className="mb-3 flex items-center gap-2 text-white/30">
            <span className="h-1.5 w-1.5 rounded-full bg-technical" />
            SYSTEM // vanguard --status
          </div>
          {rows.map((row, i) => (
            <motion.div
              key={row.key}
              initial={{ opacity: 0, x: -8 }}
              whileInView={{ opacity: 1, x: 0 }}
              viewport={{ once: true }}
              transition={{ duration: 0.3, delay: i * 0.08 }}
              className="flex items-center gap-3 py-1 text-white/70"
            >
              <span className="text-technical">&gt;</span>
              <span className="text-white/40">{row.key}</span>
              <span className="flex-1 border-b border-dotted border-white/10" />
              <span className="text-accent">{row.value}</span>
            </motion.div>
          ))}
          <div className="mt-2 flex items-center gap-2 text-white/30">
            <span className="text-technical">&gt;</span>
            <span className="animate-pulse">_</span>
          </div>
        </motion.div>
      </div>
    </section>
  );
}
