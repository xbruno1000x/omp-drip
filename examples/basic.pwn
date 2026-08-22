#include <open.mp>
#include <omp-drip>
#include <omp-drip-catalog>

static gAppearance[MAX_PLAYERS][DRIP_SLOT_COUNT];

main() {}

public OnPlayerConnect(playerid)
{
    for (new slot = 0; slot < DRIP_SLOT_COUNT; slot++)
    {
        gAppearance[playerid][slot] = DripDefaultItem[slot];
    }
    return 1;
}

public OnDripClientReady(playerid, protocol_version)
{
    printf("omp-drip protocol %d ready for player %d", protocol_version, playerid);
    Drip_SetPlayerAppearance(playerid, gAppearance[playerid], 0, 0);
    return 1;
}

public OnDripClientRejected(playerid, reason)
{
    printf("omp-drip rejected player %d (reason %d)", playerid, reason);
    return 1;
}

stock EquipCatalogItem(playerid, catalog_index)
{
    if (catalog_index < 0 || catalog_index >= DRIP_CATALOG_COUNT) return 0;
    if (!DripCatalog[catalog_index][DripItemEnabled]) return 0;

    new slot = DripCatalog[catalog_index][DripItemSlot];
    gAppearance[playerid][slot] = DripCatalog[catalog_index][DripItemID];

    // Persist gAppearance in your own database before or after this call.
    return Drip_SetPlayerAppearance(playerid, gAppearance[playerid], 0, 0);
}

stock PreviewCatalogItem(playerid, catalog_index)
{
    if (catalog_index < 0 || catalog_index >= DRIP_CATALOG_COUNT) return 0;
    return Drip_SetPlayerPreview(
        playerid,
        DripCatalog[catalog_index][DripItemSlot],
        DripCatalog[catalog_index][DripItemID]
    );
}
