type IconProps = { className?: string };

const base = {
  width: 16,
  height: 16,
  viewBox: "0 0 24 24",
  fill: "none",
  stroke: "currentColor",
  strokeWidth: 1.6,
  strokeLinecap: "round" as const,
  strokeLinejoin: "round" as const,
};

export function ChipIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className}>
      <rect x="6" y="6" width="12" height="12" rx="1.5" />
      <path d="M9 2v4M15 2v4M9 18v4M15 18v4M2 9h4M2 15h4M18 9h4M18 15h4" />
    </svg>
  );
}

export function RadioIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className}>
      <circle cx="12" cy="18" r="2" />
      <path d="M12 16V9M8 12a5.5 5.5 0 0 1 8 0M5 9a9.5 9.5 0 0 1 14 0" />
    </svg>
  );
}

export function BluetoothIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className}>
      <path d="M7 7l10 10-5 5V2l5 5L7 17" />
    </svg>
  );
}

export function DisplayIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className}>
      <rect x="3" y="4" width="18" height="13" rx="1.5" />
      <path d="M8 21h8M12 17v4" />
    </svg>
  );
}

export function AudioIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className}>
      <path d="M4 10v4h4l5 5V5L8 10H4Z" />
      <path d="M17 8a5 5 0 0 1 0 8" />
    </svg>
  );
}

export function LedIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className}>
      <path d="M9 2h6l1 8a4 4 0 0 1-8 0l1-8Z" />
      <path d="M10 14v3h4v-3M9 21h6" />
    </svg>
  );
}

export function StorageIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className}>
      <ellipse cx="12" cy="6" rx="8" ry="3" />
      <path d="M4 6v6c0 1.7 3.6 3 8 3s8-1.3 8-3V6" />
      <path d="M4 12v6c0 1.7 3.6 3 8 3s8-1.3 8-3v-6" />
    </svg>
  );
}

export function BatteryIcon({ className }: IconProps) {
  return (
    <svg {...base} className={className}>
      <rect x="2" y="7" width="18" height="10" rx="1.5" />
      <path d="M22 10v4" />
      <path d="M6 10v4M10 10v4" />
    </svg>
  );
}
