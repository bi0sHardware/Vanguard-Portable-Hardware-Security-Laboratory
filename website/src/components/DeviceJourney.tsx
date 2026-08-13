"use client";

import { useEffect, useRef, useState } from "react";
import Image from "next/image";
import { motion, AnimatePresence } from "framer-motion";
import gsap from "gsap";
import { ScrollTrigger } from "gsap/ScrollTrigger";
import { asset } from "@/lib/basePath";
import { displayBoxPercent } from "@/lib/badgeDisplay";

const box = displayBoxPercent();

type Callout = { label: string; x: string; y: string };

type Step = {
  kicker: string;
  title: string;
  lines: string[];
  screenshot: string | null;
  screenshotAlt: string;
};

const steps: Step[] = [
  {
    kicker: "HARDWARE",
    title: "Real hardware.",
    lines: ["ESP32-S3.", "WiFi. BLE. LoRa.", "TFT display."],
    screenshot: null,
    screenshotAlt: "",
  },
  {
    kicker: "HOME SCREEN",
    title: "Boots here.",
    lines: ["Every screen leads back to it."],
    screenshot: "/images/home-screen.webp",
    screenshotAlt: "Vanguard home screen",
  },
  {
    kicker: "MAIN MENU",
    title: "One menu.",
    lines: ["Challenges. Radio Chat. PeerDrop.", "Games. Settings. Contacts."],
    screenshot: "/images/main-menu.webp",
    screenshotAlt: "Vanguard main menu",
  },
  {
    kicker: "CHALLENGES",
    title: "Investigate. Analyze. Solve.",
    lines: ["Structured hardware and firmware security exercises.", "Not games. Not quizzes."],
    screenshot: "/images/challenges-menu.webp",
    screenshotAlt: "Vanguard challenges menu",
  },
  {
    kicker: "RADIO CHAT",
    title: "LoRa. Badge to badge.",
    lines: ["Messaging. Morse mode.", "Peer-to-peer, over real radio."],
    screenshot: "/images/radio-chat.webp",
    screenshotAlt: "Vanguard Radio Chat screen",
  },
];

// Real, visible-on-the-photo landmarks only — ESP32-S3, WiFi, BLE, and
// LoRa all live on the back of the board (the module + radio are
// physically there), so the hardware step shows the back, not the front.
const backCallouts: Callout[] = [
  { label: "LoRa Radio", x: "50%", y: "29%" },
  { label: "ESP32-S3 · WiFi · BLE", x: "77%", y: "62%" },
  { label: "Li-ion Battery", x: "50%", y: "58%" },
  { label: "RST / BOOT", x: "77%", y: "72%" },
  { label: "USB-C", x: "50%", y: "90%" },
];

const frontCallouts: Callout[] = [
  { label: "TFT Display", x: "50%", y: "62%" },
  { label: "Joystick Input", x: "16%", y: "74%" },
  { label: "Button Cluster", x: "85%", y: "72%" },
  { label: "LED Zones", x: "50%", y: "76%" },
];

