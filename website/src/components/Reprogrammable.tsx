"use client";

import { motion } from "framer-motion";

const capabilities = [
  "Build custom firmware",
  "Replace existing applications",
  "Develop new communication tools",
  "Create security utilities",
  "Prototype embedded projects",
  "Extend platform capabilities",
];

const highlights = [
  { label: "Open Firmware", desc: "Full source, not a locked-down binary." },
  { label: "ESP32-S3 Based", desc: "A widely documented, well-supported MCU." },
  { label: "Source Available", desc: "Read the same code that ships on the badge." },
  { label: "Developer Friendly", desc: "Docker-based build, no toolchain setup." },
  { label: "Designed for Modification", desc: "Modular subsystems, not a monolith." },
];

export default function Reprogrammable() {
  return (
    <section id="reprogrammable" className="relative border-t border-border py-24 lg:py-32">
      <div className="grid-overlay absolute inset-0 opacity-[0.12]" />
      <div className="relative mx-auto max-w-7xl px-6 lg:px-12">
        <motion.div
          initial={{ opacity: 0, y: 24 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.6 }}
          className="mx-auto max-w-3xl text-center"
        >
          <span className="mono text-xs tracking-[0.2em] text-technical">
            FULLY REPROGRAMMABLE
          </span>
          <h2 className="mt-4 text-3xl font-semibold tracking-tight text-white sm:text-5xl">
            Built around the ESP32-S3. Not locked down.
          </h2>
          <p className="mt-6 text-base leading-relaxed text-white/60 sm:text-lg">
            Vanguard ships with a complete firmware image, but nothing stops
            you from replacing it. The platform is designed to be built on.
          </p>
        </motion.div>

        <div className="mt-16 grid grid-cols-1 gap-16 lg:grid-cols-2">
          <motion.div
            initial={{ opacity: 0, x: -20 }}
            whileInView={{ opacity: 1, x: 0 }}
            viewport={{ once: true, margin: "-80px" }}
            transition={{ duration: 0.6 }}
          >
            <h3 className="mono text-xs tracking-widest text-white/40">
              WHAT YOU CAN DO
            </h3>
            <ul className="mt-6 space-y-4">
              {capabilities.map((c) => (
                <li key={c} className="flex items-center gap-3 text-sm text-white/80">
                  <span className="mono text-accent">→</span>
                  {c}
                </li>
              ))}
            </ul>
          </motion.div>

          <motion.div
            initial={{ opacity: 0, x: 20 }}
            whileInView={{ opacity: 1, x: 0 }}
            viewport={{ once: true, margin: "-80px" }}
            transition={{ duration: 0.6 }}
            className="grid grid-cols-1 gap-4 sm:grid-cols-2"
          >
            {highlights.map((h) => (
              <div key={h.label} className="rounded-md border border-border p-5">
                <h4 className="text-sm font-semibold text-white">{h.label}</h4>
                <p className="mt-2 text-xs leading-relaxed text-white/50">{h.desc}</p>
              </div>
            ))}
          </motion.div>
        </div>
      </div>
    </section>
  );
}
