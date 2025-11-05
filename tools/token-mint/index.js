#!/usr/bin/env node
/*
  LiveKit Token Minting CLI
  Usage:
    LIVEKIT_API_KEY=key LIVEKIT_API_SECRET=secret mint-livekit-token --identity user1 --room myroom --ttl 168h --publish --subscribe --publishData
  Or on Windows PowerShell:
    $env:LIVEKIT_API_KEY="key"; $env:LIVEKIT_API_SECRET="secret"; node tools/token-mint/index.js --identity user1 --room myroom --ttl 168h
*/

const { AccessToken, VideoGrant } = require('@livekit/server-sdk');

function parseArgs(argv) {
  const out = { publish: true, subscribe: true, publishData: true, json: false };
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    const next = i + 1 < argv.length ? argv[i + 1] : undefined;
    const takes = (v) => { if (next === undefined) throw new Error(`missing value for ${a}`); i++; return next; };
    switch (a) {
      case '--identity': out.identity = takes(next); break;
      case '--name': out.name = takes(next); break;
      case '--room': out.room = takes(next); break;
      case '--ttl': out.ttl = takes(next); break;
      case '--publish': out.publish = true; break;
      case '--no-publish': out.publish = false; break;
      case '--subscribe': out.subscribe = true; break;
      case '--no-subscribe': out.subscribe = false; break;
      case '--publishData': out.publishData = true; break;
      case '--no-publishData': out.publishData = false; break;
      case '--json': out.json = true; break;
      case '--help': out.help = true; break;
      default:
        if (a.startsWith('-')) throw new Error(`unknown arg: ${a}`);
    }
  }
  return out;
}

function printHelp() {
  console.log(`Mint a LiveKit token

Required env:
  LIVEKIT_API_KEY, LIVEKIT_API_SECRET

Args:
  --identity <id>        Identity for the participant (required)
  --name <display>       Optional display name
  --room <name>          Room name (required for room-scoped grants)
  --ttl <dur>            Token lifetime (e.g. 24h, 7d, or seconds). Default 24h
  --publish/--no-publish Allow publishing tracks (default on)
  --subscribe/--no-subscribe Allow subscribing (default on)
  --publishData/--no-publishData Allow data (default on)
  --json                 Output JSON instead of plain token
  --help                 Show this help

Examples:
  mint-livekit-token --identity user1 --room demo --ttl 168h
  mint-livekit-token --identity sim-bot --room demo --ttl 3600 --no-subscribe
`);
}

function parseTtl(ttl) {
  if (!ttl) return 24 * 3600; // 24h default seconds
  if (/^\d+$/.test(ttl)) return parseInt(ttl, 10);
  const m = /^([0-9]+)\s*([smhdw])$/.exec(ttl);
  if (!m) throw new Error(`invalid ttl: ${ttl}`);
  const n = parseInt(m[1], 10);
  const unit = m[2];
  switch (unit) {
    case 's': return n;
    case 'm': return n * 60;
    case 'h': return n * 3600;
    case 'd': return n * 86400;
    case 'w': return n * 604800;
    default: throw new Error(`invalid ttl unit: ${unit}`);
  }
}

(async () => {
  try {
    const args = parseArgs(process.argv);
    if (args.help) { printHelp(); process.exit(0); }
    const apiKey = process.env.LIVEKIT_API_KEY;
    const apiSecret = process.env.LIVEKIT_API_SECRET;
    if (!apiKey || !apiSecret) {
      throw new Error('LIVEKIT_API_KEY and LIVEKIT_API_SECRET must be set');
    }
    if (!args.identity) throw new Error('--identity is required');
    if (!args.room) throw new Error('--room is required');

    const ttlSeconds = parseTtl(args.ttl);

    const at = new AccessToken(apiKey, apiSecret, {
      identity: args.identity,
      name: args.name,
      ttl: ttlSeconds,
    });

    const grant = new VideoGrant({
      room: args.room,
      roomJoin: true,
      canPublish: !!args.publish,
      canSubscribe: !!args.subscribe,
      canPublishData: !!args.publishData,
    });
    at.addGrant(grant);

    const jwt = await at.toJwt();
    if (args.json) {
      console.log(JSON.stringify({ token: jwt, identity: args.identity, room: args.room, ttlSeconds }, null, 2));
    } else {
      console.log(jwt);
    }
  } catch (err) {
    console.error(`[mint-livekit-token] ${err.message || err}`);
    process.exit(1);
  }
})();
