# Security Policy

## Supported Versions

Security fixes target the latest tagged release. Unreleased branches, including
planned 4.0.0 work, are development snapshots and are not published releases.
Older release lines may receive fixes only when explicitly announced.

## Reporting a Vulnerability

If you discover a security vulnerability in this library, report it privately:

1. Do not open a public issue before a fix or disclosure plan is agreed.
2. Contact the maintainer using the address in `library.json`.
3. Include the affected version/commit, target MCU/framework, reproduction
   steps, expected and observed behavior, and potential impact.
4. Do not attach credentials, production bus captures, or proprietary firmware
   unless a secure transfer method has been agreed first.

No fixed response or remediation SLA is promised for this volunteer-maintained
project. Reports will be triaged according to reproducibility and impact.

## Scope

The framework-neutral core owns no network stack, filesystem, credentials,
persistent storage, I2C bus, pins, locks, reset GPIO, retries, or recovery.
Security-relevant defects within scope include:

- memory corruption or out-of-bounds access through public APIs;
- unbounded blocking contrary to documented callback/deadline contracts;
- transaction replay or hidden I2C access that violates owner isolation;
- malformed-length handling in drawing, framebuffer, or raw-command APIs; and
- package or generated-version behavior that can misrepresent shipped code.

Application transport, bus arbitration, physical access, reset sequencing,
watchdogs, privilege separation, and validation of externally sourced display
content remain application responsibilities.

## Security Best Practices for Users

- Treat configuration, text lengths, bitmap lengths, raw commands, and absolute
  deadlines derived from external input as untrusted.
- Keep the transport callback synchronous and timeout-bounded; permit at most
  one physical transaction and never replay an ambiguous OLED write.
- Serialize every driver call in the application bus owner. Public APIs are not
  thread-safe or ISR-safe.
- Prefer caller-owned fixed framebuffer storage for deterministic production
  memory and keep it valid until detach/end.
- Use hardware watchdogs and platform I2C timeouts appropriate to the product.
- Pin reviewed dependency revisions and retain exact build/version evidence.
