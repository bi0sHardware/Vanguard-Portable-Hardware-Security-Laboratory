"use client";

import Image from "next/image";
import { motion } from "framer-motion";

export default function DeviceFrame({
  src,
  alt,
}: {
  src: string;
  alt: string;
}) {
  return (
    <motion.div
      initial={{ opacity: 0, scale: 0.94, y: 20 }}
      whileInView={{ opacity: 1, scale: 1, y: 0 }}
      viewport={{ once: true, margin: "-80px" }}
      transition={{ duration: 0.6, ease: "easeOut" }}
      className="relative mx-auto w-full max-w-md"
    >
      {/* bezel */}
      <div className="relative rounded-[28px] border border-white/10 bg-[#0a0a0a] p-3 shadow-[0_0_60px_-15px_rgba(255,107,0,0.25)]">
        <div className="flex items-center justify-between px-2 pb-2">
          <div className="flex gap-1.5">
            <span className="h-1.5 w-1.5 rounded-full bg-accent/70" />
            <span className="h-1.5 w-1.5 rounded-full bg-white/20" />
          </div>
          <span className="mono text-[9px] tracking-widest text-white/30">
            VANGUARD DISPLAY
          </span>
        </div>
        <div className="relative aspect-[4/3] w-full overflow-hidden rounded-[16px] border border-white/10 bg-black">
          <Image
            src={src}
            alt={alt}
            fill
            sizes="(max-width: 768px) 90vw, 420px"
            className="object-cover"
          />
          {/* subtle scanline */}
          <div
            className="pointer-events-none absolute inset-0 opacity-[0.06]"
            style={{
              backgroundImage:
                "repeating-linear-gradient(to bottom, #fff 0px, #fff 1px, transparent 1px, transparent 3px)",
            }}
          />
        </div>
      </div>
    </motion.div>
  );
}
