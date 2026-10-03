/**
 * Pad pictograms from oclero/qlementine-icons (MIT).
 * https://github.com/oclero/qlementine-icons
 */
import kickSvg from "./assets/icons/drums/kick.svg?raw";
import snareSvg from "./assets/icons/drums/snare.svg?raw";
import hihatSvg from "./assets/icons/drums/hihat.svg?raw";
import cymbalSvg from "./assets/icons/drums/cymbal.svg?raw";
import clapSvg from "./assets/icons/drums/clap.svg?raw";
import rimSvg from "./assets/icons/drums/rim.svg?raw";

export type DrumIconId = "kick" | "snare" | "chh" | "ohh" | "clap" | "rim" | "abfill" | "play";

/** Control pads keep tiny inline marks; drums use the vendored set. */
const abfillSvg =
  '<svg class="drum-icon-svg" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"><rect x="3.5" y="5" width="7" height="14" rx="1.5"/><rect x="13.5" y="5" width="7" height="14" rx="1.5"/></svg>';

const playSvg =
  '<svg class="drum-icon-svg" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor"><path d="M8 5.5v13l11-6.5-11-6.5z"/></svg>';

export const DRUM_ICONS: Record<DrumIconId, string> = {
  kick: kickSvg,
  snare: snareSvg,
  chh: hihatSvg,
  ohh: cymbalSvg,
  clap: clapSvg,
  rim: rimSvg,
  abfill: abfillSvg,
  play: playSvg,
};
