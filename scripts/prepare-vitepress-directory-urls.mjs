#!/usr/bin/env node
import { promises as fs } from "node:fs";
import path from "node:path";
import process from "node:process";

const docsRoot = path.resolve("dist/docs");

async function exists(filePath) {
  try {
    await fs.access(filePath);
    return true;
  } catch {
    return false;
  }
}

async function* walk(dir) {
  for (const entry of await fs.readdir(dir, { withFileTypes: true })) {
    const entryPath = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      yield* walk(entryPath);
    } else {
      yield entryPath;
    }
  }
}

if (!(await exists(docsRoot))) {
  console.error("dist/docs does not exist. Run the VitePress build first.");
  process.exit(1);
}

let created = 0;

for await (const file of walk(docsRoot)) {
  const relative = path.relative(docsRoot, file);
  if (!relative.endsWith(".html")) continue;
  if (relative === "index.html" || relative === "404.html") continue;
  if (relative.endsWith(`${path.sep}index.html`)) continue;

  const directoryUrlPath = relative.slice(0, -".html".length);
  const target = path.join(docsRoot, directoryUrlPath, "index.html");
  await fs.mkdir(path.dirname(target), { recursive: true });
  await fs.copyFile(file, target);
  created += 1;
}

console.log(`Prepared ${created} directory-style docs URLs.`);
