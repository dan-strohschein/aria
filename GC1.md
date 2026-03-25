# Aria GC Architecture Decision: Generational Stop-the-World

## The Upgrade Path: Mark-Sweep → Spec Compliance

The spec calls for: **concurrent, generational, low-pause, compacting**.

The critical architectural decision is **write barriers** — and this is what determines how hard the upgrade is.

**What's a write barrier?** Every time a pointer is stored into an object (e.g., a struct field assignment, array element set), the GC needs to be notified. Generational GC needs this to track when old-generation objects point to young-generation objects. Concurrent GC needs this to maintain consistency during marking.

Here's the problem with starting simple:

| Start with | Write barriers? | Upgrade to generational | Upgrade to concurrent |
|---|---|---|---|
| Mark-sweep | No | **Hard** — must add barriers to ALL pointer stores in codegen | **Harder** — barriers + tri-color marking |
| Reference counting | No | **Rewrite** — completely different approach | **Rewrite** |
| Generational stop-the-world | Yes (built in) | Already done | **Moderate** — change the sweep algorithm, keep barriers |

If you start with mark-sweep **without** write barriers, upgrading to generational later means going back to the lowerer/LLVM codegen and adding a barrier call at every pointer store. That's essentially touching every struct construction, every array set, every variable assignment that involves a heap pointer. It's a full rearchitecture of the codegen's memory model.

If you start with generational **with** write barriers from day 1, then:
- The barriers are designed into the codegen once
- Upgrading to concurrent is changing the GC **algorithm** (the collector itself), not the **integration** (the compiler output)
- Compaction comes free if you use a copying collector for the young generation

## Recommendation: Start Generational, Stop-the-World

Build a **stop-the-world generational copying collector**. This gives you 3 of the 4 spec requirements immediately:

| Spec Requirement | Day 1 | Later |
|---|---|---|
| Generational | **yes** — young/old generations | -- |
| Compacting | **yes** — copying collector compacts by nature | -- |
| Low-pause | **yes** — young gen is small, collects in <1ms | -- |
| Concurrent | no — stop-the-world | Add concurrent marking phase |

Here's what this looks like concretely:

### Young generation (nursery)

- A fixed-size bump allocator (e.g., 4MB)
- All new allocations go here (fast — just bump a pointer)
- When full, do a minor GC: copy live objects to old generation, reset nursery
- Minor GC is fast because nursery is small and most objects are dead

### Old generation

- Objects that survive 2+ minor GCs get promoted here
- Major GC (mark-sweep or mark-compact) runs when old gen grows past threshold
- Major GC is slower but runs infrequently

### Write barrier

- When a pointer store happens (old gen object → young gen object), record it in a "remembered set"
- During minor GC, the remembered set tells us which young objects are reachable from old gen
- In codegen: emit `_aria_gc_write_barrier(obj_ptr, field_offset, new_value)` before every pointer store

## Implementation Effort

The work breaks down into:

### 1. Runtime changes (~500 lines)

- Nursery allocator (bump pointer, 4MB)
- Minor GC (Cheney's copying algorithm — very simple, ~100 lines)
- Old generation allocator
- Major GC (mark-sweep)
- Write barrier function
- Root scanning (stack walking)

### 2. Compiler changes (lowerer + LLVM codegen)

- Emit write barriers before pointer stores to struct fields
- Emit stack frame maps (which locals are pointers) for root scanning
- Route all allocations through nursery instead of `_aria_alloc`

### 3. The hard part: root scanning

The GC needs to know what's on the stack. Two approaches:

- **Conservative** (Boehm-style): Scan the entire stack, treat anything that looks like a heap pointer as a root. No compiler changes needed, but can have false retention.
- **Precise** (stack maps): The compiler emits metadata saying "at this GC safepoint, these stack slots are pointers." More correct, but requires LLVM codegen changes.

For a first version, **conservative scanning is fine** and dramatically simpler. You can upgrade to precise later.

## vs. Just Doing Reference Counting

Reference counting would be simpler to implement right now (no root scanning, no write barriers), but:
- It's a dead end — the spec describes tracing GC, not refcounting
- It doesn't handle cycles (uncommon in Aria, but possible)
- Upgrading from refcounting to tracing GC is a **full rewrite**, not an upgrade
- Every assignment needs inc/dec calls (similar overhead to write barriers, but different semantics)

## Bottom Line

**Start generational.** The extra upfront work (write barriers, nursery) is modest compared to a mark-sweep, and it avoids a painful rewrite later. The key implementation order:

1. Nursery bump allocator + minor GC with conservative stack scanning
2. Write barriers in codegen
3. Old gen with major GC
4. (Later) Concurrent marking

## Implementation Plan

### Phase 1: Nursery + Minor GC (conservative scanning)
- Runtime: nursery bump allocator, Cheney's copying collector, conservative root scan
- All allocations go through nursery
- When nursery fills, stop the world, scan stack conservatively, copy live objects to old gen
- Old gen uses simple mark-sweep initially

### Phase 2: Write barriers in codegen
- Lowerer emits write barrier calls before pointer stores to struct fields
- LLVM codegen generates the barrier function calls
- Remembered set tracks old→young pointers

### Phase 3: Verify
- Self-compile (gen1 → gen2)
- Run all examples
- Memory should be dramatically reduced for `build` phase

### Phase 4 (future): Concurrent marking
- Add tri-color marking
- Run marking phase concurrently with application
- Keep write barriers (already in place from Phase 2)
