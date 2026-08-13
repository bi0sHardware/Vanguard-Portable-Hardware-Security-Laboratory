"use client";

import Image from "next/image";
import { motion, AnimatePresence } from "framer-motion";
import { asset } from "@/lib/basePath";
import { displayBoxPercent } from "@/lib/badgeDisplay";

const box = displayBoxPercent();

/**
 * Renders the real badge photo with a firmware screenshot composited
 * onto its actual display region (measured against the photo, not a
 * generic device-frame mockup). `screenshot` swaps with a crossfade;
 * pass null to show the display off/dark.
 */
export default function BadgeScreen({
  screenshot,
  screenshotAlt = "",
  widthClassName = "w-[85vw] max-w-[360px] sm:max-w-[440px] lg:max-w-[520px]",
  className = "",
}: {
  screenshot: string | null;
  screenshotAlt?: string;
  widthClassName?: string;
  className?: string;
}) {
  return (
    <div
      className={`relative mx-auto ${widthClassName} ${className}`}
      style={{ aspectRatio: "916 / 1045" }}
    >
      <div className="absolute inset-0 -z-10 rounded-full bg-accent/15 blur-[80px]" />

      <Image
        src={asset("/images/badge-front-cutout.webp")}
        alt="Vanguard badge"
        fill
        sizes="440px"
        className="object-contain drop-shadow-[0_30px_60px_rgba(255,107,0,0.15)]"
        priority
      />

      <div
        className="absolute overflow-hidden rounded-[2px] bg-black"
        style={{
          left: `${box.left}%`,
          top: `${box.top}%`,
          width: `${box.width}%`,
          height: `${box.height}%`,
        }}
      >
        <AnimatePresence mode="wait">
          {screenshot && (
            <motion.div
              key={screenshot}
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
              transition={{ duration: 0.35 }}
              className="absolute inset-0"
            >
              <Image
                src={screenshot}
                alt={screenshotAlt}
                fill
                sizes="260px"
                className="object-contain"
              />
            </motion.div>
          )}
        </AnimatePresence>
      </div>
    </div>
  );
}
