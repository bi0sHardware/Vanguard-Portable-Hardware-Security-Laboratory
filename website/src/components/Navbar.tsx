"use client";

import Image from "next/image";
import { useEffect, useState } from "react";
import { asset } from "@/lib/basePath";

const links = [
  { label: "Documentation", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/tree/main/docs" },
  { label: "Firmware", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/tree/main/firmware" },
  { label: "GitHub", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory" },
  { label: "bi0s Hardware", href: "https://www.bi0shardware.in/" },
];

export default function Navbar() {
  const [scrolled, setScrolled] = useState(false);

  useEffect(() => {
    const onScroll = () => setScrolled(window.scrollY > 24);
    window.addEventListener("scroll", onScroll, { passive: true });
    return () => window.removeEventListener("scroll", onScroll);
  }, []);

  return (
    <header
      className={`fixed top-0 left-0 right-0 z-50 transition-all duration-300 ${
        scrolled ? "bg-black/80 backdrop-blur-md border-b border-border" : "bg-transparent"
      }`}
    >
      <nav className="mx-auto flex max-w-7xl items-center justify-between px-6 py-4 lg:px-12">
        <a href="#top" className="flex items-center gap-3">
          <Image
            src={asset("/images/bi0s-hardware-logo.webp")}
            alt="bi0s Hardware"
            width={28}
            height={28}
            className="invert"
          />
          <span className="mono text-sm tracking-widest text-white/80">
            bi0s HARDWARE
          </span>
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

        <a
          href="#get-vanguard"
          className="mono rounded-sm border border-accent px-4 py-2 text-xs tracking-wider text-accent transition-colors hover:bg-accent hover:text-black"
        >
          GET A BADGE
        </a>
      </nav>
    </header>
  );
}