export default function DeviceJourney() {
  const trackRef = useRef<HTMLDivElement>(null);
  const pinRef = useRef<HTMLDivElement>(null);
  const [stepIndex, setStepIndex] = useState(0);
  const [flipRotation, setFlipRotation] = useState(0);

  useEffect(() => {
    gsap.registerPlugin(ScrollTrigger);
    const ctx = gsap.context(() => {
      if (!trackRef.current || !pinRef.current) return;
      ScrollTrigger.create({
        trigger: trackRef.current,
        start: "top top",
        end: "bottom bottom",
        pin: pinRef.current,
        scrub: 0.6,
        onUpdate: (self) => {
          const idx = Math.min(steps.length - 1, Math.floor(self.progress * steps.length));
          setStepIndex(idx);
          // Flip from back (0deg) to front (180deg) across the first HALF
          // of step 0's range, leaving the second half to show front-side
          // callouts before the display sequence (step 1+) takes over.
          const segment = 1 / steps.length;
          const flipProgress = Math.min(1, self.progress / (segment * 0.5));
          setFlipRotation(flipProgress * 180);
        },
      });
    });
    return () => ctx.revert();
  }, []);

  const step = steps[stepIndex];
  const onDisplay = stepIndex > 0;

  return (
    <section ref={trackRef} className="relative" style={{ height: `${steps.length * 100}vh` }}>
      <div ref={pinRef} className="relative flex h-screen items-center overflow-hidden bg-black">
        <div className="grid-overlay absolute inset-0 opacity-20" />

        <div className="relative mx-auto grid w-full max-w-7xl grid-cols-1 items-center gap-6 px-6 lg:grid-cols-[0.85fr_1.15fr] lg:gap-4 lg:px-12">
          {/* text panel */}
          <div className="order-2 lg:order-1">
            <AnimatePresence mode="wait">
              <motion.div
                key={stepIndex}
                initial={{ opacity: 0, y: 16 }}
                animate={{ opacity: 1, y: 0 }}
                exit={{ opacity: 0, y: -16 }}
                transition={{ duration: 0.4 }}
              >
                <span className="mono text-xs tracking-[0.2em] text-technical">
                  {step.kicker}
                </span>
                <h2 className="mt-3 text-3xl font-semibold tracking-tight text-white sm:text-4xl">
                  {step.title}
                </h2>
                <div className="mt-4 space-y-1">
                  {step.lines.map((line) => (
                    <p key={line} className="text-base text-white/60">
                      {line}
                    </p>
                  ))}
                </div>
              </motion.div>
            </AnimatePresence>

            <div className="mono mt-10 flex gap-1.5">
              {steps.map((s, i) => (
                <span
                  key={s.kicker}
                  className={`h-1 rounded-full transition-all duration-300 ${
                    i === stepIndex ? "w-8 bg-accent" : "w-3 bg-white/15"
                  }`}
                />
              ))}
            </div>
          </div>

          {/* badge + floating screen stage */}
          <div className="relative order-1 flex h-[70vh] items-center justify-center lg:order-2 lg:h-[80vh]">
            <motion.div
              animate={{
                x: onDisplay ? "-28%" : "0%",
                scale: onDisplay ? 0.62 : 1,
              }}
              transition={{ duration: 0.6, ease: "easeInOut" }}
              className="absolute z-10 w-[70vw] max-w-[360px] sm:max-w-[420px] lg:max-w-[480px]"
              style={{ aspectRatio: "916 / 1045", perspective: 1400 }}
            >
              <div className="absolute inset-0 -z-10 rounded-full bg-accent/15 blur-[80px]" />

              <div
                className="relative h-full w-full"
                style={{ transform: `rotateY(${flipRotation}deg)`, transformStyle: "preserve-3d" }}
              >
                {/* back face — hardware callouts */}
                <div
                  className="absolute inset-0"
                  style={{
                    backfaceVisibility: "hidden",
                    WebkitBackfaceVisibility: "hidden",
                    // Belt-and-suspenders on top of backface-visibility:
                    // some browsers ghost the hidden face under nested
                    // transforms (a parent x/scale animation + a 3D flip
                    // context), so explicitly hide it past the midpoint
                    // instead of trusting backface-visibility alone.
                    opacity: flipRotation < 90 ? 1 : 0,
                    visibility: flipRotation < 90 ? "visible" : "hidden",
                  }}
                >
                  <Image
                    src={asset("/images/badge-back-cutout.webp")}
                    alt="Vanguard badge, back"
                    fill
                    sizes="480px"
                    className="object-contain drop-shadow-[0_30px_60px_rgba(255,107,0,0.15)]"
                    priority
                  />
                  {stepIndex === 0 &&
                    backCallouts.map((c) => (
                      <motion.div
                        key={c.label}
                        initial={{ opacity: 0 }}
                        animate={{ opacity: 1 }}
                        transition={{ duration: 0.4, delay: 0.15 }}
                        className="absolute flex -translate-x-1/2 -translate-y-1/2 items-center gap-2"
                        style={{ left: c.x, top: c.y }}
                      >
                        <span className="h-1.5 w-1.5 shrink-0 rounded-full border border-accent bg-accent/60" />
                        <span className="mono whitespace-nowrap rounded-sm border border-white/10 bg-black/80 px-2 py-1 text-[9px] tracking-wide text-white/80 backdrop-blur-sm">
                          {c.label}
                        </span>
                      </motion.div>
                    ))}
                </div>

                {/* front face — the display */}
                <div
                  className="absolute inset-0"
                  style={{
                    backfaceVisibility: "hidden",
                    WebkitBackfaceVisibility: "hidden",
                    transform: "rotateY(180deg)",
                    opacity: flipRotation >= 90 ? 1 : 0,
                    visibility: flipRotation >= 90 ? "visible" : "hidden",
                  }}
                >
                  <Image
                    src={asset("/images/badge-front-cutout.webp")}
                    alt="Vanguard badge, front"
                    fill
                    sizes="480px"
                    className="object-contain drop-shadow-[0_30px_60px_rgba(255,107,0,0.15)]"
                  />
                  {!onDisplay && (
                    <div
                      className="absolute overflow-hidden rounded-[2px] bg-black"
                      style={{
                        left: `${box.left}%`,
                        top: `${box.top}%`,
                        width: `${box.width}%`,
                        height: `${box.height}%`,
                      }}
                    />
                  )}
                  {!onDisplay &&
                    frontCallouts.map((c) => (
                      <motion.div
                        key={c.label}
                        initial={{ opacity: 0 }}
                        animate={{ opacity: flipRotation >= 90 ? 1 : 0 }}
                        transition={{ duration: 0.4, delay: 0.15 }}
                        className="absolute flex -translate-x-1/2 -translate-y-1/2 items-center gap-2"
                        style={{ left: c.x, top: c.y }}
                      >
                        <span className="h-1.5 w-1.5 shrink-0 rounded-full border border-accent bg-accent/60" />
                        <span className="mono whitespace-nowrap rounded-sm border border-white/10 bg-black/80 px-2 py-1 text-[9px] tracking-wide text-white/80 backdrop-blur-sm">
                          {c.label}
                        </span>
                      </motion.div>
                    ))}
                </div>
              </div>
            </motion.div>

            {/* floating popped-out screen: large, legible, takes over once past step 0 */}
            <AnimatePresence mode="wait">
              {onDisplay && step.screenshot && (
                <motion.div
                  key={step.screenshot}
                  initial={{ opacity: 0, scale: 0.7 }}
                  animate={{ opacity: 1, scale: 1 }}
                  exit={{ opacity: 0, scale: 0.85 }}
                  transition={{ duration: 0.5, ease: "easeOut" }}
                  className="absolute right-0 z-20 w-[70vw] max-w-[260px] sm:max-w-[380px] sm:right-[-4%] lg:max-w-[440px] lg:right-[-8%]"
                >
                  <div className="relative rounded-[20px] border border-white/10 bg-[#0a0a0a] p-2.5 shadow-[0_0_60px_-10px_rgba(255,107,0,0.3)]">
                    <div className="relative aspect-[900/700] w-full overflow-hidden rounded-[12px] border border-white/10 bg-black">
                      <Image
                        src={asset(step.screenshot)}
                        alt={step.screenshotAlt}
                        fill
                        sizes="440px"
                        className="object-contain"
                      />
                    </div>
                  </div>
                  <p className="mono mt-4 text-center text-[10px] tracking-widest text-white/30">
                    VANGUARD DISPLAY
                  </p>
                </motion.div>
              )}
            </AnimatePresence>
          </div>
        </div>
      </div>
    </section>
  );
}
