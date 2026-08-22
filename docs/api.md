# Pawn API

Include `omp-drip.inc` in the gamemode. All authoritative outfit state belongs
to the server; the ASI only renders states accepted from the component.

## `Drip_IsClientReady(playerid)`

Returns non-zero after the client handshake succeeds.

## `Drip_SetPlayerAppearance(playerid, item_ids[18], muscle, fat)`

Replaces the authoritative appearance and broadcasts it to the player and
current stream viewers. Each non-zero ID must belong to the same slot index in
the generated catalog. `muscle` and `fat` use GTA's `0..1000` range.

The component does not persist this array. Save it in your own database and
restore it after login.

## `Drip_SetPlayerPreview(playerid, slot, item_id)`

Temporarily replaces one slot on the target player's own client. A preview is
not broadcast and does not mutate authoritative state.

## `Drip_ClearPlayerPreview(playerid)`

Removes the fitting-room preview and restores authoritative state.

## `Drip_ResyncPlayer(playerid)`

Resends the target player's state and all appearances currently streamed to
that player.

## Callbacks

```pawn
public OnDripClientReady(playerid, protocol_version);
public OnDripClientRejected(playerid, reason);
```

The rejection constants are declared in `omp-drip.inc`. Rejected players are
kicked by the component after the callback.

## Gamemode responsibilities

- item ownership and pricing;
- persistence of 18 item IDs per player;
- validation that an item is enabled and belongs to the requested slot;
- shop/dialog/CEF UX and preview camera;
- purchase transactions and rollback;
- defaults and profession/faction restrictions.
