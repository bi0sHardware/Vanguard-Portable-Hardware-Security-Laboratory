import Image from "next/image";
import { asset } from "@/lib/basePath";

const columns = [
  {
    title: "Platform",
    links: [
      { label: "Firmware Source", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/tree/main/firmware" },
      { label: "Architecture", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/blob/main/ARCHITECTURE.md" },
      { label: "Hardware", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/tree/main/hardware" },
    ],
  },
  {
    title: "Documentation",
    links: [
      { label: "Wiki", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/wiki" },
      { label: "Build Guide", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/blob/main/DEVELOPMENT.md" },
      { label: "Deployment", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/blob/main/DEPLOYMENT.md" },
    ],
  },
  {
    title: "Project",
    links: [
      { label: "GitHub", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory" },
      { label: "bi0s Hardware", href: "https://github.com/bi0sHardware" },
      { label: "License (MIT)", href: "https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory/blob/main/LICENSE" },
    ],
  },
];

export default function Footer() {
  return (
    <footer className="relative border-t border-border bg-black py-16">
      <div className="mx-auto max-w-7xl px-6 lg:px-12">
        <div className="grid grid-cols-1 gap-12 sm:grid-cols-2 lg:grid-cols-4">
          <div>
            <div className="flex items-center gap-3">
              <Image
                src={asset("/images/bi0s-hardware-logo.webp")}
                alt="bi0s Hardware"
                width={32}
                height={32}
                className="invert"
              />
              <span className="mono text-sm tracking-widest text-white/80">
                bi0s HARDWARE
              </span>
            </div>
            <p className="mt-4 max-w-xs text-xs leading-relaxed text-white/40">
              Vanguard — Portable Hardware Security Laboratory. Built on the
              ESP32-S3.
            </p>
          </div>

          {columns.map((col) => (
            <div key={col.title}>
              <h4 className="mono text-xs tracking-widest text-white/40">
                {col.title.toUpperCase()}
              </h4>
              <ul className="mt-4 space-y-3">
                {col.links.map((link) => (
                  <li key={link.label}>
                    <a
                      href={link.href}
                      target="_blank"
                      rel="noreferrer"
                      className="text-sm text-white/60 transition-colors hover:text-accent"
                    >
                      {link.label}
                    </a>
                  </li>
                ))}
              </ul>
            </div>
          ))}
        </div>

        <div className="mt-16 flex flex-col items-center justify-between gap-4 border-t border-border pt-8 sm:flex-row">
          <p className="mono text-xs text-white/30">
            Vanguard — Portable Hardware Security Laboratory
          </p>
          <p className="mono text-xs text-white/30">MIT Licensed</p>
        </div>
      </div>
    </footer>
  );
}
