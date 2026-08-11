"use client";

import { motion } from "framer-motion";

const steps = [
  "Reflash the device",
  "Modify the firmware",
  "Build your own applications",
  "Create your own tools",
  "Experiment with protocols",
  "Learn embedded development",
  "Explore wireless systems",
];

export default function BeyondCompetition() {
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
            BEYOND THE COMPETITION
          </span>
          <h2 className="mt-4 text-3xl font-semibold tracking-tight text-white text-balance sm:text-5xl">
            The event is only the beginning.
          </h2>
          <p className="mx-auto mt-6 max-w-2xl text-base leading-relaxed text-white/60 sm:text-lg">
            After the competition, the badge stays with you — and the
            hardware stays useful long after the last flag is submitted.
          </p>
        </motion.div>

        <div className="mt-14 flex flex-wrap items-center justify-center gap-3">
          {steps.map((step, i) => (
            <motion.span
              key={step}
              initial={{ opacity: 0, y: 10 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true }}
              transition={{ duration: 0.4, delay: i * 0.06 }}
              className="mono rounded-full border border-white/10 px-4 py-2 text-xs tracking-wide text-white/70"
            >
              {step}
            </motion.span>
          ))}
        </div>
      </div>
    </section>
  );
}
