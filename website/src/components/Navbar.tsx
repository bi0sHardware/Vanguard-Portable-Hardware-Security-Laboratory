"use client";

import Image from "next/image";
import { useEffect, useState } from "react";
import { AnimatePresence, motion } from "framer-motion";
import { asset } from "@/lib/basePath";

const links = [
  { label: "Documentation", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/tree/main/docs" },
  { label: "Firmware", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/tree/main/firmware" },
  { label: "GitHub", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory" },
  { label: "bi0s Hardware", href: "https://www.bi0shardware.in/" },
];

export default function Navbar() {
  const [scrolled, setScrolled] = useState(false);
  const [menuOpen, setMenuOpen] = useState(false);

  useEffect(() => {
    const onScroll = () => setScrolled(window.scrollY > 24);
    window.addEventListener("scroll", onScroll, { passive: true });
    return () => window.removeEventListener("scroll", onScroll);
  }, []);

  useEffect(() => {
    document.body.style.overflow = menuOpen ? "hidden" : "";
    return () => {
      document.body.style.overflow = "";
    };
  }, [menuOpen]);

  return (
    <header
      className={`fixed top-0 left-0 right-0 z-50 transition-all duration-300 ${
        scrolled || menuOpen ? "bg-black/80 backdrop-blur-md border-b border-border" : "bg-transparent"
      }`}
    >
      <nav className="mx-auto flex max-w-7xl items-center justify-between px-4 py-4 sm:px-6 lg:px-12">
        <a href="#top" className="flex shrink-0 items-center gap-3" onClick={() => setMenuOpen(false)}>
          <Image
            src={asset("/images/bi0s-hardware-logo.webp")}
            alt="bi0s Hardware"
            width={104}
            height={50}
            priority
            className="h-7 w-auto invert sm:h-9"
          />
        </a>

        <div className="hidden items-center gap-8 md:flex">
          {links.map((link) => (
            <a
              key={link.label}
              href={link.href}
              target="_blank"
              rel="noreferrer"
              className="mono text-xs tracking-wider text-white/60 transition-colors hover:text-accent"
            >
              {link.label.toUpperCase()}
            </a>
          ))}
        </div>

        <div className="flex items-center gap-3">
          <a
            href="#get-vanguard"
            onClick={() => setMenuOpen(false)}
            className="mono rounded-sm border border-accent px-3 py-2 text-[11px] tracking-wider text-accent transition-colors hover:bg-accent hover:text-black sm:px-4 sm:text-xs"
          >
            GET A BADGE
          </a>

          <button
            type="button"
            aria-label="Toggle menu"
            aria-expanded={menuOpen}
            onClick={() => setMenuOpen((v) => !v)}
            className="flex h-9 w-9 shrink-0 flex-col items-center justify-center gap-1.5 rounded-sm border border-white/15 md:hidden"
          >
            <span
              className={`block h-px w-4 bg-white transition-transform ${menuOpen ? "translate-y-[3px] rotate-45" : ""}`}
            />
            <span
              className={`block h-px w-4 bg-white transition-transform ${menuOpen ? "-translate-y-[3px] -rotate-45" : ""}`}
            />
          </button>
        </div>
      </nav>

      <AnimatePresence>
        {menuOpen && (
          <motion.div
            initial={{ opacity: 0, height: 0 }}
            animate={{ opacity: 1, height: "auto" }}
            exit={{ opacity: 0, height: 0 }}
            transition={{ duration: 0.25 }}
            className="overflow-hidden border-t border-border md:hidden"
          >
            <div className="flex flex-col gap-1 px-4 py-4 sm:px-6">
              {links.map((link) => (
                <a
                  key={link.label}
                  href={link.href}
                  target="_blank"
                  rel="noreferrer"
                  onClick={() => setMenuOpen(false)}
                  className="mono rounded-sm px-2 py-3 text-xs tracking-wider text-white/70 transition-colors hover:bg-white/5 hover:text-accent"
                >
                  {link.label.toUpperCase()}
                </a>
              ))}
            </div>
          </motion.div>
        )}
      </AnimatePresence>
    </header>
  );
}
