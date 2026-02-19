import type { Region } from '../shared/types.js';

/** Simple region list — can be extended with latency-based routing */
export const REGIONS: Region[] = ['us-east', 'us-west', 'eu-west', 'ap-south', 'ap-east'];

/**
 * Determine the best relay server endpoint for a region.
 * In production this would query a service registry or load balancer.
 */
export function relayEndpointForRegion(_region: Region, relayBase: string): string {
  return relayBase;
}
