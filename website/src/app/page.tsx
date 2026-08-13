import Navbar from "@/components/Navbar";
import Hero from "@/components/Hero";
import DeviceJourney from "@/components/DeviceJourney";
import WhatVanguardGives from "@/components/WhatVanguardGives";
import PeerNetworking from "@/components/PeerNetworking";
import Diagnostics from "@/components/Diagnostics";
import OpenFirmware from "@/components/OpenFirmware";
import Reprogrammable from "@/components/Reprogrammable";
import BeyondCompetition from "@/components/BeyondCompetition";
import ArchitectureSection from "@/components/ArchitectureSection";
import Roadmap from "@/components/Roadmap";
import GetVanguard from "@/components/GetVanguard";
import FinalCTA from "@/components/FinalCTA";
import Footer from "@/components/Footer";

export default function Home() {
  return (
    <>
      <Navbar />
      <main>
        <Hero />
        <DeviceJourney />
        <WhatVanguardGives />
        <PeerNetworking />
        <Diagnostics />
        <OpenFirmware />
        <Reprogrammable />
        <BeyondCompetition />
        <ArchitectureSection />
        <Roadmap />
        <GetVanguard />
        <FinalCTA />
      </main>
      <Footer />
    </>
  );
}
