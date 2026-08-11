"use client";

import { motion } from "framer-motion";
import DeviceFrame from "./DeviceFrame";
import { asset } from "@/lib/basePath";

export default function EmbeddedApps() {
  return (
    <section id="applications" className="relative border-t border-border py-24 lg:py-32">
      <div className="mx-auto max-w-7xl px-6 lg:px-12">
        <motion.div
          initial={{ opacity: 0, y: 24 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.6 }}
          className="mx-auto max-w-2xl text-center"
        >
          <span className="mono text-xs tracking-[0.2em] text-technical">
            EMBEDDED APPLICATIONS
          </span>
          <h2 className="mt-4 text-3xl font-semibold tracking-tight text-white sm:text-4xl">
            Platform capabilities, demonstrated
          </h2>
          <p className="mt-5 text-base leading-relaxed text-white/60">
            The music player and games are not the point of Vanguard — they
            are working examples of what the platform&apos;s UI framework, state
            management, and wireless stack can support. Read the source, and
            they become a starting point for your own applications.
          </p>
        </motion.div>

        <div className="mt-16 grid grid-cols-1 gap-12 sm:grid-cols-2">
          <div>
            <DeviceFrame
              src={asset("/images/music-player.webp")}
              alt="Vanguard cassette-tape music player interface"
            />
            <p className="mono mt-6 text-center text-xs tracking-widest text-white/40">
              MUSIC PLAYER — UI STATE &amp; RENDERING
            </p>
          </div>
          <div>
            <DeviceFrame
              src={asset("/images/game-menu.webp")}
              alt="Vanguard game session menu"
            />
            <p className="mono mt-6 text-center text-xs tracking-widest text-white/40">
              GAMES — LOCAL &amp; NETWORKED STATE
            </p>
          </div>
        </div>
      </div>
    </section>
  );
}
