# SYCL agent skills

Skills in this directory follow the Agent Skills format (`<name>/SKILL.md` with `name` and
`description` frontmatter). Claude Code discovers them when working under `sycl/`. GitHub
Copilot is pointed here by `.github/instructions/sycl.instructions.md`.

| Skill | Use when |
|---|---|
| [`sycl-test`](sycl-test/SKILL.md) | Adding, extending, fixing, or reviewing a SYCL test. Chooses the tier (unit / LIT / E2E), enforces the write-then-prove-it-can-fail process, and lists the CI lints and UR-mock traps a reviewer should check. |
