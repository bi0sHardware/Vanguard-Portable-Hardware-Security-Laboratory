"use client";

import { motion } from "framer-motion";

const capabilities = [
  {
    title: "Hardware Security Lab",
    lines: [
      "Study real hardware.",
      "Analyze firmware behavior.",
      "Investigate communication systems.",
      "Experiment with wireless protocols.",
    ],
  },
  {
    title: "Open Firmware",
    lines: [
      "The complete firmware source is available.",
      "Build it. Flash it. Modify it. Extend it.",
      "Nothing is locked down.",
    ],
  },
  {
    title: "Real Hardware",
    lines: ["ESP32-S3. LoRa. BLE.", "Display. Storage. Audio.", "A complete embedded platform."],
  },
  {
    title: "Real Interaction",
    lines: [
      "Security challenges. LoRa communication.",
      "Radio Chat. Morse mode. Peer discovery.",
      "Everything runs on the device. No emulation.",
    ],
  },
];

export default function WhatVanguardGives() {
  return (
    <section className="relative border-t border-border py-24 lg:py-32">
      <div className="mx-auto max-w-6xl px-6 lg:px-12">
        <motion.div
          initial={{ opacity: 0, y: 24 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.6 }}
          className="mx-auto max-w-2xl text-center"
        >
          <span className="mono text-xs tracking-[0.2em] text-technical">
            THE PLATFORM
          </span>
          <h2 className="mt-4 text-3xl font-semibold tracking-tight text-white sm:text-4xl">
            What Vanguard gives you
          </h2>
          <p className="mt-5 text-base leading-relaxed text-white/60">
            A portable hardware security platform, built to be explored,
            modified, and extended. Security challenges, wireless
            communication, and open firmware in one device.
          </p>
        </motion.div>

        <div className="mt-16 grid grid-cols-1 gap-px overflow-hidden rounded-lg border border-border bg-border sm:grid-cols-2">
          {capabilities.map((cap, i) => (
            <motion.div
              key={cap.title}
              initial={{ opacity: 0, y: 16 }}
              whileInView={{ opacity: 1, y: 0 }}
              viewport={{ once: true, margin: "-60px" }}
              transition={{ duration: 0.5, delay: i * 0.08 }}
              className="bg-black p-8"
            >
              <h3 className="text-lg font-semibold text-white">{cap.title}</h3>
              <div className="mt-4 space-y-1.5">
                {cap.lines.map((line) => (
                  <p key={line} className="text-sm text-white/55">
                    {line}
                  </p>
                ))}
              </div>
            </motion.div>
          ))}
        </div>
      </div>
    </section>
  );
}
