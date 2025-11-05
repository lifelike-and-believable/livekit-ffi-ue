# LiveKit Token Minting Utility

This repo includes a small CLI and PowerShell wrapper to mint LiveKit access tokens for development, with configurable TTL so you don’t have to regenerate daily.

## Prerequisites
- Node.js 18+
- LiveKit Server API credentials:
  - `LIVEKIT_API_KEY`
  - `LIVEKIT_API_SECRET`

## One‑time setup
From the repo root, dependencies will be auto‑installed by the wrapper on first use. If you prefer manual install:

```pwsh
cd tools/token-mint
npm install
```

## Quick start
PowerShell wrapper (recommended on Windows):

```pwsh
$env:LIVEKIT_API_KEY = "your_key"
$env:LIVEKIT_API_SECRET = "your_secret"
./tools/mint-token.ps1 -Identity user1 -Room demo -Ttl 168h
```

Direct Node CLI (cross‑platform):

```pwsh
$env:LIVEKIT_API_KEY = "your_key"; $env:LIVEKIT_API_SECRET = "your_secret"
node tools/token-mint/index.js --identity user1 --room demo --ttl 168h
```

The CLI returns a signed JWT you can paste into Unreal (`Token` field).

## Arguments
- `--identity <id>`: Participant identity (required)
- `--name <display>`: Optional display name
- `--room <name>`: Room name (required)
- `--ttl <dur>`: Lifetime (defaults to `24h`). Accepts seconds (`3600`) or `Xs|Xm|Xh|Xd|Xw` (e.g., `90m`, `7d`).
- `--publish` / `--no-publish`: Allow publishing tracks (default on)
- `--subscribe` / `--no-subscribe`: Allow subscribing (default on)
- `--publishData` / `--no-publishData`: Allow data messages (default on)
- `--json`: Output JSON instead of plain token

PowerShell wrapper parameters:
- `-Identity`, `-Room`, `-Ttl`, `-Name`, `-NoPublish`, `-NoSubscribe`, `-NoPublishData`, `-Json`

## Examples
- Long‑lived dev token (7 days):
  ```pwsh
  ./tools/mint-token.ps1 -Identity alice -Room demo -Ttl 168h
  ```
- Data‑only bot for a room, 1 hour, JSON output:
  ```pwsh
  ./tools/mint-token.ps1 -Identity simbot -Room demo -Ttl 3600 -NoPublish -Json
  ```

## Notes & best practices
- Use only the grants you need (publish/subscribe/data) and scope to a specific `room`.
- Prefer medium TTL in development (24–168h). In production, prefer short TTLs and refresh on demand.
- Tokens are bearer credentials; protect them as secrets.

## How it works
The CLI uses `@livekit/server-sdk` to construct an `AccessToken` with the provided grants and sign it using your API key/secret. TTL sets the JWT expiration.
