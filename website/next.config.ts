import type { NextConfig } from "next";

const repoName = "Vanguard-Portable-Hardware-Security-Laboratory";

const nextConfig: NextConfig = {
  output: "export",
  basePath: `/${repoName}`,
  assetPrefix: `/${repoName}/`,
  images: {
    unoptimized: true,
  },
  trailingSlash: true,
};

export default nextConfig;
