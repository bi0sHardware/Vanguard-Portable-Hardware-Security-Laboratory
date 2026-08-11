# Vanguard Website

The public product website for Vanguard — Portable Hardware Security
Laboratory. Next.js (App Router), Tailwind CSS, Framer Motion, static
export, deployed to GitHub Pages.

## Development

```bash
npm install
npm run dev
```

Serves at `http://localhost:3000/Vanguard-Portable-Hardware-Security-Laboratory/`
— note the `basePath`, configured in `next.config.ts` to match this
repository's GitHub Pages URL (`bi0sHardware.github.io/Vanguard-Portable-Hardware-Security-Laboratory/`).
All internal image references go through `src/lib/basePath.ts`'s `asset()`
helper rather than bare `/images/...` paths, since static export with
`images.unoptimized` does not auto-prefix `basePath` onto image URLs.

## Build

```bash
npm run build
```

Produces a static export in `out/`, ready to serve as-is (no server
required).

## Deployment

Deploys automatically via `.github/workflows/deploy-website.yml` on every
push to `main` that touches `website/`, using GitHub Pages' native
Actions deployment (Settings → Pages → Source: GitHub Actions). No manual
build/upload step is needed.

## Assets

Real product photos and firmware screenshots live in `public/images/`,
resized and re-encoded (WebP for UI screenshots, compressed JPEG for
hardware photos) for web delivery. No fabricated renders or invented
interfaces — every image is the actual hardware or actual firmware UI.
