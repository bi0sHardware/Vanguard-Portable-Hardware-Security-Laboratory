"use client";

import { motion } from "framer-motion";

const INSTAGRAM_URL = "https://www.instagram.com/bi0shardware?igsh=MXYzMXpldzNyaXY0ag==";
const EMAIL = "teambi0shardware@gmail.com";

export default function GetVanguard() {
  return (
    <section id="get-vanguard" className="relative border-t border-border py-24 lg:py-32">
      <div className="mx-auto max-w-3xl px-6 text-center lg:px-12">
        <motion.div
          initial={{ opacity: 0, y: 24 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.6 }}
        >
          <span className="mono text-xs tracking-[0.2em] text-accent">
            GET A BADGE
          </span>
          <h2 className="mt-4 text-3xl font-semibold tracking-tight text-white sm:text-4xl">
            DM us on Instagram
          </h2>
          <p className="mx-auto mt-5 max-w-lg text-base leading-relaxed text-white/60">
            Vanguard badges are handed out directly by the bi0s Hardware
            team. Send a DM and we&apos;ll take it from there.
          </p>
        </motion.div>

        <motion.div
          initial={{ opacity: 0, y: 16 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-60px" }}
          transition={{ duration: 0.5, delay: 0.1 }}
          className="mt-10 flex flex-wrap items-center justify-center gap-4"
        >
          <a
            href={INSTAGRAM_URL}
            target="_blank"
            rel="noreferrer"
            className="mono rounded-sm bg-accent px-7 py-3.5 text-xs tracking-wider text-black transition-transform hover:scale-[1.03]"
          >
            DM @bi0shardware
          </a>
          <a
            href={`mailto:${EMAIL}`}
            className="mono rounded-sm border border-white/20 px-7 py-3.5 text-xs tracking-wider text-white transition-colors hover:border-technical hover:text-technical"
          >
            OR EMAIL US
          </a>
        </motion.div>

        <p className="mono mt-6 text-xs text-white/30">{EMAIL}</p>
      </div>
    </section>
  );
}
