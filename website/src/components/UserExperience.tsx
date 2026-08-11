"use client";

import { motion } from "framer-motion";
import DeviceFrame from "./DeviceFrame";
import { asset } from "@/lib/basePath";

const screens = [
  { src: asset("/images/home-screen.webp"), alt: "Vanguard home screen", label: "Home Screen" },
  { src: asset("/images/main-menu.webp"), alt: "Vanguard main menu", label: "Main Menu" },
  { src: asset("/images/profile-setup.webp"), alt: "Vanguard profile setup screen", label: "Profile Setup" },
];

export default function UserExperience() {
  return (
    <section id="ux" className="relative border-t border-border py-24 lg:py-32">
      <div className="mx-auto max-w-7xl px-6 lg:px-12">
        <motion.div
          initial={{ opacity: 0, y: 24 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.6 }}
          className="mx-auto max-w-2xl text-center"
        >
          <span className="mono text-xs tracking-[0.2em] text-technical">
            USER EXPERIENCE
          </span>
          <h2 className="mt-4 text-3xl font-semibold tracking-tight text-white sm:text-4xl">
            Straightforward navigation, start to finish
          </h2>
          <p className="mt-5 text-base leading-relaxed text-white/60">
            One consistent menu convention across every feature — boot lands
            on the home screen, every path leads back to it.
          </p>
        </motion.div>

        <div className="mt-16 grid grid-cols-1 gap-10 sm:grid-cols-3">
          {screens.map((s, i) => (
            <motion.div
              key={s.label}
              initial={{ opacity: 0, y: 20 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true, margin: "-60px" }}
              transition={{ duration: 0.5, delay: i * 0.1 }}
            >
              <DeviceFrame src={s.src} alt={s.alt} />
              <p className="mono mt-6 text-center text-xs tracking-widest text-white/40">
                {s.label.toUpperCase()}
              </p>
            </motion.div>
          ))}
        </div>
      </div>
    </section>
  );
}
