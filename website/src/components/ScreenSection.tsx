"use client";

import { motion } from "framer-motion";
import DeviceFrame from "./DeviceFrame";

export type ScreenSectionProps = {
  kicker: string;
  title: string;
  description: string;
  points: string[];
  image: string;
  imageAlt: string;
  reverse?: boolean;
  id?: string;
};

export default function ScreenSection({
  kicker,
  title,
  description,
  points,
  image,
  imageAlt,
  reverse = false,
  id,
}: ScreenSectionProps) {
  return (
    <section id={id} className="relative border-t border-border py-24 lg:py-32">
      <div
        className={`mx-auto grid max-w-7xl grid-cols-1 items-center gap-16 px-6 lg:grid-cols-2 lg:px-12 ${
          reverse ? "lg:[&>*:first-child]:order-2" : ""
        }`}
      >
        <motion.div
          initial={{ opacity: 0, y: 24 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.6, ease: "easeOut" }}
        >
          <span className="mono text-xs tracking-[0.2em] text-technical">
            {kicker}
          </span>
          <h2 className="mt-4 text-3xl font-semibold tracking-tight text-white text-balance lg:text-4xl">
            {title}
          </h2>
          <p className="mt-5 max-w-lg text-base leading-relaxed text-white/60">
            {description}
          </p>
          <ul className="mt-8 space-y-3">
            {points.map((point) => (
              <li key={point} className="flex items-start gap-3 text-sm text-white/70">
                <span className="mt-1.5 h-1.5 w-1.5 shrink-0 rounded-full bg-accent" />
                {point}
              </li>
            ))}
          </ul>
        </motion.div>

        <DeviceFrame src={image} alt={imageAlt} />
      </div>
    </section>
  );
}
