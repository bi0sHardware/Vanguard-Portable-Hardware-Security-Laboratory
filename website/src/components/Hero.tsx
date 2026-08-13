"use client";

import { motion, useScroll, useTransform } from "framer-motion";
import { useRef } from "react";
import StarField from "./StarField";
import BadgeViewer from "./BadgeViewer";

export default function Hero() {
  const ref = useRef<HTMLDivElement>(null);
  const { scrollYProgress } = useScroll({
    target: ref,
    offset: ["start start", "end start"],
  });

  const badgeScale = useTransform(scrollYProgress, [0, 1], [1, 1.3]);
  const badgeY = useTransform(scrollYProgress, [0, 1], [0, -30]);
  const contentOpacity = useTransform(scrollYProgress, [0, 0.6], [1, 0]);

  return (
    <section
      id="top"
      ref={ref}
      className="relative flex min-h-screen flex-col items-center justify-center overflow-hidden bg-black"
    >
      <div className="grid-overlay absolute inset-0 opacity-40" />
      <StarField density={160} shootingStars />
      <div className="pointer-events-none absolute inset-0 bg-gradient-to-b from-black/0 via-black/0 to-black" />

      <motion.div
        style={{ opacity: contentOpacity }}
        className="relative z-10 mx-auto flex max-w-5xl flex-col items-center px-6 pt-20 text-center"
      >
        <motion.div
          initial={{ opacity: 0, y: -10 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.6 }}
          className="mono mb-6 flex max-w-[92vw] items-center gap-2 rounded-full border border-white/10 px-3 py-1.5 text-center text-[9px] tracking-widest text-white/50 sm:max-w-none sm:px-4 sm:text-[11px]"
        >
          <span className="h-1.5 w-1.5 shrink-0 animate-pulse rounded-full bg-accent" />
          <span className="whitespace-normal sm:whitespace-nowrap">
            DESIGNED &amp; DEVELOPED BY bi0s HARDWARE
          </span>
        </motion.div>

        <motion.div
          style={{ scale: badgeScale, y: badgeY }}
          initial={{ opacity: 0, y: 30 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.9, ease: "easeOut" }}
        >
          <BadgeViewer widthClassName="w-[62vw] max-w-[260px] sm:max-w-[360px] lg:max-w-[480px]" />
        </motion.div>

        <motion.h1
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.7, delay: 0.2 }}
          className="mt-4 text-5xl font-semibold tracking-tight text-white sm:text-7xl"
        >
          Vanguard
        </motion.h1>
        <motion.p
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.7, delay: 0.3 }}
          className="mono mt-3 text-sm tracking-[0.25em] text-accent sm:text-base"
        >
          PORTABLE HARDWARE SECURITY LABORATORY
        </motion.p>

        <motion.p
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.7, delay: 0.4 }}
          className="mt-8 max-w-xl text-balance text-base leading-relaxed text-white/60 sm:text-lg"
        >
          Hardware security starts with hardware.
        </motion.p>

        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.7, delay: 0.5 }}
          className="mt-10 flex flex-wrap items-center justify-center gap-4"
        >
          <a
            href="#get-vanguard"
            className="mono rounded-sm bg-accent px-7 py-3.5 text-xs tracking-wider text-black transition-transform hover:scale-[1.03]"
          >
            GET A BADGE
          </a>
          <a
            href="#hardware"
            className="mono rounded-sm border border-white/20 px-7 py-3.5 text-xs tracking-wider text-white transition-colors hover:border-technical hover:text-technical"
          >
            EXPLORE THE PLATFORM
          </a>
        </motion.div>
      </motion.div>
    </section>
  );
}
