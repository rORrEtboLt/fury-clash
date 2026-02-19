import type WebSocket from 'ws';
import { RelayRoom, type RoomConfig } from './relay-room.js';
import { DisconnectHandler } from './disconnect-handler.js';
import { decodeClientMsg } from './protocol.js';
import { MsgType, type MatchEndPayload } from '../shared/types.js';
import { Queue } from 'bullmq';
import { QUEUE_ELO, QUEUE_REPLAY, KEY_RELAY_ROOMS } from '../shared/constants.js';
import { config, bullmqConnection } from '../shared/config.js';
import { childLogger } from '../shared/logger.js';
import { createRedisClient } from '../api-server/plugins/redis.js';
import type { Redis } from 'ioredis';
import { prisma } from '../api-server/plugins/db.js';
import type { EloJobData, ReplayJobData } from '../shared/types.js';

const log = childLogger('room-manager');

export class RoomManager {
  private rooms       : Map<string, RelayRoom> = new Map();
  private disconnectH : DisconnectHandler;
  private eloQueue    : Queue | null    = null;
  private replayQueue : Queue | null    = null;
  private redis       : Redis | null    = null;
  private readonly skipDb: boolean;

  constructor(skipDb = false) {
    this.skipDb = skipDb;

    if (!skipDb) {
      this.redis       = createRedisClient('room-manager');
      this.eloQueue    = new Queue(QUEUE_ELO,    { connection: bullmqConnection() });
      this.replayQueue = new Queue(QUEUE_REPLAY, { connection: bullmqConnection() });
    }

    this.disconnectH = new DisconnectHandler(
      config.relay.disconnectGraceSec,
      (room, loserSlot) => {
        const winnerSlot = (loserSlot ^ 1) as 0 | 1;
        room.end({
          winnerSlot,
          reason        : 'forfeit',
          p1RoundsWon   : loserSlot === 0 ? 0 : 2,
          p2RoundsWon   : loserSlot === 1 ? 0 : 2,
          durationFrames: 0,
        });
      },
    );
  }

  async init(): Promise<void> {
    if (this.redis) {
      await this.redis.connect();
    }
    log.info('RoomManager initialised');
  }

  createRoom(cfg: Omit<RoomConfig, 'onEnd' | 'pingIntervalMs'>): RelayRoom {
    if (this.rooms.size >= config.relay.maxRooms) throw new Error('Relay at capacity');

    const room = new RelayRoom({
      ...cfg,
      pingIntervalMs: config.relay.pingIntervalMs,
      onEnd: (r, payload) => this.handleRoomEnd(r, payload),
    });

    this.rooms.set(room.roomId, room);
    this.redis?.sadd(KEY_RELAY_ROOMS(config.relay.serverId), room.roomId).catch(() => {});
    log.info({ roomId: room.roomId }, 'Room created');
    return room;
  }

  handleNewConnection(ws: WebSocket): void {
    ws.once('message', (data) => {
      const msg = decodeClientMsg(data as Buffer);
      if (!msg || msg.type !== MsgType.CLIENT_JOIN) {
        ws.close(4000, 'Expected CLIENT_JOIN first');
        return;
      }

      const room = this.rooms.get(msg.roomId);
      if (!room) { ws.close(4004, 'Room not found'); return; }

      const slot = msg.slot;
      const expectedPlayerId = slot === 0 ? room.cfg.p1Id : room.cfg.p2Id;
      if (msg.playerId !== expectedPlayerId) {
        ws.close(4003, 'Wrong player for this slot');
        return;
      }

      const fighterId = slot === 0 ? room.cfg.p1Fighter : room.cfg.p2Fighter;
      if (!room.attach(slot, ws, msg.playerId, fighterId)) {
        ws.close(4001, 'Slot already taken');
        return;
      }

      this.disconnectH.cancel(room, slot);
    });
  }

  private async handleRoomEnd(room: RelayRoom, payload: MatchEndPayload): Promise<void> {
    this.disconnectH.clear(room);
    this.rooms.delete(room.roomId);
    this.redis?.srem(KEY_RELAY_ROOMS(config.relay.serverId), room.roomId).catch(() => {});

    if (this.skipDb) {
      log.info({ roomId: room.roomId }, 'RELAY_SKIP_DB: skipping match persistence');
      return;
    }

    const { p1Id, p2Id, p1Fighter, p2Fighter } = room.cfg;

    try {
      const match = await prisma.match.create({
        data: {
          player1Id  : p1Id,
          player2Id  : p2Id,
          winnerId   : payload.winnerSlot === 0 ? p1Id : payload.winnerSlot === 1 ? p2Id : null,
          p1Fighter,
          p2Fighter,
          p1EloChange: 0,
          p2EloChange: 0,
          p1EloAfter : 0,
          p2EloAfter : 0,
          rounds     : [],
          region     : 'us-east',
          durationSec: room.durationSec(),
          endReason  : payload.reason,
        },
      });

      const [p1, p2] = await Promise.all([
        prisma.player.findUnique({ where: { id: p1Id }, select: { elo: true, totalGames: true } }),
        prisma.player.findUnique({ where: { id: p2Id }, select: { elo: true, totalGames: true } }),
      ]);

      const eloJob: EloJobData = {
        matchId: match.id, p1Id, p2Id,
        winnerId    : match.winnerId,
        p1Elo       : p1?.elo ?? 1000,
        p2Elo       : p2?.elo ?? 1000,
        p1TotalGames: p1?.totalGames ?? 0,
        p2TotalGames: p2?.totalGames ?? 0,
      };
      await this.eloQueue!.add('update-elo', eloJob);

      const replayJob: ReplayJobData = {
        matchId : match.id,
        roomId  : room.roomId,
        inputLog: room.inputLog.toArray(),
      };
      await this.replayQueue!.add('store-replay', replayJob);

    } catch (err) {
      log.error({ err, roomId: room.roomId }, 'Failed to persist match');
    }
  }

  roomCount(): number { return this.rooms.size; }
}
