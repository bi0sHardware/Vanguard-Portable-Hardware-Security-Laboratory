"use client";

import Image from "next/image";
import { useRef, useState } from "react";
import { motion, useMotionValue, useSpring, useTransform } from "framer-motion";
import { asset } from "@/lib/basePath";

const CLICK_THRESHOLD_PX = 6;

/**
 * Interactive badge viewer: drag horizontally (or click/tap) to flip
 * between front/back, with a subtle pointer-follow tilt when idle. Real
 * product photos only (cutout, transparent background) — no 3D model, no
 * fabricated render.
 *
 * Sized entirely via CSS (not a JS pixel prop) so one instance scales
 * fluidly across breakpoints instead of mounting separate fixed-size
 * copies per breakpoint, which would each carry their own (desynced)
 * flip state.
 *
 * The two source photos aren't pixel-identical in scale/framing (real
 * photos, shot independently), so rather than chase perfect alignment,
 * the flip itself dips scale/opacity as it passes edge-on (~90°) — the
 * same trick real flip-card UIs use — which hides the seam and reads as
 * a deliberate flourish instead of a jump cut.
 */
export default function BadgeViewer({
  widthClassName = "w-[68vw] max-w-[240px] sm:max-w-[340px] lg:max-w-[460px]",
  className = "",
}: {
  widthClassName?: string;
  className?: string;
}) {
  const [flipped, setFlipped] = useState(false);
  const containerRef = useRef<HTMLDivElement>(null);
  const isDragging = useRef(false);
  const dragStartX = useRef(0);
  const dragDistance = useRef(0);

  const rotateY = useMotionValue(0);
  const tiltX = useMotionValue(0);
  const springY = useSpring(rotateY, { stiffness: 90, damping: 18, mass: 0.9 });
  const springTiltX = useSpring(tiltX, { stiffness: 140, damping: 20 });

  // Dips scale/opacity as the card passes edge-on (90°/270°) — masks any
  // seam between the two independently-shot photos and sells the flip as
  // a deliberate motion rather than a swap.
  const edgeFactor = useTransform(springY, (v) => {
    const mod = ((v % 180) + 180) % 180; // 0 = flat (front or back facing), 90 = edge-on
    const distFromEdge = Math.abs(mod - 90); // 0 at edge-on, 90 at flat
    return distFromEdge / 90; // 0..1, 0 = edge-on, 1 = flat
  });
  const flipScale = useTransform(edgeFactor, [0, 1], [0.88, 1]);
  const flipOpacity = useTransform(edgeFactor, [0, 0.35, 1], [0.4, 1, 1]);

  // Belt-and-suspenders on top of backface-visibility: some browsers
  // ghost the hidden face under nested/animated transforms, so each
  // face's opacity is also hard-gated to whichever side is actually
  // facing the camera — only one image is ever visibly present.
  const frontOpacity = useTransform([flipOpacity, springY], (latest) => {
    const [op, v] = latest as [number, number];
    const mod = ((v % 360) + 360) % 360;
    return mod < 90 || mod > 270 ? op : 0;
  });
  const backOpacity = useTransform([flipOpacity, springY], (latest) => {
    const [op, v] = latest as [number, number];
    const mod = ((v % 360) + 360) % 360;
    return mod >= 90 && mod <= 270 ? op : 0;
  });

  const handlePointerMove = (e: React.PointerEvent) => {
    if (!containerRef.current) return;
    const rect = containerRef.current.getBoundingClientRect();
    const py = (e.clientY - rect.top) / rect.height - 0.5;
    if (isDragging.current) {
      const delta = e.clientX - dragStartX.current;
      dragDistance.current = delta;
      rotateY.set((flipped ? 180 : 0) + delta * 0.6);
    } else {
      const px = (e.clientX - rect.left) / rect.width - 0.5;
      tiltX.set(py * -10);
      rotateY.set((flipped ? 180 : 0) + px * 12);
    }
  };

  const handlePointerLeave = () => {
    if (isDragging.current) return;
    tiltX.set(0);
    rotateY.set(flipped ? 180 : 0);
  };

  const handlePointerDown = (e: React.PointerEvent) => {
    isDragging.current = true;
    dragStartX.current = e.clientX;
    dragDistance.current = 0;
    (e.target as HTMLElement).setPointerCapture(e.pointerId);
  };

  const settle = () => {
    if (Math.abs(dragDistance.current) < CLICK_THRESHOLD_PX) {
      // Treated as a tap/click, not a drag — just toggle.
      const next = !flipped;
      setFlipped(next);
      rotateY.set(next ? 180 : 0);
      return;
    }
    const current = ((rotateY.get() % 360) + 360) % 360;
    const shouldFlip = current > 90 && current < 270;
    setFlipped(shouldFlip);
    rotateY.set(shouldFlip ? 180 : 0);
  };

  const handlePointerUp = () => {
    if (!isDragging.current) return;
    isDragging.current = false;
    settle();
  };

  const toggle = () => {
    const next = !flipped;
    setFlipped(next);
    rotateY.set(next ? 180 : 0);
  };

  return (
    <div className={`flex flex-col items-center ${className}`}>
      <div
        ref={containerRef}
        onPointerMove={handlePointerMove}
        onPointerLeave={handlePointerLeave}
        onPointerDown={handlePointerDown}
        onPointerUp={handlePointerUp}
        style={{ perspective: 1400 }}
        className={`relative aspect-[3/4] cursor-grab touch-none select-none active:cursor-grabbing ${widthClassName}`}
      >
        <div className="absolute inset-0 -z-10 rounded-full bg-accent/20 blur-[90px]" />

        <motion.div
          style={{
            rotateY: springY,
            rotateX: springTiltX,
            scale: flipScale,
            transformStyle: "preserve-3d",
          }}
          className="relative h-full w-full"
        >
          {/* front */}
          <motion.div
            style={{
              backfaceVisibility: "hidden",
              WebkitBackfaceVisibility: "hidden",
              opacity: frontOpacity,
            }}
            className="absolute inset-0 flex items-center justify-center"
          >
            <Image
              src={asset("/images/badge-front-cutout.webp")}
              alt="Vanguard badge, front"
              fill
              priority
              sizes="(max-width: 640px) 68vw, (max-width: 1024px) 340px, 460px"
              className="object-contain drop-shadow-[0_30px_60px_rgba(255,107,0,0.18)]"
              draggable={false}
            />
          </motion.div>
          {/* back */}
          <motion.div
            style={{
              backfaceVisibility: "hidden",
              WebkitBackfaceVisibility: "hidden",
              transform: "rotateY(180deg)",
              opacity: backOpacity,
            }}
            className="absolute inset-0 flex items-center justify-center"
          >
            <Image
              src={asset("/images/badge-back-cutout.webp")}
              alt="Vanguard badge, back"
              fill
              sizes="(max-width: 640px) 68vw, (max-width: 1024px) 340px, 460px"
              className="object-contain drop-shadow-[0_30px_60px_rgba(255,107,0,0.18)]"
              draggable={false}
            />
          </motion.div>
        </motion.div>
      </div>

      <button
        type="button"
        onClick={toggle}
        className="mono mt-6 flex items-center gap-2 rounded-full border border-white/10 px-4 py-1.5 text-[10px] tracking-widest text-white/40 transition-colors hover:border-white/30 hover:text-white/70"
      >
        <span aria-hidden>⟲</span>
        DRAG TO ROTATE · {flipped ? "SHOWING BACK" : "SHOWING FRONT"}
      </button>
    </div>
  );
}
