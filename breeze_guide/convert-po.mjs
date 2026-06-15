// Convert gettext .po file to JSON format for gettext.js
// Usage: node convert-po.mjs

import * as fs from "fs";
import * as path from "path";
import { fileURLToPath } from "url";
import gettextParser from "gettext-parser";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

const PO_FILE = path.resolve(__dirname, "../lang/po/zh_CN.po");
const OUTPUT_DIR = path.resolve(__dirname, "public/data");
const OUTPUT_FILE = path.join(OUTPUT_DIR, "zh_CN.json");

try {
  console.log("PO_FILE:", PO_FILE);
  console.log("OUTPUT:", OUTPUT_FILE);

  if (!fs.existsSync(PO_FILE)) {
    console.error(`PO file not found: ${PO_FILE}`);
    process.exit(1);
  }

  const poContent = fs.readFileSync(PO_FILE, "utf-8");
  console.log("PO file size:", poContent.length, "bytes");

  const parsed = gettextParser.po.parse(poContent);
  console.log("Headers:", parsed.headers);

  // Build Jed-compatible JSON for gettext.js
  const messages = {};
  const translations = parsed.translations;

  // Add header
  messages[""] = {
    domain: "messages",
    language: "zh_CN",
    "plural-forms":
      parsed.headers["plural-forms"] || "nplurals=2; plural=(n!=1);",
  };

  let translatedCount = 0;
  let untranslatedCount = 0;

  for (const [, msgs] of Object.entries(translations)) {
    for (const [key, val] of Object.entries(msgs)) {
      if (key === "") continue;
      // Only include entries that have actual translations
      const hasTranslation = val.msgstr && val.msgstr.some((s) => s && s.length > 0);
      if (hasTranslation) {
        // gettext.js expects singular translations as strings, plural as arrays
        messages[key] = val.msgstr.length === 1 ? val.msgstr[0] : val.msgstr;
        translatedCount++;
      } else {
        untranslatedCount++;
      }
    }
  }

  console.log("Translated entries:", translatedCount);
  console.log("Untranslated entries:", untranslatedCount);
  console.log("Total message entries:", Object.keys(messages).length);

  // Write output
  if (!fs.existsSync(OUTPUT_DIR)) {
    fs.mkdirSync(OUTPUT_DIR, { recursive: true });
  }

  fs.writeFileSync(OUTPUT_FILE, JSON.stringify(messages));
  console.log("Written:", OUTPUT_FILE, `(${fs.statSync(OUTPUT_FILE).size} bytes)`);
  console.log("Done!");
} catch (e) {
  console.error("Error:", e.message);
  console.error(e.stack);
  process.exit(1);
}