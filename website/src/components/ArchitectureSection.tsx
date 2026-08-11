"use client";

import { motion } from "framer-motion";

const layers = [
  {
    title: "Hardware",
    items: ["ESP32-S3-WROOM-1", "TFT Display", "LoRa Radio Module", "Addressable LEDs", "Input Cluster"],
  },
  {
    title: "Firmware Core",
    items: ["AppState state machine", "enter() / frame() screens", "Subsystem managers (LED, audio, power)"],
  },
  {
    title: "Communication",
    items: ["LoRa link layer (radiolink)", "BLE / NimBLE stack", "Frame codec & addressing"],
  },
  {
    title: "Applications",
    items: ["Challenge Framework", "Radio Chat / Ship Battle", "PeerDrop", "Music Player & Games"],
  },
];

export default function ArchitectureSection() {
  return (
    <section id="architecture" className="relative border-t border-border py-24 lg:py-32">
      <div className="mx-auto max-w-6xl px-6 lg:px-12">
        <motion.div
          initial={{ opacity: 0, y: 24 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.6 }}
          className="mx-auto max-w-2xl text-center"
        >
          <span className="mono text-xs tracking-[0.2em] text-technical">
            ARCHITECTURE
          </span>
          <h2 className="mt-4 text-3xl font-semibold tracking-tight text-white sm:text-4xl">
            Hardware, firmware, and communication layers
          </h2>
          <p className="mt-5 text-base leading-relaxed text-white/60">
            Vanguard&apos;s firmware is a single `AppState` machine over always-on
            subsystem managers, with communication built as a shared layer
            underneath every wireless feature.
          </p>
        </motion.div>

        <div className="mt-16 space-y-3">
          {layers.map((layer, i) => (
            <motion.div
              key={layer.title}
              initial={{ opacity: 0, y: 16 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true, margin: "-60px" }}
              transition={{ duration: 0.5, delay: i * 0.1 }}
              className="flex flex-col gap-4 rounded-md border border-border p-6 sm:flex-row sm:items-center"
            >
              <div className="w-full shrink-0 sm:w-48">
                <span className="mono text-xs tracking-widest text-accent">
                  LAYER {String(i + 1).padStart(2, "0")}
                </span>
                <h3 className="mt-1 text-base font-semibold text-white">
                  {layer.title}
                </h3>
              </div>
              <div className="flex flex-wrap gap-2">
                {layer.items.map((item) => (
                  <span
                    key={item}
                    className="mono rounded-sm border border-white/10 px-3 py-1.5 text-[11px] text-white/60"
                  >
                    {item}
                  </span>
                ))}
              </div>
            </motion.div>
          ))}
          {/* connecting line */}
          <div className="pointer-events-none absolute left-1/2 top-[28%] hidden h-[44%] w-px -translate-x-1/2 bg-gradient-to-b from-transparent via-white/10 to-transparent lg:block" />
        </div>

        <p className="mono mt-10 text-center text-xs text-white/30">
          Full reference: docs/architecture/ in the firmware repository
        </p>
      </div>
    </section>
  );
}
