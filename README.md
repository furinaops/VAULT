# VAULT — Personal Cryptographic Version Control System

VAULT is a content-addressed, cryptographically chained version store with a
custom CP-based transport protocol. It is **not a Git clone**. It is built for
solo developers and small teams (2–5 people) who want full data ownership, no
cloud dependency, and a simpler mental model than Git.

```
                    ┌──────────┐
                    │  Relay   │  (stateless CP byte pipe)
                    │ :9001    │
                    └────┬─────┘
                         │
            ┌────────────┴────────────┐
            │                         │
      ┌─────▼──────┐          ┌──────▼─────┐
      │  Receiver  │          │   Sender   │
      │  (daemon)  │          │   (CLI)    │
      │  :9000     │          │            │
      └────────────┘          └────────────┘
      Owns the data           Stateless, only
      Source of truth         caches ~/.vault/HEAD
```

---

## Table of Contents

- [Philosophy](#philosophy)
- [How VAULT Differs from Git](#how-vault-differs-from-git)
- [Quickstart Tutorial](#quickstart-tutorial)
  - [Building](#building)
  - [Your First Push](#your-first-push)
  - [Viewing History](#viewing-history)
  - [Comparing Versions](#comparing-versions)
  - [Verifying Integrity](#verifying-integrity)
  - [Rolling Back](#rolling-back)
  - [Setting Up the Receiver & Relay](#setting-up-the-receiver--relay)
- [Architecture](#architecture)
  - [Three Node Architecture](#three-node-architecture)
  - [CP Protocol Integration](#cp-protocol-integration)
- [Command Reference](#command-reference)
  - [/HOOK — Push a version](#hook--push-a-version)
  - [/GET — Fetch a version](#get--fetch-a-version)
  - [/SIC — Query version identification](#sic--query-version-identification)
  - [/DIFF — Compare two versions](#diff--compare-two-versions)
  - [/SYNC — Pull latest changes](#sync--pull-latest-changes)
  - [/VERIFY — Verify chain integrity](#verify--verify-chain-integrity)
  - [/ROLLBACK — Revert HEAD](#rollback--revert-head)
- [Technical Details](#technical-details)
  - [SIC Generation Algorithm](#sic-generation-algorithm)
  - [Object Store Layout](#object-store-layout)
  - [Delta / Diffing Strategy](#delta--diffing-strategy)
  - [Name Registry](#name-registry)
  - [Author Identity](#author-identity)
  - [Optimistic Locking](#optimistic-locking)
- [Configuration](#configuration)
- [Project Structure](#project-structure)

---

## Philosophy

VAULT was designed around a few core beliefs:

**1. You should own your data.** No cloud dependency. No SaaS. No centralised
server that can go away or change its terms. Your repository lives on hardware
you control — a NAS in your closet, a Raspberry Pi on your desk, or the cheapest
VPS you can find acting as a dumb relay.

**2. Version control should be simpler than Git.** Git is powerful but its
mental model — staging area, index, committed/uncommitted/untracked, branches vs
tags, remote tracking branches, rebase vs merge — is punishingly complex for
what most people actually do. VAULT has one primitive: the **version**. A version
contains a snapshot of your files and a pointer to its parent. There are no
branches, no staging area, no rebase. You push versions, you fetch versions, you
roll back. That's it.

**3. Cryptographic integrity should be automatic and obvious.** Every version
has a **Special Identification Code (SIC)** — a SHA256 hash of the version's
content PLUS its parent's SIC. This means the chain is cryptographically
provable: you cannot tamper with any version without invalidating all
descendants. Verification is a first-class command, not an afterthought.

**4. The receiver is the source of truth.** Your local machine is disposable.
The receiver daemon on your NAS holds the canonical repository. Your
`~/.vault/HEAD` is a cache. If they diverge, `/SYNC` resolves it — the receiver
always wins.

**5. No auto-merge, ever.** If `/SYNC` detects that a file changed both on the
server and locally, VAULT tells you about the conflict and stops. You resolve it
manually, then push. Machines should not make judgement calls about code.

---

## How VAULT Differs from Git

| Concept | Git | VAULT |
|---------|-----|-------|
| **Primitive** | Blob, tree, commit, tag, branch | Version (one thing) |
| **Identity** | SHA1 of commit content | SIC = SHA256(parent + tree + author + timestamp + name) |
| **Staging** | Index / staging area required | None — everything is auto-detected |
| **Branching** | First-class branches with DAG merges | Linear chain only (no branches) |
| **Merge** | Auto-merge with conflict markers | Never auto-merges — lists conflicts, stops |
| **Remote** | Remote tracking branches, fetch/pull/rebase | Single receiver = source of truth |
| **Transport** | SSH, HTTP, Git protocol | CP protocol (built-in, encrypted) |
| **State** | Complex state machine | Stateless sender, stateful receiver |
| **Undo** | Reset, revert, rebase, reflog | Rollback moves HEAD (versions stay) |
| **Integrity** | SHA1 (migrating to SHA256) | SIC chain is independently verifiable |
| **Target** | Large teams, open source | Solo devs, small teams (2–5) |
| **Setup** | Server + SSH keys + permissions | Single binary on NAS, one config file |
| **Network** | Requires direct or SSH access | Works through stateless relay, no open ports needed |

### What VAULT deliberately does NOT do

- **No branches or forks.** The version chain is linear. If you want parallel
  lines of development, run a second receiver.
- **No staging area.** Every `/HOOK` snapshots the working directory. If you
  want selective commits, curate your files first.
- **No auto-merging.** Conflict detection is exact (hash comparison). Resolution
  is human-only.
- **No rebase, amend, or history rewriting.** The SIC chain is append-only.
  Tampering is detectable, not preventable.
- **No submodules, subtrees, or similar.** One repository, one project.
- **No GUI (yet).** There is a terminal UI and a planned single-file HTML
  read-only web view.

---

## Quickstart Tutorial

### Building

**Dependencies:**
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- OpenSSL (development headers — `libssl-dev` on Debian, `openssl-devel` on RPM)
- CMake 3.20+
- Make or Ninja

```bash
# From the VAULT project root:
mkdir build && cd build
cmake ..
make -j$(nproc)

# Three binaries are produced:
ls -lh vault vault-receiver vault-relay
```

> **Note:** All default data paths (`storage_path`, `log_path`, config files) resolve
> via `$HOME` at runtime — no hardcoded machine-specific paths. The project builds
> and runs on any machine without editing configuration.

### Your First Push

VAULT works offline by default. The sender CLI stores everything locally in
`~/.vault/` — no receiver or relay needed for standalone use.

Create a project and push your first version:

```bash
# Create a small project
mkdir -p ~/hello-world/src
cat > ~/hello-world/src/main.c << 'EOF'
#include <stdio.h>
int main() {
    printf("hello, world\n");
    return 0;
}
EOF

# Create a .vaultignore (optional — like .gitignore)
echo "build/" > ~/hello-world/.vaultignore
mkdir ~/hello-world/build

# Push version 1
./build/vault /HOOK ~/hello-world --n initial --msg "hello world in C"

# Output:
#   Pushing initial...
#   [████████████████████] 100%
#
#   ✓ Received by server
#   ✓ Arranged against genesi (genesi)
#   ✓ SIC generated
#
#   ┌─────────────────────────────────────────┐
#   │ initial                                │
#   │ SIC    a1b2c3  (full: a1b2c3d4e5f6...) │
#   │ Parent genesi                          │
#   │ Files  1 changed                       │
#   │ Time   2026-06-18 14:32:01             │
#   └─────────────────────────────────────────┘
```

Now modify a file and push again:

```bash
cat > ~/hello-world/src/main.c << 'EOF'
#include <stdio.h>
int main(int argc, char* argv[]) {
    printf("hello, %s\n", argc > 1 ? argv[1] : "world");
    return 0;
}
EOF

./build/vault /HOOK ~/hello-world --n fix1 --msg "added arg support"

# Output shows parent linkage:
#   ✓ Arranged against initial (a1b2c3)
```

### Viewing History

```bash
./build/vault /SIC --log

#   ┌──────────────────────────────────────────────────────────────────┐
#   │  VAULT version chain                                             │
#   ├──────────┬─────────┬──────────────────┬──────────────────────────┤
#   │ Name     │ SIC     │ Timestamp        │ Author                   │
#   ├──────────┼─────────┼──────────────────┼──────────────────────────┤
#   │ initial  │ a1b2c3  │ 2026-06-18 14:32 │ default                  │
#   │ fix1     │ d4e5f6  │ 2026-06-18 14:33 │ default                  HEAD │
#   └──────────┴─────────┴──────────────────┴──────────────────────────┘
```

Get details on a specific version:

```bash
./build/vault /SIC fix1

#   SIC: d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2c3d4
#   Short: d4e5f6
#   Name: fix1
#   Parent: a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0
#   Author: default
#   Time: 2026-06-18 14:33:01
#   Message: added arg support
```

### Comparing Versions

```bash
./build/vault /DIFF initial fix1

#   Comparing initial (a1b2c3) → fix1 (d4e5f6)
#
#   src/main.c
#     - line 1: #include <stdio.h>
#     + line 1: int main(int argc, char* argv[]) {
#     - line 2: int main() {
#     - line 3:     printf("hello, world\n");
#     + line 2:     printf("hello, %s\n", argc > 1 ? argv[1] : "world");
#     - line 4:     return 0;
#     + line 3:     return 0;
#     - line 5: }
#     + line 4: }
```

### Verifying Integrity

```bash
./build/vault /VERIFY

#   Verifying chain integrity...
#
#   a1b2c3  initial   ✓
#   d4e5f6  fix1      ✓
#
#   Chain intact. 2 version(s) verified.
```

The SIC chain is independently verifiable. Anyone with the version data can
verify that no version has been tampered with.

### Rolling Back

```bash
./build/vault /ROLLBACK initial

#   ⚠ Rolling back to initial (a1b2c3)...
#   ✓ HEAD is now initial (a1b2c3)
#   Newer versions still exist. Use /SIC --log to see full chain.
```

Rollback only moves the HEAD pointer. It does **not** delete any data. Orphaned
versions remain in the object store and can be re-fetched.

### Fetching a Version

```bash
# Restore files from a specific version to the current directory
./build/vault /GET fix1

#   Restored: src/main.c
#   ✓ Fetched version "fix1" (d4e5f6)
```

### Setting Up the Receiver & Relay

For team use or centralised backup, run the receiver on your NAS:

```bash
# Create config
mkdir -p ~/.vault
cat > ~/.vault/receiver.json << 'JSON'
{
  "port": 9000,
  "storage_path": "/mnt/nas/vault-repos/",
  "owner_key": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
  "guest_key": "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210",
  "max_repo_size_mb": 2048,
  "log_path": "/var/log/vault-receiver.log"
}
JSON

# Start receiver daemon
./build/vault-receiver --config ~/.vault/receiver.json
```

For internet access through NAT, run the relay on a public VPS:

```bash
cat > ~/.vault/relay.json << 'JSON'
{
  "port": 9001,
  "session_timeout_seconds": 3600,
  "max_concurrent_sessions": 50,
  "log_path": "/var/log/vault-relay.log"
}
JSON

./build/vault-relay --config ~/.vault/relay.json
```

---

## Architecture

### Three Node Architecture

#### 1. Receiver Node (Daemon)

The receiver is the **source of truth**. It is a long-running CP server daemon
that:

- Listens on a configurable TCP port (default: 9000)
- Owns the object store, version chain, name registry, and author registry
- Handles incoming CP trigger messages (HOOK_INIT, GET_REQ, SIC_QUERY, etc.)
- Generates SICs for new versions
- Enforces optimistic locking — rejects push if HEAD has moved
- Stores data at a configurable path (e.g. `/mnt/nas/vault-repos/`)

The receiver never initiates contact. It waits for senders to connect (directly
or through the relay) and responds to commands.

#### 2. Sender Node (CLI)

The sender is a **stateless CLI tool**. It:

- Reads the current HEAD SIC from `~/.vault/HEAD`
- Computes deltas by walking the working directory and comparing against the
  last known tree
- Packs changed files, builds a version, and sends it through CP
- Receives the generated SIC from the receiver
- Updates `~/.vault/HEAD` to the new SIC

The sender is stateless in the sense that the only persistent local state is
`~/.vault/HEAD` (a single file containing one SIC). If you lose your local
machine, you can clone by fetching from the receiver.

#### 3. Relay Server

The relay is a **minimal stateless byte pipe**. It:

- Maintains two persistent socket connections (sender + receiver)
- Matches them by a session token
- Forwards raw bytes bidirectionally
- Has zero storage, zero business logic
- Times out and drops sessions after configurable inactivity

Session lifecycle:

1. Receiver starts, generates a session token, connects to relay, sends
   `REGI{token}`.
2. Relay registers the session: `token → receiver_fd`.
3. Sender connects to relay, sends `JOIN{token}`.
4. Relay pairs the sender with the receiver.
5. Both sides now exchange raw bytes through the relay.
6. When either side disconnects, the relay drops the session.

The relay **cannot read message content** (everything is CP-encrypted). It is
purely a byte pipe. Deploy it on the cheapest VPS you can find.

### CP Protocol Integration

All network I/O goes through the **Conversational Protocol (CP)** library. No
raw TCP or socket code exists in VAULT outside of what CP provides.

VAULT defines these CP trigger words (message types):

| Message | Direction | Purpose |
|---------|-----------|---------|
| HOOK_INIT | sender → receiver | Begin a push |
| HOOK_READY | receiver → sender | Ready to receive payload |
| HOOK_DATA | sender → receiver | Encrypted file payload |
| HOOK_DONE | receiver → sender | Push complete, SIC follows |
| GET_REQ | sender → receiver | Request a version |
| GET_READY | receiver → sender | Ready to send, payload size |
| GET_DATA | receiver → sender | Version payload |
| GET_ACK | sender → receiver | Received successfully |
| SIC_QUERY | sender → receiver | Query SIC for a name or HEAD |
| SYNC_REQ | sender → receiver | What changed since my HEAD? |
| SYNC_RESPONSE | receiver → sender | Versions since sender's HEAD |
| VERIFY_REQ | sender → receiver | Verify chain integrity |
| ROLLBACK_REQ | sender → receiver | Roll HEAD back |
| HOOK_REJECTED | receiver → sender | Push rejected (optimistic lock) |

CP handles authentication, session management, heartbeat, encryption, and
message integrity (CRC32). VAULT plugs business logic into CP's trigger
dispatch system.

---

## Command Reference

### `/HOOK` — Push a version

```
/HOOK "<path>" --n "<name>" [--msg "<description>"]
```

**Arguments:**
- `path` — File or directory to snapshot (required)
- `--n <name>` — Human-readable name for this version (required)
- `--msg <description>` — Optional description message

**Behaviour:**
1. Reads current HEAD SIC from `~/.vault/HEAD`
2. Loads the last known tree (from the version referenced by HEAD)
3. Walks the working directory recursively, computing SHA256 hashes
4. Compares each file against the previous tree — only changed files are packed
5. Respects `.vaultignore` (same syntax as `.gitignore`)
6. Warns if a file exceeds 10 MB — binary files are skipped unless `--force`
7. Packs changed blobs, metadata, and parent SIC
8. Computes the new SIC locally and stores everything in `~/.vault/data/`
9. Prints a progress bar, version card, and SIC

**Examples:**
```bash
/HOOK "~/project/" --n "initial"
/HOOK "~/project/parser.cpp" --n "fix-bug" --msg "null check on line 47"
/HOOK "~/project/" --n "release-1.0" --msg "stable build"
```

### `/GET` — Fetch a version

```
/GET "<sic_or_name_or_latest>" [--delta] [--log]
```

**Arguments:**
- `sic_or_name_or_latest` — Short SIC (6+ hex chars), name alias, or `latest`
- `--delta` — Download only changed files (not yet implemented in local mode)
- `--log` — Print version history after fetching

**Behaviour:**
1. Resolves the argument to a full SIC (name → SIC, short SIC → full SIC)
2. Loads the version and its tree
3. Writes all files to the current working directory
4. Updates `~/.vault/HEAD` to the fetched SIC

**Examples:**
```bash
/GET "a1b2c3"         # fetch by short SIC
/GET "fix1"           # fetch by name
/GET latest           # fetch HEAD
/GET latest --log     # fetch HEAD + print chain
```

### `/SIC` — Query version identification

```
/SIC "<name_or_latest>" [--log] [--log --n <count>]
```

**Arguments:**
- `name_or_latest` — Version name or `latest` for HEAD
- `--log` — Print the full version chain as a table
- `--n <count>` — With `--log`, show only the last N versions

**Behaviour:**
- Without `--log`: prints full SIC, short SIC, name, parent, author, timestamp,
  and message for the specified version
- With `--log`: prints a formatted table of the version chain with HEAD marker

**Examples:**
```bash
/SIC "fix1"           # single version details
/SIC latest           # HEAD details
/SIC --log            # full chain table
/SIC --log --n 5      # last 5 versions
```

### `/DIFF` — Compare two versions

```
/DIFF "<sic_or_name_1>" "<sic_or_name_2>"
```

**Arguments:**
- Two version identifiers (name, short SIC, or full SIC)

**Behaviour:**
1. Resolves both arguments to full SICs
2. Loads both trees
3. Compares every file entry — added, deleted, and modified
4. For modified files, shows a line-by-line diff (± markers, line numbers)

**Examples:**
```bash
/DIFF initial fix1
/DIFF "a1b2c3" latest
```

### `/SYNC` — Pull latest changes

```
/SYNC
```

**Behaviour:**
1. Reads local HEAD SIC from `~/.vault/HEAD`
2. Lists all versions in local store that were added after that SIC
3. For each new version, checks every file against the local working directory
4. If a file exists locally with a different hash → **conflict detected**
5. If no conflicts: overwrites working directory files with latest versions,
   updates HEAD
6. If conflicts: lists each conflicting file, prints the version that caused it,
   and stops — **no auto-merge**

**Output (no conflicts):**
```
Syncing...
  ✓ 2 new versions since your last sync

  d4e5f6  fix1    2026-06-18 14:33  default  ◄ HEAD

  Local files updated.
```

**Output (conflicts):**
```
Syncing...
  ⚠ Conflict detected in src/main.c
    Your local changes overlap with fix1 (d4e5f6)
    Resolve manually then run /HOOK to push your version.
```

### `/VERIFY` — Verify chain integrity

```
/VERIFY [<sic>]
```

**Arguments:**
- `sic` — Optional: verify up to this version only

**Behaviour:**
1. Walks every version in the store chronologically
2. Recomputes the SIC for each version from its fields
3. Compares computed SIC against stored SIC
4. Reports each version as ✓ or ✗
5. If any version fails, all descendants are implicitly invalid

**Examples:**
```bash
/VERIFY               # verify entire chain
/VERIFY "a1b2c3"      # verify up to this SIC
```

**Output (tampered):**
```
Verifying chain integrity...

  a1b2c3  initial   ✓
  d4e5f6  fix1      ✗  SIC mismatch — history may have been altered

  Chain integrity check failed — history may have been tampered.
```

### `/ROLLBACK` — Revert HEAD

```
/ROLLBACK "<sic_or_name>"
```

**Arguments:**
- `sic_or_name` — Target version to make HEAD

**Behaviour:**
1. Resolves the argument to a full SIC
2. Sets HEAD to that SIC (both in `~/.vault/HEAD` and the name registry)
3. Does **not** delete any data — orphaned versions remain in the store
4. Prints a warning that newer versions still exist

**Examples:**
```bash
/ROLLBACK "fix1"
/ROLLBACK "d4e5f6"
```

---

## Technical Details

### SIC Generation Algorithm

The **Special Identification Code (SIC)** is the core cryptographic primitive.
It is a SHA256 hash that chains every version to its parent:

```
SIC(N) = SHA256(
    parent_sic(N) + '\0' +
    tree_hash(N)  + '\0' +
    author_id(N)  + '\0' +
    timestamp(N)  + '\0' +
    name(N)
)
```

Where fields are concatenated with null byte (`\0`) separators and then hashed
with SHA256. The result is a 64-character lowercase hex string.

**Properties:**

1. **Deterministic** — The same inputs always produce the same SIC. Unit test
   `test_sic_generation` verifies this.

2. **Chained** — The SIC of version N depends on the SIC of version N-1.
   Tampering with any version changes its SIC, which changes its child's parent
   SIC input, which changes the child's SIC, and so on. The entire descendant
   chain breaks.

3. **Timestamped** — Two identical pushes at different times produce different
   SICs (timestamp differs). This prevents replay attacks.

4. **Attributed** — The author ID is baked into the SIC. A version's author is
   cryptographically bound to the version content.

5. **Named** — The human-readable name is part of the SIC input. Renaming
   changes the SIC (names are mutable aliases, but the SIC captures the name at
   creation time).

**Verification:**
```cpp
bool verify = (generate_sic(parent_sic, tree_hash, author_id, timestamp, name)
               == stored_sic);
```

### Object Store Layout

VAULT uses a content-addressed object store modelled after Git's layout:

```
~/.vault/data/
├── objects/
│   ├── a1/
│   │   ├── b2c3d4e5f6a7b8c9...   # blob file (SHA256[2:] as filename)
│   │   └── ...
│   └── ff/
│       └── ...
├── versions/
│   ├── a1b2c3d4e5f6....json      # version metadata
│   └── ...
├── names.json                     # name → SIC mapping
└── HEAD                           # current HEAD SIC (single line)
```

- **Blobs** are stored at `objects/XX/YYYY...` where XX is the first two hex
  characters of the SHA256 hash and YYYY... is the remainder. This prevents any
  single directory from having too many entries.

- **Trees** are stored as blobs (their serialised content is hashed and stored
  in the objects directory).

- **Versions** are stored as JSON-like key=value files in `versions/`.

- **`names.json`** is a flat JSON mapping of human names (including "HEAD") to
  full SICs. Names are mutable. SICs are immutable.

- **`HEAD`** is a single-line text file containing the current HEAD SIC. This is
  the only file the sender reads on every operation.

### Delta / Diffing Strategy

On every `/HOOK`, the sender computes what changed since the last push:

```cpp
struct FileDelta {
    enum class Op { ADDED, MODIFIED, DELETED };
    Op operation;
    std::string path;
    std::string blob_hash;           // new blob hash (empty if DELETED)
    std::vector<uint8_t> content;    // new content (empty if DELETED)
};
```

**Algorithm:**

1. Walk the directory recursively using `std::filesystem::recursive_directory_iterator`
2. For each file, compute SHA256 of its raw bytes
3. Compare against the hash stored in the previous version's tree
4. Only pack files where:
   - Hash differs from previous tree (MODIFIED)
   - File is new (not in previous tree) (ADDED)
   - File was in previous tree but no longer exists (DELETED)
5. **Skip** files matching `.vaultignore` patterns (glob-style, same as
   `.gitignore`)
6. **Warn** if any file exceeds 10 MB
7. **Skip** binary files (detected by null byte in first 8 KB)

The payload is packed with a custom binary format:

```
[version_sic_len:4][version_sic_data...]
[parent_sic_len:4][parent_sic_data...]
[tree_hash_len:4][tree_hash_data...]
[author_id_len:4][author_id_data...]
[name_len:4][name_data...]
[message_len:4][message_data...]
[timestamp:8]
[changed_files_count:4]
  [file_path_len:4][file_path_data...]  (repeated)
[delta_count:4]
  [delta_op:4]
  [path_len:4][path_data...]
  [blob_hash_len:4][blob_hash_data...]
  [content_len:4][content_data...]      (repeated)
```

All integer fields are in network byte order (big-endian).

### Name Registry

The name registry is a flat `name → SIC` mapping stored as JSON:

```json
{
  "initial": "a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0",
  "fix1":    "d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2c3",
  "HEAD":    "d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2c3"
}
```

**Rules:**

- **Names are mutable** — A name can be reassigned to a different SIC (like a
  branch pointer in Git, not like a tag).
- **SICs are immutable** — A SIC always refers to the same version forever.
- **HEAD is a special name** — Always points to the latest accepted version.
- **Resolution order:**
  1. If the input is `latest`, return HEAD.
  2. If the input matches a name in the registry, return its SIC.
  3. If the input is 6–64 hex characters, search for a SIC starting with those
     characters.
  4. If the input is exactly 64 hex characters, treat as a raw SIC.

### Author Identity

Each developer has an identity baked into every SIC they generate. The author
config is stored at `~/.vault/author.json`:

```json
{
  "default": {
    "id": "abcd",
    "display_name": "abcd",
    "public_key": ""
  }
}
```

The author ID is included in SIC generation:

```
SIC = SHA256(parent_sic + tree_hash + author_id + timestamp + name)
```

This means:
- Every version is permanently attributed to its author
- The attribution is cryptographically bound to the content
- Tampering with the author changes the SIC and breaks the chain
- Author identity is immutable per version — once committed, it cannot change

The receiver maintains an author registry for verification. New authors can be
registered by pushing an identity file.

### Optimistic Locking

VAULT uses a simple optimistic locking mechanism to prevent silent overwrites
in multi-user scenarios:

1. Sender sends `HOOK_INIT` with `parent_sic` = the sender's current HEAD.
2. Receiver compares `parent_sic` against its own HEAD.
3. If they **match** → push proceeds normally.
4. If they **differ** → receiver responds with `HOOK_REJECTED`:

```
HOOK_REJECTED:
  ✗ Push rejected — HEAD has moved
  Server HEAD : d4e5f6 (fix1)
  Your HEAD   : a1b2c3 (initial)
  Run /SYNC to pull latest changes before pushing.
```

The sender must then `/SYNC` to pull the latest state, resolve any conflicts,
and re-attempt the push.

This is NOT a distributed lock. It is a compare-and-swap on the HEAD pointer.
It works because the receiver is the single source of truth and the version
chain is linear.

---

## Configuration

### Receiver Config (`~/.vault/receiver.json`)

```json
{
  "port": 9000,
  "storage_path": "/mnt/nas/vault-repos/",
  "owner_key": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
  "guest_key": "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210",
  "max_repo_size_mb": 2048,
  "log_path": "/var/log/vault-receiver.log"
}
```

| Key | Default | Description |
|-----|---------|-------------|
| `port` | 9000 | TCP port for CP connections |
| `storage_path` | `/mnt/nas/vault-repos/` | Where repository data is stored |
| `owner_key` | (none) | CP authentication key for owner |
| `guest_key` | (none) | CP authentication key for guests |
| `max_repo_size_mb` | 2048 | Quota enforcement (soft limit) |
| `log_path` | `/var/log/vault-receiver.log` | Log file path |

### Relay Config (`~/.vault/relay.json`)

```json
{
  "port": 9001,
  "session_timeout_seconds": 3600,
  "max_concurrent_sessions": 50,
  "log_path": "/var/log/vault-relay.log"
}
```

| Key | Default | Description |
|-----|---------|-------------|
| `port` | 9001 | Public TCP port |
| `session_timeout_seconds` | 3600 | Drop idle sessions after this |
| `max_concurrent_sessions` | 50 | Maximum simultaneous sessions |
| `log_path` | `/var/log/vault-relay.log` | Log file path |

### Author Config (`~/.vault/author.json`)

```json
{
  "my_id": {
    "id": "abcd",
    "display_name": "abcd",
    "public_key": ""
  }
}
```

Created automatically on first run with a `"default"` author if not present.

---

## Project Structure

```
~/VAULT/
├── CMakeLists.txt                      # CMake build (3.20+)
├── config.json                         # Default receiver config
├── README.md                           # This file
│
├── include/vault/                      # Public headers
│   ├── object_store.hpp                #   Blob/tree/version storage
│   ├── sic.hpp                         #   SIC generation + verification
│   ├── version_chain.hpp               #   Linear version chain
│   ├── delta.hpp                       #   File diffing + delta packing
│   ├── name_registry.hpp               #   Human name → SIC lookup
│   ├── author_registry.hpp             #   Author identity management
│   └── relay.hpp                       #   Stateless relay server
│
├── src/
│   ├── object_store.cpp                # Content-addressed object storage
│   ├── sic.cpp                         # SHA256 hashing, SIC generation
│   ├── version_chain.cpp               # Append, rollback, verify chain
│   ├── delta.cpp                       # File diffing, .vaultignore, payload
│   ├── name_registry.cpp               # JSON-backed name ↔ SIC mapping
│   ├── author_registry.cpp             # JSON-backed author identity store
│   │
│   ├── receiver/
│   │   ├── main.cpp                    # Receiver daemon entry point
│   │   ├── receiver.hpp                # Receiver handler declarations
│   │   └── receiver.cpp                # HOOK/GET/SIC/SYNC/VERIFY/ROLLBACK handlers
│   │
│   ├── sender/
│   │   ├── main.cpp                    # Sender CLI entry point + dispatch
│   │   └── commands/
│   │       ├── hook.cpp                # /HOOK implementation
│   │       ├── get.cpp                 # /GET implementation
│   │       ├── sic_cmd.cpp             # /SIC implementation
│   │       ├── diff.cpp                # /DIFF implementation
│   │       ├── sync.cpp                # /SYNC implementation
│   │       ├── rollback.cpp            # /ROLLBACK implementation
│   │       └── verify.cpp              # /VERIFY implementation
│   │
│   └── relay/
│       ├── main.cpp                    # Relay server entry point
│       └── relay.cpp                   # Session brokering + byte forwarding
│
├── tests/
│   ├── CMakeLists.txt                  # Test build config
│   ├── test_sic.cpp                    # SIC generation + verification (8 tests)
│   ├── test_object_store.cpp           # Blob/tree/version storage (5 tests)
│   ├── test_version_chain.cpp          # Append/history/rollback/verify (5 tests)
│   └── test_delta.cpp                  # Diff/ignore/pack/unpack (6 tests)
│
└── Protocol/CP/                        # Conversational Protocol library
    ├── shared/                         #   CP shared library (packet, session, crypto, config)
    ├── sender/                         #   CP sender node implementation
    └── receiver/                       #   CP receiver node implementation
```

---

## License

MIT. Use it, modify it, share it. No warranty, no guarantee, no cloud.

*VAULT is built on top of the [CP (Conversational Protocol)](https://github.com/furinaops/CP) project.*
