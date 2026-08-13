"use client";

import { motion } from "framer-motion";

export default function BeyondCompetition() {
  return (
    <section className="relative border-t border-border py-24 lg:py-32">
      <div className="mx-auto max-w-3xl px-6 text-center lg:px-12">
        <motion.div
          initial={{ opacity: 0, y: 24 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.6 }}
        >
          <span className="mono text-xs tracking-[0.2em] text-accent">
            BEYOND THE EVENT
          </span>
          <h2 className="mt-4 text-3xl font-semibold tracking-tight text-white text-balance sm:text-5xl">
            The competition ends. The hardware doesn&apos;t.
          </h2>
          <p className="mx-auto mt-6 max-w-lg text-base leading-relaxed text-white/60 sm:text-lg">
            The challenges are the starting point, not the whole story. The
            badge is yours after the last flag is submitted.
          </p>
        </motion.div>
      </div>
    </section>
  );
}
