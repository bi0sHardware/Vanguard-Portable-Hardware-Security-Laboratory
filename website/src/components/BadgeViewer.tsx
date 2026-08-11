"use client";

import Image from "next/image";
import { useRef, useState } from "react";
import { motion, useMotionValue, useSpring } from "framer-motion";
import { asset } from "@/lib/basePath";

/**
 * Interactive badge viewer: drag horizontally (or click) to flip between
 * front/back, with a subtle pointer-follow tilt when idle. Real product
 * photos only (cutout, transparent background) — no 3D model, no
 * fabricated render.
 */
export default function BadgeViewer({
  size = 340,
  className = "",
}: {
  size?: number;
  className?: string;
}) {
  const [flipped, setFlipped] = useState(false);
  const dragStartX = useRef(0);
  const containerRef = useRef<HTMLDivElement>(null);

  const rotateY = useMotionValue(0);
  const tiltX = useMotionValue(0);
  const springY = useSpring(rotateY, { stiffness: 120, damping: 16 });
  const springTiltX = useSpring(tiltX, { stiffness: 120, damping: 16 });

  const handlePointerMove = (e: React.PointerEvent) => {
    if (!containerRef.current) return;
    const rect = containerRef.current.getBoundingClientRect();
    const px = (e.clientX - rect.left) / rect.width - 0.5;
    const py = (e.clientY - rect.top) / rect.height - 0.5;
    tiltX.set(py * -12);
    if (!isDragging.current) {
      rotateY.set((flipped ? 180 : 0) + px * 14);
    }
  };

  const handlePointerLeave = () => {
    tiltX.set(0);
    rotateY.set(flipped ? 180 : 0);
  };

  const isDragging = useRef(false);

  const handlePointerDown = (e: React.PointerEvent) => {
    isDragging.current = true;
    dragStartX.current = e.clientX;
    (e.target as HTMLElement).setPointerCapture(e.pointerId);
  };

  const handleDragMove = (e: React.PointerEvent) => {
    if (!isDragging.current) return;
    const delta = e.clientX - dragStartX.current;
    rotateY.set((flipped ? 180 : 0) + delta * 0.6);
  };

  const handlePointerUp = () => {
    if (!isDragging.current) return;
    isDragging.current = false;
    const current = ((rotateY.get() % 360) + 360) % 360;
    const shouldFlip = current > 90 && current < 270;
    setFlipped(shouldFlip);
    rotateY.set(shouldFlip ? 180 : 0);
  };

  return (
    <div className={`flex flex-col items-center ${className}`}>
      <div
        ref={containerRef}
        onPointerMove={(e) => {
          handlePointerMove(e);
          handleDragMove(e);
        }}
        onPointerLeave={handlePointerLeave}
        onPointerDown={handlePointerDown}
        onPointerUp={handlePointerUp}
        onClick={() => {
          if (Math.abs(dragStartX.current - (dragStartX.current || 0)) < 2) {
            const next = !flipped;
            setFlipped(next);
            rotateY.set(next ? 180 : 0);
          }
        }}
        style={{ width: size, height: size * 1.3, perspective: 1200 }}
        className="relative cursor-grab touch-none select-none active:cursor-grabbing"
      >
        <div className="absolute inset-0 -z-10 rounded-full bg-accent/20 blur-[90px]" />

        <motion.div
          style={{
            rotateY: springY,
            rotateX: springTiltX,
            transformStyle: "preserve-3d",
          }}
          className="relative h-full w-full"
        >
          {/* front */}
          <div
            style={{ backfaceVisibility: "hidden" }}
            className="absolute inset-0 flex items-center justify-center"
          >
            <Image
              src={asset("/images/badge-front-cutout.webp")}
              alt="Vanguard badge, front"
              fill
              priority
              sizes={`${size}px`}
              className="object-contain drop-shadow-[0_30px_60px_rgba(255,107,0,0.18)]"
              draggable={false}
            />
          </div>
          {/* back */}
          <div
            style={{
              backfaceVisibility: "hidden",
              transform: "rotateY(180deg)",
            }}
            className="absolute inset-0 flex items-center justify-center"
          >
            <Image
              src={asset("/images/badge-back-cutout.webp")}
              alt="Vanguard badge, back"
              fill
              sizes={`${size}px`}
              className="object-contain drop-shadow-[0_30px_60px_rgba(255,107,0,0.18)]"
              draggable={false}
            />
          </div>
        </motion.div>
      </div>

      <button
        type="button"
        onClick={() => {
          const next = !flipped;
          setFlipped(next);
          rotateY.set(next ? 180 : 0);
        }}
        className="mono mt-6 flex items-center gap-2 rounded-full border border-white/10 px-4 py-1.5 text-[10px] tracking-widest text-white/40 transition-colors hover:border-white/30 hover:text-white/70"
      >
        <span aria-hidden>⟲</span>
        DRAG TO ROTATE · {flipped ? "SHOWING BACK" : "SHOWING FRONT"}
      </button>
    </div>
  );
}
