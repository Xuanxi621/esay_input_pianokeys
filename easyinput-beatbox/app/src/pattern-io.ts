import { bankHex, banksFromDump, type PatternBanks } from "./pattern";

export const PATTERN_FILE_FORMAT = "easyinput-beatbox-pattern";
export const PATTERN_FILE_VERSION = 1;

export type PatternFile = {
  format: typeof PATTERN_FILE_FORMAT;
  v: number;
  bpm?: number;
  swing?: number;
  a: string;
  b: string;
  fill: string;
};

export function exportPatternFile(
  pattern: PatternBanks,
  meta: { bpm: number; swing: number },
): PatternFile {
  return {
    format: PATTERN_FILE_FORMAT,
    v: PATTERN_FILE_VERSION,
    bpm: meta.bpm,
    swing: meta.swing,
    a: bankHex(pattern, 0),
    b: bankHex(pattern, 1),
    fill: bankHex(pattern, 2),
  };
}

export function patternFileToJson(file: PatternFile): string {
  return `${JSON.stringify(file, null, 2)}\n`;
}

export function parsePatternFile(text: string): { ok: true; file: PatternFile; banks: PatternBanks } | { ok: false; error: string } {
  let raw: unknown;
  try {
    raw = JSON.parse(text);
  } catch {
    return { ok: false, error: "JSON 无法解析" };
  }
  if (!raw || typeof raw !== "object") {
    return { ok: false, error: "文件格式无效" };
  }
  const obj = raw as Record<string, unknown>;
  if (obj.format !== PATTERN_FILE_FORMAT) {
    return { ok: false, error: "不是 EasyInput Beatbox Pattern 文件" };
  }
  if (obj.v !== PATTERN_FILE_VERSION) {
    return { ok: false, error: `不支持的文件版本：${String(obj.v)}` };
  }
  if (typeof obj.a !== "string" || typeof obj.b !== "string" || typeof obj.fill !== "string") {
    return { ok: false, error: "缺少 a / b / fill 字段" };
  }
  const banks = banksFromDump({
    rev: 1,
    a: obj.a,
    b: obj.b,
    f: obj.fill,
  });
  if (!banks) {
    return { ok: false, error: "Pattern hex 无效（需要每 bank 192 字符）" };
  }
  const file: PatternFile = {
    format: PATTERN_FILE_FORMAT,
    v: PATTERN_FILE_VERSION,
    a: obj.a,
    b: obj.b,
    fill: obj.fill,
  };
  if (typeof obj.bpm === "number") file.bpm = obj.bpm;
  if (typeof obj.swing === "number") file.swing = obj.swing;
  return { ok: true, file, banks };
}

export function downloadTextFile(filename: string, text: string, mime = "application/json") {
  const blob = new Blob([text], { type: mime });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = filename;
  a.click();
  URL.revokeObjectURL(url);
}
