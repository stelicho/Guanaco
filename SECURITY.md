# Security Policy

Guanaco is a local, offline CLI tool: it reads a `.gua` source file and
writes a G-code file. It has no network surface and doesn't execute the
G-code it emits. The most realistic security concerns are in the
hand-rolled C lexer/parser/evaluator (`lexer.c`, `parser.c`, `ast.c`,
`eval.c`, `value.c`, `gcode.c`) — e.g. a malformed or adversarial `.gua`
file triggering a crash, a memory-safety bug (buffer overflow, use of
uninitialized memory, etc.), or other undefined behavior.

## Supported Versions

Guanaco is pre-1.0 (see the README's "Versioning" section) and makes no
compatibility guarantees between releases. Only the most recently
tagged release receives security fixes:

| Version        | Supported          |
| --------------- | ------------------- |
| latest (`0.x`) | :white_check_mark: |
| older `0.x`    | :x:                 |

## Reporting a Vulnerability

Please **do not open a public issue or pull request** for a suspected
vulnerability — that discloses it to everyone before a fix exists.

Instead, use GitHub's private vulnerability reporting for this
repository:

1. Go to
   [github.com/stelicho/Guanaco/security/advisories/new](https://github.com/stelicho/Guanaco/security/advisories/new)
   (or the repository's **Security** tab → **Report a vulnerability**).
2. Open a private security advisory describing:
   - The issue and its potential impact.
   - Steps to reproduce it — ideally a minimal `.gua` file or command
     line that triggers it.
   - The Guanaco version, tag, or commit you tested against.

This starts a private conversation with the maintainer that stays
hidden from the public until a fix is ready.

If private reporting isn't enabled on this repository when you go to
use it, please open a regular issue asking the maintainer to enable it
or provide an alternate contact — without including any exploit
details there.

## Response

This is a small, actively-developed personal project with no formal
SLA. Reports will be acknowledged and triaged as promptly as possible;
a fix typically ships as a new patch release, with the issue disclosed
afterward via a GitHub Security Advisory.
