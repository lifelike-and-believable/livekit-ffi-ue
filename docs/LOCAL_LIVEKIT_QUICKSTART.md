# LiveKit Local Server Quickstart

This guide gets a local LiveKit media server running on your machine and shows how to create a room token for testing with the Unreal plugin.

## Prerequisites

- Windows 10/11, macOS, or Linux
- Docker Desktop (recommended) OR the standalone `livekit-server` binary
- Node.js 18+ (only if you want to generate tokens via a one-liner)

## Option A: Run with Docker (recommended)

Open a terminal and start the server in dev mode:

```powershell
# Windows PowerShell / pwsh
docker run --rm --name livekit -p 7880:7880 -p 7881:7881 -p 7882:7882/udp livekit/livekit-server start --dev
```

Notes:
- `--dev` launches with sensible defaults for local testing and auto-generates API key/secret.
- Keep this terminal open; the server prints the API Key and Secret on startup.
- If prompted by Windows Firewall, allow access for UDP 7882.

## Option B: Run the standalone binary

Download the `livekit-server` release for your OS from the LiveKit GitHub releases and run:

```powershell
# Same ports and flags as Docker
livekit-server start --dev --bind 0.0.0.0
```

## Option C: Run via VS Code task

If you're using this repo in VS Code, you can start the server with the built-in task:

```text
Terminal → Run Task → LiveKit Server (Dev)
```

Notes:
- The task runs `tools/livekit-server.exe start --dev --bind 0.0.0.0` and keeps it running in a dedicated terminal.
- View logs in the “LiveKit Server (Dev)” terminal. To stop, use “Tasks: Terminate Task”.
- On first run, allow Windows Firewall prompts for ports 7880/TCP, 7881/TCP, and 7882/UDP.

## Ports you need

- 7880 (TCP): HTTP API + WebSocket signaling (use this in your Room URL)
- 7881 (TCP): Optional TCP fallback transport for media
- 7882 (UDP): Media (RTP/RTCP)

For local testing on one machine, these are sufficient. No TURN server is needed.

## Generate a room token

In `--dev` mode, the server prints an API Key and Secret to the console (e.g., `API Key: devkey`, `Secret: ...`). Use them to create a JWT token that grants room join permissions.

### Quick one-liner (Node.js)

```powershell
# Set your dev API key + secret from the server logs
$env:LK_API_KEY="<your-dev-api-key>"
$env:LK_API_SECRET="<your-dev-api-secret>"

# Install the LiveKit server SDK
npm init -y > $null
npm i livekit-server-sdk > $null

# Generate a token for identity "ue_tester" joining room "test"
node -e "const { AccessToken, VideoGrant } = require('livekit-server-sdk'); const at = new AccessToken(process.env.LK_API_KEY, process.env.LK_API_SECRET, { identity: 'ue_tester' }); at.addGrant(new VideoGrant({ roomJoin: true, room: 'test', canPublish: true, canSubscribe: true })); console.log(at.toJwt());"
```

Copy the printed token for the next step.

## Configure the Unreal plugin

In your actor with `ULiveKitPublisherComponent`:
- Room URL: `ws://127.0.0.1:7880` (use `ws://` locally; `wss://` requires TLS)
- Token: paste the JWT you generated
- Optionally set the component to connect on BeginPlay

Play In Editor. On success, you should see:

```
LogTemp: Display: Loaded LiveKit FFI DLL: .../livekit_ffi.dll
# ...
[Optional] Join/room logs depending on your setup
```

## LAN or other devices

To connect from another device on your network:
- Use `ws://<host-ip>:7880` for Room URL.
- Ensure your firewall allows inbound 7880/TCP, 7881/TCP, and 7882/UDP.
- For NAT traversal beyond your LAN, configure a TURN server (not needed for same‑host testing).

## Troubleshooting

- "Failed to parse the url": ensure Room URL is `ws://127.0.0.1:7880` (no `wss://` unless TLS is configured).
- No media flowing: check Windows Firewall for UDP 7882 and that the container is running with the port published.
- Token errors: regenerate the token with the correct API key/secret and matching room name (e.g., `test`).
- DLL load failures in UE: confirm the plugin staged `livekit_ffi.dll` under `Plugins/LiveKitBridge/Binaries/Win64` and that the Editor log shows it loaded.

## Stopping the server

- Docker: press Ctrl+C in the terminal where it’s running (or `docker stop livekit`).
- Standalone: press Ctrl+C in the terminal.
