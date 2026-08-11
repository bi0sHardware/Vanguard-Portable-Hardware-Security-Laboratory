"use client";

import { motion } from "framer-motion";

const resources = [
  { label: "Firmware Source", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/tree/main/firmware" },
  { label: "Documentation", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/tree/main/docs" },
  { label: "Build Instructions", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/blob/main/DEVELOPMENT.md" },
  { label: "Development Guides", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/blob/main/DEVELOPMENT.md" },
  { label: "Architecture References", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/blob/main/ARCHITECTURE.md" },
];

export default function OpenFirmware() {
  return (
    <section id="firmware" className="relative border-t border-border py-24 lg:py-32">
      <div className="mx-auto max-w-5xl px-6 text-center lg:px-12">
        <motion.div
          initial={{ opacity: 0, y: 24 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.6 }}
        >
          <span className="mono text-xs tracking-[0.2em] text-technical">
            OPEN FIRMWARE
          </span>
          <h2 className="mt-4 text-3xl font-semibold tracking-tight text-white sm:text-5xl">
            Complete source code available.
          </h2>
          <p className="mx-auto mt-6 max-w-2xl text-base leading-relaxed text-white/60 sm:text-lg">
            Firmware source, documentation, build instructions, development
            guides, and architecture references — all in the same
            repository the shipped firmware is built from.
          </p>
        </motion.div>

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
            href="https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory"
            target="_blank"
            rel="noreferrer"
            className="mono rounded-sm border border-white/20 px-6 py-3 text-xs tracking-wider text-white transition-colors hover:border-white/40"
          >
            VIEW REPOSITORY
          </a>
          <a
            href="https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/wiki"
            target="_blank"
            rel="noreferrer"
            className="mono rounded-sm border border-white/20 px-6 py-3 text-xs tracking-wider text-white transition-colors hover:border-technical hover:text-technical"
          >
            DOCUMENTATION
          </a>
        </div>

        <div className="mt-14 grid grid-cols-1 gap-px overflow-hidden rounded-lg border border-border bg-border sm:grid-cols-2 lg:grid-cols-5">
          {resources.map((r) => (
            <a
              key={r.label}
              href={r.href}
              target="_blank"
              rel="noreferrer"
              className="group bg-black p-5 text-left transition-colors hover:bg-white/[0.03]"
            >
              <span className="mono text-xs text-white/70 group-hover:text-technical">
                {r.label}
              </span>
            </a>
          ))}
        </div>
      </div>
    </section>
  );
}
