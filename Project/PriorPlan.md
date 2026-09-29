# Quantum API audit and Braided Quanta completion plan

## Verdict

**No—the complete Quantum API portion is not finished.** The core braid endpoint is implemented, but the braid → time evolution → Blueprint gameplay pipeline still has gaps.

Reviewed `quantum-api-topological-braid` at **`e89df40`**. The worktree is clean; no source files were changed.

| Plan requirement | Current status |
|---|---|
| Stateless Fibonacci three-anyon endpoint | Implemented |
| Generators, inverses, complex amplitudes, exact probabilities | Implemented |
| Optional measurement, counts, seeded sampling | Implemented |
| 256-operation and 4,096-shot limits | Implemented |
| Existing authentication, rate limiting, request IDs | Integrated |
| Mathematical and endpoint tests | Present; independent reference coverage remains incomplete |
| Unreal async braid node | Present, but returns raw JSON |
| Blueprint-friendly braid result and dedicated time-evolution node | Missing |
| Braid state passed into time evolution | Missing |
| Unity typed braid client | Implemented; runtime parity testing remains |
| Godot braid client | Added to SDK script, missing from distributable addon |
| RX/RZ, compile endpoint, arbitrary unitary | Not implemented; explicitly later roadmap work |

The concrete blockers are visible in the [time-evolution input model](D:/SoftwareDev/APIs/quantum-api-i2Workspace/quantum-api-topological-braid/src/quantum_api/models/algorithms.py:296), which requires a circuit, and the [Unreal async node](D:/SoftwareDev/APIs/quantum-api-i2Workspace/quantum-api-topological-braid/sdk/unreal/Source/QuantumApi/Public/QuantumApiAsyncActions.h:96), which returns JSON.

## Finish the API and plugin integration

1. **Support state continuity.** Add optional `initial_statevector` to Trotter time evolution, mutually exclusive with its existing circuit input. Validate finite, normalized amplitudes and dimensions against the Hamiltonian and existing qubit limits. Preserve existing requests and return exact basis probabilities alongside the final statevector.
2. **Expose typed Unreal results.** Add Blueprint structs and async actions for braid evaluation and time evolution, including amplitudes, probabilities, measurement information, and structured errors. Preserve the existing JSON interfaces.
3. **Complete verification and documentation.** Add published reference vectors, convention-aware TQSim comparisons, long-braid normalization tests, measurement defaults, boundary cases, and braid-to-evolution integration tests. Document chronological execution order and the actual `real`/`imag` contract; replace placeholder numerical examples.
4. **Finish hardening.** Verify authentication and throttling specifically on the braid route; add an explicit request-body ceiling and JSON content-type enforcement where absent.
5. **Close SDK parity separately from the game blocker.** Update the Godot addon, add shared fixtures and runnable Unity/Godot examples, and verify their responses in-engine. Keep RX/RZ, compilation, and arbitrary unitary support outside the first game slice.

Source completion does not establish deployment: verify the intended server has the updated routes and required time-evolution dependencies before connecting the game.

## Build the selected three-minute, 2.5D game slice

The [Unreal project](</C:/Users/DJLeg/Documents/Unreal Projects/Braided_Quanta2026/Braided_Quanta2026.uproject>) already has C++ infrastructure and useful combat templates: combos, charged attacks, damage, enemy StateTree AI, health bars, and checkpoints. It currently starts in the Third Person template map. No custom quantum integration, braid history, boss system, or QTE was found in source/configuration.

Use `Braided_Quanta2026.sln`; the supplied `Automation_...sln` contains Unreal automation tooling.

1. **Connect the plugin.** Install the completed plugin into the game, enable it, add the module dependency, and compile against installed UE **5.8.3**. Use backend-proxy configuration for distributed builds. Preserve the existing Diversion workspace and pending files.
2. **Create the combat slice.** Extend the combat variant with a fixed side camera, bounded movement lane, dodge, one short encounter, and two branching exits. Keep presentation, animation, tuning, and level scripting in Blueprint.
3. **Persist route history.** Add a C++ GameInstance subsystem and SaveGame support for ordered braid operations, completed choices, checkpoint restoration, and restart. Each exit records its choice once.
4. **Make choices mathematically meaningful.** The report’s `σ₁± → σ₂±` example produces identical initial probabilities for all four routes. Use this verified replacement: fixed preparation `[σ₂, σ₁⁻¹]`, choice one `σ₁±`, choice two `σ₂±`, fixed readout `[σ₁, σ₂⁻¹]`. Show every crossing in the braid visualization. The four routes produce approximately **9.0%, 61.8%, 94.4%, and 74.3% tau**.
5. **Add the quantum boss component.** Evaluate the complete braid at entrance, retain its complex state, and expose probabilities to Blueprint. Collect combat telemetry and perform one phase-boundary evolution using `H = aX + bZ`. Tune coefficients through a data asset; never request on Tick. Reject stale callbacks after death, restart, or level changes.
6. **Add one QTE.** Use the latest completed evolution probabilities, sample locally once, and record the collapsed basis state. Player timing determines success; the sampled channel selects the boss consequence. The visible QTE must not wait for HTTP.
7. **Finish the demo.** Add boss telegraphs, braid HUD, collapse effects, sound, victory/death/retry flows, controller prompts, and a clearly labelled prerecorded offline mode. Package Win64 and verify a complete three-minute playthrough.

## Validation and completion criteria

**Audit results:** 60 selected tests passed; two skipped because `nbformat` and a final Fab ZIP were unavailable. Redis/JWKS startup dependencies were isolated for the local run. Unreal compilation, Blueprint execution, deployed-service behavior, and packaged gameplay were not verified.

Before declaring completion:

- Preserve complex phase through braid → evolution → subsequent evolution; test malformed states and backward compatibility.
- Verify the four route outcomes, save/load, duplicate exit prevention, and deterministic restart.
- Exercise timeout, unavailable service, malformed response, stale callback, and offline presentation behavior.
- Compile the plugin and game, test typed Blueprint nodes, and complete the packaged combat → choices → boss → evolution → QTE loop.

**Defaults:** single-player Win64, C++ foundations with Blueprint gameplay, one encounter, two choices, one boss evolution event, and one QTE. General anyon simulation and IBM hardware execution remain outside this slice.
