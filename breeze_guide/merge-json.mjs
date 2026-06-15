// Script to merge all JSON files from ../data/json/ into a single all.json
// for use by the breeze_guide app.

import * as fs from "fs";
import * as path from "path";
import * as crypto from "crypto";
import { fileURLToPath } from "url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

const DATA_DIR = path.resolve(__dirname, "../data/json");
const OUTPUT_DIR = path.resolve(__dirname, "public/data");
const VERSION_FILE = path.resolve(__dirname, "../src/version.h");

// Read version from version.h
const versionContent = fs.readFileSync(VERSION_FILE, "utf8");
const versionMatch = versionContent.match(/VERSION\s+"(.+)"/);
const version = versionMatch ? versionMatch[1] : "CDDA-Breeze-unknown";

console.log(`Version: ${version}`);

// Recursively find all .json files
function findJsonFiles(dir) {
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  const files = [];
  for (const entry of entries) {
    const fullPath = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      files.push(...findJsonFiles(fullPath));
    } else if (entry.isFile() && entry.name.endsWith(".json")) {
      files.push(fullPath);
    }
  }
  return files;
}

const jsonFiles = findJsonFiles(DATA_DIR);
console.log(`Found ${jsonFiles.length} JSON files in ${DATA_DIR}`);

// Parse all JSON files and merge them into one array
const allData = [];
let fileCount = 0;
let errorCount = 0;

for (const file of jsonFiles) {
  try {
    const content = fs.readFileSync(file, "utf8");
    const parsed = JSON.parse(content);
    const relPath = path.relative(DATA_DIR, file).replace(/\\/g, "/");

    if (Array.isArray(parsed)) {
      for (const obj of parsed) {
        if (obj && typeof obj === "object") {
          obj.__filename = relPath;
          allData.push(obj);
        }
      }
    } else if (parsed && typeof parsed === "object") {
      parsed.__filename = relPath;
      allData.push(parsed);
    }
    fileCount++;
  } catch (e) {
    console.error(`Error parsing ${file}: ${e.message}`);
    errorCount++;
  }
}

console.log(
  `Successfully read ${fileCount} files, ${errorCount} errors, ${allData.length} objects total`,
);

// Create output directory
fs.mkdirSync(OUTPUT_DIR, { recursive: true });

// Write the merged JSON
const output = {
  data: allData,
  build_number: version,
  release: { version },
};

const outputPath = path.join(OUTPUT_DIR, "all.json");
fs.writeFileSync(outputPath, JSON.stringify(output));
console.log(`Wrote ${outputPath}`);

// Compute SHA256 hash
const shaHash = crypto.createHash("sha256").update(JSON.stringify(output), "utf8").digest("hex");

// Write meta file
const metaPath = path.join(OUTPUT_DIR, "all.meta.json");
fs.writeFileSync(
  metaPath,
  JSON.stringify({ buildNum: version, sha: shaHash }),
);
console.log(`Wrote ${metaPath}`);
console.log("Done!");