"use client";

import { motion } from "framer-motion";

export default function Roadmap() {
  return (
    <section className="relative border-t border-border py-24 lg:py-32">
      <div className="mx-auto max-w-5xl px-6 lg:px-12">
        <motion.div
          initial={{ opacity: 0, y: 24 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.6 }}
          className="mx-auto max-w-2xl text-center"
        >
          <span className="mono text-xs tracking-[0.2em] text-accent">
            ROADMAP
          </span>
          <h2 className="mt-4 text-3xl font-semibold tracking-tight text-white sm:text-4xl">
            Beyond Vanguard
          </h2>
          <p className="mt-5 text-base leading-relaxed text-white/60">
            One hardware platform. Multiple firmware experiences.
          </p>
        </motion.div>

        <div className="mt-16 grid grid-cols-1 gap-6 lg:grid-cols-3">
          <motion.div
            initial={{ opacity: 0, y: 20 }}
            whileInView={{ opacity: 1, y: 0 }}
            viewport={{ once: true, margin: "-80px" }}
            transition={{ duration: 0.6 }}
            className="rounded-lg border border-border p-8"
          >
            <span className="mono rounded-full border border-white/20 px-3 py-1 text-[10px] tracking-widest text-white/50">
              CURRENT
            </span>
            <h3 className="mt-5 text-lg font-semibold text-white/90">
              Security Challenge Firmware
            </h3>
            <p className="mt-4 text-sm leading-relaxed text-white/50">
              What ships on the badge today.
            </p>
          </motion.div>

          <motion.div
            initial={{ opacity: 0, y: 20 }}
            whileInView={{ opacity: 1, y: 0 }}
            viewport={{ once: true, margin: "-80px" }}
            transition={{ duration: 0.6, delay: 0.08 }}
            className="glow-accent rounded-lg border border-accent/40 bg-accent/[0.04] p-8"
          >
            <span className="mono rounded-full border border-accent/50 px-3 py-1 text-[10px] tracking-widest text-accent">
              COMING SOON
            </span>
            <h3 className="mt-5 text-xl font-semibold text-white">
              Cyber Lab Firmware
            </h3>
            <p className="mt-4 text-sm leading-relaxed text-white/60">
              A dedicated hardware security and wireless experimentation
              environment. Protocol analysis. Wireless tooling. Hardware
              exploration.
            </p>
          </motion.div>

          <motion.div
            initial={{ opacity: 0, y: 20 }}
            whileInView={{ opacity: 1, y: 0 }}
            viewport={{ once: true, margin: "-80px" }}
            transition={{ duration: 0.6, delay: 0.16 }}
            className="rounded-lg border border-border p-8"
          >
            <span className="mono rounded-full border border-white/20 px-3 py-1 text-[10px] tracking-widest text-white/50">
              COMING SOON
            </span>
            <h3 className="mt-5 text-lg font-semibold text-white/90">
              Gaming Firmware
            </h3>
            <p className="mt-4 text-sm leading-relaxed text-white/50">
              Multiplayer experiences and community activities.
            </p>
          </motion.div>
        </div>
      </div>
    </section>
  );
}
