# BUG: parser `_expect()` doesn't advance on error → potential infinite loop

## Status
**Defensive fix applied 2026-04-20** — `_expect()` now advances one token on mismatch.
Original reported symptom (hang on `pub` prefix in impl-block method) is **not
reproducible** on current HEAD with the self-hosted compiler. The fix is latent
protection against the described failure mode.

## Original symptom
Aria bootstrap parser hangs on malformed input. Reproducer (claimed): a `pub`
prefix on an impl-block method puts the parser into an infinite loop instead of
producing a diagnostic.

## Root cause (now fixed)
`src/parser/parser.aria:50` — `_expect()` reported an error but did not advance
the cursor. Any caller that re-entered `_expect()` in a loop would spin at the
same `pos`.

## Fix
On mismatch, `_expect()` now advances `pos` by one (unless already at EOF) *in
addition to* recording the diagnostic. This guarantees forward progress for all
callers, even ones that retry.

## Related (not fixed here — separate issue)
The self-hosted `check` pipeline uses `build_decl_index` (not the full
recursive-descent `parse`) and tolerates many kinds of malformed top-level
syntax without reporting errors. Files like `@@@garbage` build into empty,
do-nothing binaries. This is a **different** bug (decl-index permissiveness)
and was not addressed here.
