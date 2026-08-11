"use client";

import { motion } from "framer-motion";

const specGroups = [
  {
    label: "Compute",
    rows: [
      ["MCU", "ESP32-S3-WROOM-1"],
      ["Cores", "Dual-core, WiFi + BLE"],
      ["Build system", "ESP-IDF, Arduino as a component"],
    ],
  },
  {
    label: "Display & Input",
    rows: [
      ["Display", "Color TFT, direct-draw rendering"],
      ["Input", "Joystick + 5-button cluster"],
      ["Feedback", "14-LED addressable chain, buzzer audio"],
    ],
  },
  {
    label: "Radio",
    rows: [
      ["Long range", "LoRa module, external antenna"],
      ["Short range", "Bluetooth Low Energy (NimBLE)"],
      ["Protocols", "Shared link layer across chat, games, telemetry"],
    ],
  },
  {
    label: "Power",
    rows: [
      ["Battery", "Li-ion cell, onboard charge circuit"],
      ["Charging / flashing", "USB-C"],
    ],
  },
];

export default function Specifications() {
  return (
    <section id="specifications" className="relative border-t border-border py-24 lg:py-32">
      <div className="mx-auto max-w-6xl px-6 lg:px-12">
        <motion.div
          initial={{ opacity: 0, y: 24 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.6 }}
          className="mx-auto max-w-2xl text-center"
        >
          <span className="mono text-xs tracking-[0.2em] text-technical">
            SPECIFICATIONS
          </span>
          <h2 className="mt-4 text-3xl font-semibold tracking-tight text-white sm:text-4xl">
            What&apos;s inside
          </h2>
          <p className="mt-5 text-base leading-relaxed text-white/60">
            Every subsystem is documented in the firmware repository —
            this is the short version.
          </p>
        </motion.div>

        <div className="mt-16 grid grid-cols-1 gap-6 sm:grid-cols-2">
          {specGroups.map((group, gi) => (
            <motion.div
              key={group.label}
              initial={{ opacity: 0, y: 16 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true, margin: "-60px" }}
              transition={{ duration: 0.5, delay: gi * 0.08 }}
              className="rounded-lg border border-border p-6"
            >
              <h3 className="mono text-xs tracking-widest text-accent">
                {group.label.toUpperCase()}
              </h3>
              <dl className="mt-5 space-y-4">
                {group.rows.map(([k, v]) => (
                  <div
                    key={k}
                    className="flex items-baseline justify-between gap-4 border-t border-white/5 pt-4 first:border-t-0 first:pt-0"
                  >
                    <dt className="text-xs text-white/40">{k}</dt>
                    <dd className="text-right text-sm text-white/80">{v}</dd>
                  </div>
                ))}
              </dl>
            </motion.div>
          ))}
        </div>
      </div>
    </section>
  );
}
