#!/usr/bin/env node
/**
 * mcu-skills - MCU Skills 知识库安装器
 *
 * 用法:
 *   npx mcu-skills install                    # 安装全部 Skill 到检测到的平台
 *   npx mcu-skills install mcu-sensors        # 安装单个 Skill（自动带依赖闭包）
 *   npx mcu-skills install --target claude    # 只装 Claude Code (~/.claude)
 *   npx mcu-skills install --target codex     # 只装 Codex (~/.codex)
 *   npx mcu-skills install --target codebuddy # 只装 CodeBuddy (~/.codebuddy)
 *   npx mcu-skills uninstall [skill...]       # 卸载
 *   npx mcu-skills list                       # 列出可用 Skill 与安装状态
 */

"use strict";

const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");

const PKG_ROOT = path.resolve(__dirname, "..");
const SKILLS_SRC = path.join(PKG_ROOT, "skills");
const COMMANDS_SRC = path.join(PKG_ROOT, ".codebuddy", "commands");

const AGENTS_START = "<!-- MCU-SKILLS:START -->";
const AGENTS_END = "<!-- MCU-SKILLS:END -->";

const TARGETS = {
  claude: {
    label: "Claude Code",
    dir: (home) => path.join(home, ".claude"),
    skillsDir: (home) => path.join(home, ".claude", "skills"),
    commandsDir: (home) => path.join(home, ".claude", "commands"),
    agentsFile: null,
  },
  codex: {
    label: "Codex",
    dir: (home) => path.join(home, ".codex"),
    skillsDir: (home) => path.join(home, ".codex", "skills"),
    commandsDir: (home) => path.join(home, ".codex", "prompts"),
    agentsFile: (home) => path.join(home, ".codex", "AGENTS.md"),
  },
  codebuddy: {
    label: "CodeBuddy",
    dir: (home) => path.join(home, ".codebuddy"),
    skillsDir: (home) => path.join(home, ".codebuddy", "skills"),
    commandsDir: (home) => path.join(home, ".codebuddy", "commands"),
    agentsFile: null,
  },
};

function loadSkillMeta() {
  const meta = new Map();
  for (const name of fs.readdirSync(SKILLS_SRC)) {
    const jsonPath = path.join(SKILLS_SRC, name, "skill.json");
    if (fs.existsSync(jsonPath)) {
      meta.set(name, JSON.parse(fs.readFileSync(jsonPath, "utf8")));
    }
  }
  return meta;
}

function dependencyClosure(names, meta) {
  const result = new Set();
  const stack = [...names];
  while (stack.length) {
    const name = stack.pop();
    if (result.has(name)) continue;
    if (!meta.has(name)) {
      throw new Error(`未知 Skill: ${name}（可用: ${[...meta.keys()].join(", ")}）`);
    }
    result.add(name);
    for (const dep of meta.get(name).dependencies || []) stack.push(dep);
  }
  return [...result].sort();
}

function detectTargets(home) {
  return Object.keys(TARGETS).filter((key) => fs.existsSync(TARGETS[key].dir(home)));
}

function copyDir(src, dest) {
  fs.rmSync(dest, { recursive: true, force: true });
  fs.mkdirSync(path.dirname(dest), { recursive: true });
  fs.cpSync(src, dest, { recursive: true });
}

function installCommands(target, home) {
  if (!fs.existsSync(COMMANDS_SRC)) return 0;
  const destDir = TARGETS[target].commandsDir(home);
  fs.mkdirSync(destDir, { recursive: true });
  let count = 0;
  for (const file of fs.readdirSync(COMMANDS_SRC)) {
    if (!file.endsWith(".md")) continue;
    fs.copyFileSync(path.join(COMMANDS_SRC, file), path.join(destDir, file));
    count += 1;
  }
  return count;
}

function uninstallCommands(target, home) {
  if (!fs.existsSync(COMMANDS_SRC)) return;
  const destDir = TARGETS[target].commandsDir(home);
  if (!fs.existsSync(destDir)) return;
  for (const file of fs.readdirSync(COMMANDS_SRC)) {
    fs.rmSync(path.join(destDir, file), { force: true });
  }
}

