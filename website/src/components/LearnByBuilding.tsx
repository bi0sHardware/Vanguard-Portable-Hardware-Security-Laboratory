"use client";

import { motion } from "framer-motion";

const topics = [
  "Embedded Systems",
  "Hardware Interfaces",
  "Wireless Protocols",
  "Communication Systems",
  "Device Architecture",
  "Security Concepts",
];

export default function LearnByBuilding() {
  return (
    <section className="relative border-t border-border py-24 lg:py-32">
      <div className="mx-auto max-w-5xl px-6 text-center lg:px-12">
        <motion.div
          initial={{ opacity: 0, y: 24 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.6 }}
        >
          <span className="mono text-xs tracking-[0.2em] text-accent">
            LEARN BY BUILDING
          </span>
          <h2 className="mt-4 text-3xl font-semibold tracking-tight text-white sm:text-5xl">
            Learn. Modify. Experiment. Build.
          </h2>
          <p className="mx-auto mt-6 max-w-2xl text-base leading-relaxed text-white/60 sm:text-lg">
            Vanguard encourages practical learning through direct
            experimentation with real hardware and firmware, not abstracted
            simulations.
          </p>
        </motion.div>

        <div className="mt-14 grid grid-cols-2 gap-4 sm:grid-cols-3">
          {topics.map((t, i) => (
            <motion.div
              key={t}
              initial={{ opacity: 0, scale: 0.95 }}
              whileInView={{ opacity: 1, scale: 1 }}
              viewport={{ once: true, margin: "-60px" }}
              transition={{ duration: 0.4, delay: i * 0.06 }}
              className="rounded-md border border-border py-6 text-sm text-white/70"
            >
              {t}
            </motion.div>
          ))}
        </div>
      </div>
    </section>
  );
}
