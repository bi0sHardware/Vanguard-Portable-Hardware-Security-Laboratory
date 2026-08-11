"use client";

import Image from "next/image";
import { motion, useScroll, useTransform } from "framer-motion";
import { useRef } from "react";
import StarField from "./StarField";
import { asset } from "@/lib/basePath";

export default function Hero() {
  const ref = useRef<HTMLDivElement>(null);
  const { scrollYProgress } = useScroll({
    target: ref,
    offset: ["start start", "end start"],
  });

  const badgeScale = useTransform(scrollYProgress, [0, 1], [1, 1.35]);
  const badgeRotate = useTransform(scrollYProgress, [0, 1], [0, 6]);
  const badgeY = useTransform(scrollYProgress, [0, 1], [0, -40]);
  const contentOpacity = useTransform(scrollYProgress, [0, 0.6], [1, 0]);

  return (
    <section
      id="top"
      ref={ref}
      className="relative flex min-h-screen flex-col items-center justify-center overflow-hidden bg-black"
    >
      <div className="grid-overlay absolute inset-0 opacity-40" />
      <StarField density={160} />
      <div className="pointer-events-none absolute inset-0 bg-gradient-to-b from-black/0 via-black/0 to-black" />

      <motion.div
        style={{ opacity: contentOpacity }}
        className="relative z-10 mx-auto flex max-w-5xl flex-col items-center px-6 pt-24 text-center"
      >
        <motion.div
          initial={{ opacity: 0, y: -10 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.6 }}
          className="mono mb-8 flex items-center gap-2 rounded-full border border-white/10 px-4 py-1.5 text-[11px] tracking-widest text-white/50"
        >
          <span className="h-1.5 w-1.5 animate-pulse rounded-full bg-accent" />
          bi0s HARDWARE
        </motion.div>

        <motion.div
          style={{ scale: badgeScale, rotate: badgeRotate, y: badgeY }}
          initial={{ opacity: 0, y: 30 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.9, ease: "easeOut" }}
          className="relative mb-10 h-[260px] w-[200px] sm:h-[340px] sm:w-[260px]"
        >
          <div className="absolute inset-0 -z-10 rounded-full bg-accent/20 blur-[80px]" />
          <Image
            src={asset("/images/badge-front.jpeg")}
            alt="Vanguard hardware security badge"
            fill
            priority
            sizes="260px"
            className="object-contain drop-shadow-[0_0_60px_rgba(255,107,0,0.15)]"
          />
        </motion.div>

        <motion.h1
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.7, delay: 0.2 }}
          className="text-5xl font-semibold tracking-tight text-white sm:text-7xl"
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
          className="mt-8 max-w-2xl text-balance text-base leading-relaxed text-white/60 sm:text-lg"
        >
          Explore hardware security, embedded systems, wireless
          communication, RF experimentation, and cybersecurity challenges
          from a single platform.
        </motion.p>

        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.7, delay: 0.5 }}
          className="mt-10 flex flex-wrap items-center justify-center gap-4"
        >
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
            href="https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory"
            target="_blank"
            rel="noreferrer"
            className="mono rounded-sm border border-white/20 px-6 py-3 text-xs tracking-wider text-white transition-colors hover:border-white/40"
          >
            GITHUB
          </a>
          <a
            href="#get-vanguard"
            className="mono rounded-sm border border-white/20 px-6 py-3 text-xs tracking-wider text-white transition-colors hover:border-white/40"
          >
            REQUEST BADGE
          </a>
        </motion.div>
      </motion.div>

      <motion.div
        initial={{ opacity: 0 }}
        animate={{ opacity: 1 }}
        transition={{ delay: 1, duration: 0.6 }}
        className="absolute bottom-8 left-1/2 z-10 -translate-x-1/2"
      >
        <div className="mono flex flex-col items-center gap-2 text-[10px] tracking-widest text-white/30">
          SCROLL
          <span className="h-8 w-px animate-pulse bg-white/20" />
        </div>
      </motion.div>
    </section>
  );
}
