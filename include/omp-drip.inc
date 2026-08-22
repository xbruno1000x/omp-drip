#if defined _omp_drip_included
    #endinput
#endif
#define _omp_drip_included

#define DRIP_SLOT_COUNT (18)

// Client handshake state.
native Drip_IsClientReady(playerid);

// Applies the complete appearance and synchronizes it with streamed players.
// item_ids must contain one catalog ID per GTA clothing slot. Use 0 to clear
// optional slots. muscle and fat use GTA's 0..1000 range.
native Drip_SetPlayerAppearance(playerid, const item_ids[DRIP_SLOT_COUNT], muscle, fat);

// Applies one temporary item only to the owning player's client.
native Drip_SetPlayerPreview(playerid, slot, item_id);
native Drip_ClearPlayerPreview(playerid);

// Sends the authoritative state again to one player.
native Drip_ResyncPlayer(playerid);

// Called by the component after the client package passes validation.
forward OnDripClientReady(playerid, protocol_version);

// Called before a rejected client is kicked.
forward OnDripClientRejected(playerid, reason);

#define DRIP_REJECT_NONE              (0)
#define DRIP_REJECT_MISSING_CLIENT    (1)
#define DRIP_REJECT_PROTOCOL_MISMATCH (2)
#define DRIP_REJECT_INVALID_PACKAGE   (3)
#define DRIP_REJECT_MALFORMED_PACKET  (4)
#define DRIP_REJECT_DUPLICATE_CLIENT  (5)
#define DRIP_REJECT_HOOK_CONFLICT     (6)
