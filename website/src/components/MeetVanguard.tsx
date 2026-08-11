"use client";

import Image from "next/image";
import { motion } from "framer-motion";
import { asset } from "@/lib/basePath";
import {
  ChipIcon,
  RadioIcon,
  BluetoothIcon,
  DisplayIcon,
  AudioIcon,
  LedIcon,
  StorageIcon,
  BatteryIcon,
} from "./icons";

type Callout = {
  label: string;
  x: string;
  y: string;
};

const frontCallouts: Callout[] = [
  { label: "LoRa Antenna", x: "50%", y: "4%" },
  { label: "TFT Display", x: "53%", y: "58%" },
  { label: "Joystick Input", x: "17%", y: "77%" },
  { label: "LED Zones", x: "65%", y: "77%" },
  { label: "Button Cluster", x: "92%", y: "73%" },
];

const backCallouts: Callout[] = [
  { label: "LoRa Radio Module", x: "52%", y: "26%" },
  { label: "ESP32-S3-WROOM-1", x: "75%", y: "62%" },
  { label: "Li-ion Cell", x: "50%", y: "58%" },
  { label: "RST / BOOT", x: "74%", y: "76%" },
  { label: "USB-C", x: "52%", y: "92%" },
];

function CalloutImage({
  src,
  alt,
  callouts,
}: {
  src: string;
  alt: string;
  callouts: Callout[];
}) {
  return (
    <div className="relative mx-auto aspect-[4/5] w-full max-w-[420px]">
      <Image src={src} alt={alt} fill sizes="420px" className="object-contain" />
      {callouts.map((c) => (
        <motion.div
          key={c.label}
          initial={{ opacity: 0 }}
          whileInView={{ opacity: 1 }}
          viewport={{ once: true }}
          transition={{ duration: 0.5 }}
          className="absolute flex -translate-x-1/2 -translate-y-1/2 items-center gap-2"
          style={{ left: c.x, top: c.y }}
        >
          <span className="h-2 w-2 shrink-0 rounded-full border border-accent bg-accent/60" />
          <span className="mono whitespace-nowrap rounded-sm border border-white/10 bg-black/80 px-2 py-1 text-[9px] tracking-wide text-white/80 backdrop-blur-sm">
            {c.label}
          </span>
        </motion.div>
      ))}
    </div>
  );
}

const specs = [
  { label: "ESP32-S3", Icon: ChipIcon },
  { label: "LoRa", Icon: RadioIcon },
  { label: "BLE", Icon: BluetoothIcon },
  { label: "TFT Display", Icon: DisplayIcon },
  { label: "Audio", Icon: AudioIcon },
  { label: "Addressable LEDs", Icon: LedIcon },
  { label: "NVS Storage", Icon: StorageIcon },
  { label: "Battery Powered", Icon: BatteryIcon },
];

export default function MeetVanguard() {
  return (
    <section id="hardware" className="relative border-t border-border py-24 lg:py-32">
      <div className="mx-auto max-w-7xl px-6 lg:px-12">
        <motion.div
          initial={{ opacity: 0, y: 24 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.6 }}
          className="mx-auto max-w-2xl text-center"
        >
          <span className="mono text-xs tracking-[0.2em] text-technical">
            HARDWARE
          </span>
          <h2 className="mt-4 text-3xl font-semibold tracking-tight text-white sm:text-4xl">
            Meet Vanguard
          </h2>
          <p className="mt-5 text-base leading-relaxed text-white/60">
            A custom PCB built around the ESP32-S3, with the board itself
            doubling as the enclosure. No case, no shell — the hardware is
            the product.
          </p>
        </motion.div>

        <div className="mt-16 grid grid-cols-1 gap-16 sm:grid-cols-2">
          <div>
            <CalloutImage
              src={asset("/images/badge-front-cutout.webp")}
              alt="Vanguard badge, front"
              callouts={frontCallouts}
            />
            <p className="mono mt-6 text-center text-xs tracking-widest text-white/40">
              FRONT
            </p>
          </div>
          <div>
            <CalloutImage
              src={asset("/images/badge-back-cutout.webp")}
              alt="Vanguard badge, back"
              callouts={backCallouts}
            />
            <p className="mono mt-6 text-center text-xs tracking-widest text-white/40">
              BACK
            </p>
          </div>
        </div>

        <motion.div
          initial={{ opacity: 0, y: 16 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true }}
          transition={{ duration: 0.6 }}
          className="mx-auto mt-20 flex max-w-4xl flex-wrap justify-center gap-3"
        >
          {specs.map(({ label, Icon }) => (
            <span
              key={label}
              className="mono flex items-center gap-2 rounded-full border border-white/10 px-4 py-2 text-xs tracking-wide text-white/70 transition-colors hover:border-accent/40 hover:text-white"
            >
              <Icon className="text-accent" />
              {label}
            </span>
          ))}
        </motion.div>
      </div>
    </section>
  );
}
