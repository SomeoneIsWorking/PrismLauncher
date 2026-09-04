# Project goals

This document records durable product intent. Current capability evidence is in
`docs/project-state.md`; atomic work is in `docs/issues/`.

## G001 — Dependable Minecraft instance management

**Outcome.** Players can create, configure, launch, import, and export Minecraft
instances through a coherent desktop launcher.

**Why it matters.** Instances are the player's durable game environments; their
configuration and data must remain usable across ordinary launcher workflows.

**Observable success conditions.** The launcher manages a selected instance,
launches it with its declared Minecraft components, and retains the existing
archive import and export workflows.

**Constraints and non-goals.** This goal does not replace Minecraft itself or
require a hosted account service.

**Contributing state items:** S001, S002.

## G002 — Automatic local-network instance import

**Outcome.** A player can open Add Instance -> Import from LAN, see the
instances in other running Prism Launchers on the same LAN, and import one
without any sender-side sharing action.

**Why it matters.** Family members should be able to share an instance without
manually locating directories, copying files, or involving a cloud service.

**Observable success conditions.**

- Every running launcher automatically advertises its instance catalogue on the
  LAN without requiring an instance to be selected or shared first.
- The receiving launcher lists the discovered instances under Add Instance ->
  Import from LAN and can request an import directly.
- A requested transfer is prepared on demand, uses an unguessable bounded
  capability scoped to that archive, and expires automatically.
- The existing importer remains the sole owner of archive validation and
  instance creation.

**Constraints and non-goals.** No sender-side share toggle, share wizard, cloud
relay, account exchange, permanent archive publication, or alternate archive
parser is in scope. A live instance may be listed but must not be archived while
it is running.

**Contributing state items:** S003, S004, S005.

## G003 — Maintainable launcher ownership

**Outcome.** New launcher responsibilities have cohesive modules, are documented
at their ownership boundary, and have focused verification.

**Why it matters.** Instance workflows connect UI, networking, archives, and
filesystem state; unclear ownership causes regressions and duplicate formats.

**Observable success conditions.** Each responsibility has one owner, existing
import/export contracts remain exercised, and LAN transfer behavior has both
positive and negative verification.

**Constraints and non-goals.** This does not require rewriting unrelated Prism
subsystems or inventing a second network stack.

**Contributing state items:** S002, S005, S006.