function updateAgentsBlock(target, home, meta) {
  const agentsFn = TARGETS[target].agentsFile;
  if (!agentsFn) return;
  const agentsPath = agentsFn(home);
  const skillsDir = TARGETS[target].skillsDir(home);
  const installed = fs.existsSync(skillsDir)
    ? fs.readdirSync(skillsDir).filter((name) => meta.has(name)).sort()
    : [];

  let block = "";
  if (installed.length) {
    const lines = [
      AGENTS_START,
      "## MCU Skills（由 mcu-skills 安装器维护，请勿手工编辑本区块）",
      "",
      "以下 Skill 已安装。当用户请求匹配某个 Skill 的描述时，先读取其 SKILL.md 并按其中的意图路由表加载对应资源。",
      "跨 Skill 引用 `skill://<name>/<path>` 解析为本目录下 `<name>/<path>`。",
      "",
    ];
    for (const name of installed) {
      const item = meta.get(name);
      lines.push(`- \`${path.join(skillsDir, name, "SKILL.md")}\` — ${item.title}: ${item.summary}`);
    }
    lines.push(AGENTS_END);
    block = lines.join("\n");
  }

  let text = fs.existsSync(agentsPath) ? fs.readFileSync(agentsPath, "utf8") : "";
  const start = text.indexOf(AGENTS_START);
  const end = text.indexOf(AGENTS_END);
  if (start !== -1 && end !== -1) {
    text = text.slice(0, start) + block + text.slice(end + AGENTS_END.length);
  } else if (block) {
    text = text ? `${text.replace(/\n*$/, "\n\n")}${block}\n` : `${block}\n`;
  }
  text = text.replace(/\n{3,}/g, "\n\n").replace(/\n*$/, "\n");
  if (text.trim()) {
    fs.mkdirSync(path.dirname(agentsPath), { recursive: true });
    fs.writeFileSync(agentsPath, text);
  }
}

function parseArgs(argv) {
  const args = { command: argv[0] || "help", skills: [], targets: null, home: os.homedir() };
  for (let i = 1; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--target") {
      const value = argv[++i];
      if (value === "all") args.targets = Object.keys(TARGETS);
      else if (TARGETS[value]) args.targets = [...(args.targets || []), value];
      else throw new Error(`未知 target: ${value}（可用: claude, codex, codebuddy, all）`);
    } else if (arg === "--home") {
      args.home = argv[++i];
    } else if (arg.startsWith("--")) {
      throw new Error(`未知参数: ${arg}`);
    } else {
      args.skills.push(arg);
    }
  }
  return args;
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const meta = loadSkillMeta();

  if (args.command === "list") {
    const detected = detectTargets(args.home);
    console.log("可用 Skill:");
    for (const [name, item] of [...meta.entries()].sort()) {
      const where = detected.filter((t) =>
        fs.existsSync(path.join(TARGETS[t].skillsDir(args.home), name))
      );
      const status = where.length ? `已安装: ${where.join(", ")}` : "未安装";
      console.log(`  ${name} v${item.version} [${item.layer}] — ${status}`);
    }
    console.log(`\n检测到的平台: ${detected.join(", ") || "无"}`);
    return;
  }

  if (args.command !== "install" && args.command !== "uninstall") {
    console.log(fs.readFileSync(__filename, "utf8").match(/\/\*\*[\s\S]*?\*\//)[0]);
    process.exitCode = args.command === "help" ? 0 : 1;
    return;
  }

  const targets = args.targets || detectTargets(args.home);
  if (!targets.length) {
    throw new Error("未检测到 ~/.claude、~/.codex 或 ~/.codebuddy，请用 --target 指定平台");
  }

  const requested = args.skills.length ? args.skills : [...meta.keys()];
  const names =
    args.command === "install" ? dependencyClosure(requested, meta) : requested;

  for (const target of targets) {
    const skillsDir = TARGETS[target].skillsDir(args.home);
    if (args.command === "install") {
      for (const name of names) {
        copyDir(path.join(SKILLS_SRC, name), path.join(skillsDir, name));
      }
      const commandCount = names.includes("project-organizer")
        ? installCommands(target, args.home)
        : 0;
      updateAgentsBlock(target, args.home, meta);
      console.log(
        `[${TARGETS[target].label}] 已安装 ${names.length} 个 Skill 到 ${skillsDir}` +
          (commandCount ? `，${commandCount} 个命令` : "")
      );
    } else {
      for (const name of names) {
        if (!meta.has(name)) throw new Error(`未知 Skill: ${name}`);
        fs.rmSync(path.join(skillsDir, name), { recursive: true, force: true });
      }
      if (names.includes("project-organizer")) uninstallCommands(target, args.home);
      updateAgentsBlock(target, args.home, meta);
      console.log(`[${TARGETS[target].label}] 已卸载: ${names.join(", ")}`);
    }
  }
}

try {
  main();
} catch (error) {
  console.error(`错误: ${error.message}`);
  process.exitCode = 1;
}
