// Compile-only API check. No GTA assets or generated catalog are required.
#include <open.mp>
#include <omp-drip>
#include <omp-drip>
#include <omp-drip-catalog>

main() {}

public OnGameModeInit()
{
    SetGameModeText("omp-drip API check");
    printf("Catalog %s: first row ID %x", DRIP_CATALOG_VERSION, DripCatalog[0][DripItemID]);
    return 1;
}

public OnDripClientReady(playerid, protocol_version)
{
    printf("Player %d: omp-drip protocol %d", playerid, protocol_version);
    if (!Drip_IsClientReady(playerid)) return 0;
    Drip_SetPlayerAppearance(playerid, DripDefaultItem, 0, 0);
    Drip_SetPlayerPreview(playerid, 0, DripDefaultItem[0]);
    Drip_ClearPlayerPreview(playerid);
    return Drip_ResyncPlayer(playerid);
}

public OnDripClientRejected(playerid, reason)
{
    printf("Player %d rejected: %d (missing client: %d)",
        playerid, reason, DRIP_REJECT_MISSING_CLIENT);
    return 1;
}
