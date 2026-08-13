"use client";

import { motion } from "framer-motion";
import BadgeScreen from "./BadgeScreen";
import { asset } from "@/lib/basePath";

export default function PeerNetworking() {
  return (
    <section id="peerdrop" className="relative border-t border-border py-24 lg:py-32">
      <div className="mx-auto grid max-w-7xl grid-cols-1 items-center gap-16 px-6 lg:grid-cols-2 lg:px-12">
        <motion.div
          initial={{ opacity: 0, y: 24 }}
          whileInView={{ opacity: 1, y: 0 }}
          viewport={{ once: true, margin: "-100px" }}
          transition={{ duration: 0.6 }}
        >
          <span className="mono text-xs tracking-[0.2em] text-technical">
            PEERDROP
          </span>
          <h2 className="mt-3 text-3xl font-semibold tracking-tight text-white sm:text-4xl">
            Build your network.
          </h2>
          <div className="mt-4 space-y-1.5">
            <p className="text-base text-white/60">Device discovery.</p>
            <p className="text-base text-white/60">Contact exchange.</p>
            <p className="text-base text-white/60">Badge-to-badge communication over BLE.</p>
          </div>
        </motion.div>

        <BadgeScreen
          screenshot={asset("/images/contacts.webp")}
          screenshotAlt="Vanguard Contacts / PeerDrop screen"
        />
      </div>
    </section>
  );
}
