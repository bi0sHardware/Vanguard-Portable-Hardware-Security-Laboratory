"use client";

import { useEffect, useRef } from "react";

type Shooter = {
  x: number;
  y: number;
  vx: number;
  vy: number;
  life: number;
  maxLife: number;
};

/**
 * Lightweight canvas starfield with gentle opacity twinkle, plus
 * occasional shooting stars when enabled. Static star positions, no
 * motion that competes with foreground content.
 */
export default function StarField({
  density = 140,
  shootingStars = false,
}: {
  density?: number;
  shootingStars?: boolean;
}) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    let frame = 0;
    let raf = 0;
    let stars: { x: number; y: number; r: number; phase: number }[] = [];
    let shooters: Shooter[] = [];
    let nextShooterAt = 90 + Math.random() * 150;

    const resize = () => {
      canvas.width = canvas.offsetWidth * window.devicePixelRatio;
      canvas.height = canvas.offsetHeight * window.devicePixelRatio;
      stars = Array.from({ length: density }, () => ({
        x: Math.random() * canvas.width,
        y: Math.random() * canvas.height,
        r: Math.random() * 1.4 + 0.3,
        phase: Math.random() * Math.PI * 2,
      }));
    };

    const spawnShooter = () => {
      const startX = Math.random() * canvas.width;
      const startY = Math.random() * canvas.height * 0.4;
      const speed = 9 + Math.random() * 6;
      const angle = (Math.PI / 5) + Math.random() * 0.3;
      shooters.push({
        x: startX,
        y: startY,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed,
        life: 0,
        maxLife: 40 + Math.random() * 20,
      });
    };

    const draw = () => {
      if (!ctx) return;
      ctx.clearRect(0, 0, canvas.width, canvas.height);

      for (const s of stars) {
        const twinkle = 0.5 + 0.5 * Math.sin(frame * 0.01 + s.phase);
        ctx.beginPath();
        ctx.arc(s.x, s.y, s.r, 0, Math.PI * 2);
        ctx.fillStyle = `rgba(255,255,255,${0.15 + twinkle * 0.5})`;
        ctx.fill();
      }

      if (shootingStars) {
        if (frame >= nextShooterAt && shooters.length < 2) {
          spawnShooter();
          nextShooterAt = frame + 180 + Math.random() * 260;
        }

        shooters = shooters.filter((s) => s.life < s.maxLife);
        for (const s of shooters) {
          const progress = s.life / s.maxLife;
          const fade = progress < 0.15 ? progress / 0.15 : 1 - (progress - 0.15) / 0.85;
          const tailX = s.x - s.vx * 6;
          const tailY = s.y - s.vy * 6;

          const grad = ctx.createLinearGradient(s.x, s.y, tailX, tailY);
          grad.addColorStop(0, `rgba(255,255,255,${0.9 * fade})`);
          grad.addColorStop(1, "rgba(255,255,255,0)");

          ctx.strokeStyle = grad;
          ctx.lineWidth = 1.6;
          ctx.beginPath();
          ctx.moveTo(s.x, s.y);
          ctx.lineTo(tailX, tailY);
          ctx.stroke();

          s.x += s.vx;
          s.y += s.vy;
          s.life += 1;
        }
      }

      frame++;
      raf = requestAnimationFrame(draw);
    };

    resize();
    draw();
    window.addEventListener("resize", resize);
    return () => {
      window.removeEventListener("resize", resize);
      cancelAnimationFrame(raf);
    };
  }, [density, shootingStars]);

  return (
    <canvas
      ref={canvasRef}
      className="pointer-events-none absolute inset-0 h-full w-full opacity-70"
      aria-hidden
    />
  );
}
