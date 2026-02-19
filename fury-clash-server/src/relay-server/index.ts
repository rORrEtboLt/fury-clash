import { WebSocketServer } from 'ws';
import http from 'http';
import { RoomManager } from './room-manager.js';
import { config } from '../shared/config.js';
import { childLogger } from '../shared/logger.js';
import { connectDb } from '../api-server/plugins/db.js';

const log = childLogger('relay');

const SKIP_DB = process.env['RELAY_SKIP_DB'] === '1';

async function main() {
  if (SKIP_DB) {
    log.warn('RELAY_SKIP_DB=1 — skipping DB and Redis (standalone/dev mode)');
  } else {
    await connectDb();
  }

  const manager = new RoomManager(SKIP_DB);

  if (!SKIP_DB) {
    await manager.init();
  }

  /* HTTP server for health checks and room creation (called by matchmaker) */
  const httpServer = http.createServer((req, res) => {
    if (req.method === 'GET' && req.url === '/health') {
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ status: 'ok', rooms: manager.roomCount() }));
      return;
    }

    if (req.method === 'POST' && req.url === '/rooms') {
      let body = '';
      req.on('data', (c) => { body += c; });
      req.on('end', () => {
        try {
          const cfg = JSON.parse(body);
          const room = manager.createRoom(cfg);
          res.writeHead(201, { 'Content-Type': 'application/json' });
          res.end(JSON.stringify({ roomId: room.roomId }));
        } catch (err: unknown) {
          res.writeHead(400);
          res.end(JSON.stringify({ error: String(err) }));
        }
      });
      return;
    }

    res.writeHead(404);
    res.end();
  });

  const wss = new WebSocketServer({ server: httpServer });
  wss.on('connection', (ws) => manager.handleNewConnection(ws));
  wss.on('error', (err) => log.error(err, 'WSS error'));

  httpServer.listen(config.relay.port, config.relay.host, () => {
    log.info({ port: config.relay.port }, 'Relay server listening');
  });

  process.on('SIGTERM', async () => {
    log.info('SIGTERM — shutting down relay');
    httpServer.close();
    process.exit(0);
  });
}

main().catch((err) => {
  log.error(err, 'Fatal relay startup error');
  process.exit(1);
});
