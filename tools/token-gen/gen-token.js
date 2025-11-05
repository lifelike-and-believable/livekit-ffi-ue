const { AccessToken, VideoGrant } = require('livekit-server-sdk');

const key = process.env.LK_API_KEY;
const secret = process.env.LK_API_SECRET;
const identity = process.env.LK_IDENTITY || 'ue_tester';
const room = process.env.LK_ROOM || 'test';
const canPublish = (process.env.LK_CAN_PUBLISH || 'true') === 'true';
const canSubscribe = (process.env.LK_CAN_SUBSCRIBE || 'true') === 'true';

if (!key || !secret) {
  console.error('Missing LK_API_KEY or LK_API_SECRET');
  process.exit(2);
}

const at = new AccessToken(key, secret, { identity });
at.addGrant(new VideoGrant({ roomJoin: true, room, canPublish, canSubscribe }));
console.log(at.toJwt());
