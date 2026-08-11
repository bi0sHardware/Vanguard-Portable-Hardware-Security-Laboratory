import Navbar from "@/components/Navbar";
import Hero from "@/components/Hero";
import MeetVanguard from "@/components/MeetVanguard";
import PlatformIdentity from "@/components/PlatformIdentity";
import ScreenSection from "@/components/ScreenSection";
import WirelessComm from "@/components/WirelessComm";
import PeerNetworking from "@/components/PeerNetworking";
import Diagnostics from "@/components/Diagnostics";
import EmbeddedApps from "@/components/EmbeddedApps";
import UserExperience from "@/components/UserExperience";
import BeyondCompetition from "@/components/BeyondCompetition";
import Reprogrammable from "@/components/Reprogrammable";
import LearnByBuilding from "@/components/LearnByBuilding";
import OpenFirmware from "@/components/OpenFirmware";
import ArchitectureSection from "@/components/ArchitectureSection";
import Roadmap from "@/components/Roadmap";
import FinalCTA from "@/components/FinalCTA";
import Footer from "@/components/Footer";
import { asset } from "@/lib/basePath";

export default function Home() {
  return (
    <>
      <Navbar />
      <main>
        <Hero />
        <MeetVanguard />
        <PlatformIdentity />
        <ScreenSection
          id="challenges"
          kicker="SECURITY CHALLENGE FRAMEWORK"
          title="Progressive, hardware-grounded exercises"
          description="Four sequential levels covering UART reconnaissance, RF protocol reverse engineering, authenticated uplink scripting, and file-format forensics — each one solved against real hardware behavior, not a simulated target."
          points={[
            "Investigation over instruction",
            "Protocol interaction, not multiple choice",
            "Sequential unlock chain with persistent progress",
            "Built on the same firmware that runs everything else",
          ]}
          image={asset("/images/challenges-menu.webp")}
          imageAlt="Vanguard challenges menu"
        />
        <WirelessComm />
        <PeerNetworking />
        <Diagnostics />
        <EmbeddedApps />
        <UserExperience />
        <BeyondCompetition />
        <Reprogrammable />
        <LearnByBuilding />
        <OpenFirmware />
        <ArchitectureSection />
        <Roadmap />
        <FinalCTA />
      </main>
      <Footer />
    </>
  );
}
