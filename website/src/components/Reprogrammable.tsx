"use client";

import { motion } from "framer-motion";

const capabilities = [
  "Replace existing functionality",
  "Build new tools",
  "Create custom firmware",
  "Develop your own communication systems",
];

export default function Reprogrammable() {
  return (
    <section id="reprogrammable" className="relative border-t border-border py-24 lg:py-32">
      <div className="grid-overlay absolute inset-0 opacity-[0.12]" />
      <div className="relative mx-auto max-w-3xl px-6 text-center lg:px-12">
        <motion.div
          initial={{ opacity: 0, y: 24 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.6 }}
        >
          <span className="mono text-xs tracking-[0.2em] text-technical">
            FULLY REPROGRAMMABLE
          </span>
          <h2 className="mt-4 text-3xl font-semibold tracking-tight text-white sm:text-5xl">
            Nothing locked down.
          </h2>
          <p className="mt-6 text-base leading-relaxed text-white/60 sm:text-lg">
            Vanguard is built around the ESP32-S3. The platform is meant to
            be modified.
          </p>
        </motion.div>

        <motion.div
          initial={{ opacity: 0, y: 16 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-60px" }}
          transition={{ duration: 0.5, delay: 0.1 }}
          className="mt-10 flex flex-wrap items-center justify-center gap-3"
        >
          {capabilities.map((c) => (
            <span
              key={c}
              className="mono rounded-full border border-white/10 px-4 py-2 text-xs tracking-wide text-white/70"
            >
              {c}
            </span>
          ))}
        </motion.div>
      </div>
    </section>
  );
}
