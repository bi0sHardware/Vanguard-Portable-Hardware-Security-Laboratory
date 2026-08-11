"use client";

import { motion } from "framer-motion";
import StarField from "./StarField";

export default function FinalCTA() {
  return (
    <section className="relative flex min-h-[70vh] flex-col items-center justify-center overflow-hidden border-t border-border py-24 text-center">
      <StarField density={80} shootingStars />
      <div className="grid-overlay absolute inset-0 opacity-20" />

      <motion.div
        initial={{ opacity: 0, y: 24 }}
        whileInView={{ opacity: 1, y: 0 }}
        viewport={{ once: true, margin: "-100px" }}
        transition={{ duration: 0.6 }}
        className="relative mx-auto max-w-3xl px-6"
      >
        <h2 className="text-4xl font-semibold tracking-tight text-white sm:text-6xl">
          Build.
          <br />
          Learn.
          <br />
          Explore.
        </h2>
        <p className="mx-auto mt-8 max-w-xl text-balance text-base leading-relaxed text-white/60 sm:text-lg">
          A portable platform for learning and exploring hardware security,
          embedded systems, wireless communication, cybersecurity concepts,
          and embedded development.
        </p>

        <div className="mt-10 flex flex-wrap items-center justify-center gap-4">
          <a
            href="https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/tree/main/firmware"
            target="_blank"
            rel="noreferrer"
            className="mono rounded-sm bg-accent px-6 py-3 text-xs tracking-wider text-black transition-transform hover:scale-[1.03]"
          >
            GET FIRMWARE
          </a>
          <a
            href="https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/wiki"
            target="_blank"
            rel="noreferrer"
            className="mono rounded-sm border border-white/20 px-6 py-3 text-xs tracking-wider text-white transition-colors hover:border-technical hover:text-technical"
          >
            DOCUMENTATION
          </a>
          <a
            href="#get-vanguard"
            className="mono rounded-sm border border-white/20 px-6 py-3 text-xs tracking-wider text-white transition-colors hover:border-white/40"
          >
            GET A BADGE
          </a>
        </div>
      </motion.div>
    </section>
  );
}
