"use client";

import { motion } from "framer-motion";

const pillars = [
  {
    title: "Learning Platform",
    description:
      "A physical device for working through hardware and firmware concepts hands-on, not in an emulator.",
  },
  {
    title: "Security Training Platform",
    description:
      "Structured, progressive challenges grounded in real interfaces — UART, RF, and protocol-level exercises.",
  },
  {
    title: "Embedded Development Platform",
    description:
      "An ESP32-S3 target with a display, radio, storage, and input already wired up and ready to build on.",
  },
  {
    title: "Wireless Experimentation Platform",
    description:
      "A working LoRa and BLE stack to study, extend, or replace — not a black box.",
  },
  {
    title: "Research Platform",
    description:
      "Open firmware source and documented architecture for anyone extending the platform beyond its shipped state.",
  },
];

export default function PlatformIdentity() {
  return (
    <section className="relative border-t border-border py-24 lg:py-32">
      <div className="grid-overlay absolute inset-0 opacity-[0.15]" />
      <div className="relative mx-auto max-w-7xl px-6 lg:px-12">
        <motion.div
          initial={{ opacity: 0, y: 24 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.6 }}
          className="mx-auto max-w-3xl text-center"
        >
          <span className="mono text-xs tracking-[0.2em] text-technical">
            IDENTITY
          </span>
          <h2 className="mt-4 text-3xl font-semibold tracking-tight text-white text-balance sm:text-5xl">
            Portable Hardware Security Laboratory
          </h2>
          <p className="mt-6 text-base leading-relaxed text-white/60 sm:text-lg">
            Vanguard covers hardware exploration, embedded systems, IoT
            security, wireless communication, and device architecture — one
            badge, five overlapping disciplines.
          </p>
        </motion.div>

        <div className="mt-16 grid grid-cols-1 gap-px overflow-hidden rounded-lg border border-border bg-border sm:grid-cols-2 lg:grid-cols-5">
          {pillars.map((pillar, i) => (
            <motion.div
              key={pillar.title}
              initial={{ opacity: 0, y: 16 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true, margin: "-60px" }}
              transition={{ duration: 0.5, delay: i * 0.08 }}
              className="bg-black p-6"
            >
              <span className="mono text-xs text-accent">0{i + 1}</span>
              <h3 className="mt-4 text-sm font-semibold text-white">
                {pillar.title}
              </h3>
              <p className="mt-3 text-xs leading-relaxed text-white/50">
                {pillar.description}
              </p>
            </motion.div>
          ))}
        </div>
      </div>
    </section>
  );
}
