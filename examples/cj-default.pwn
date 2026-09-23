// Gamemode independente. Gere o catalogo com cj-default.overrides.json.
#include <open.mp>
#include <omp-drip>
#include <omp-drip-catalog>

#if DRIP_CATALOG_COUNT == 0
    #error Gere o catalogo do CJ antes de compilar este exemplo.
#endif

#define COLOR_INFO      0xB8E0FFFF
#define COLOR_ERROR     0xFF8888FF
#define ITEMS_PER_PAGE  (8) // 12 linhas: 8 pecas + padrao + anterior + proxima + voltar.
#define MAX_ITEM_PAGES (DRIP_CATALOG_COUNT / ITEMS_PER_PAGE + 1)
#define SHOP_INTERIOR   (14)
#define SPAWN_X         (204.16)
#define SPAWN_Y         (-165.76)
#define SPAWN_Z         (1000.52)
#define FITTING_X       (215.2920)
#define FITTING_Y       (-156.1625)
#define FITTING_Z       (1000.5234)
#define FITTING_ANGLE   (90.1563)
#define ROW_DEFAULT     (-1)
#define ROW_PREVIOUS    (-2)
#define ROW_NEXT        (-3)
#define ROW_BACK        (-4)

// Categorias de roupas e cabelos presentes no catalogo original.
static const gSlots[] = {0, 1, 2, 3, 13, 14, 15, 16, 17};
static const gSlotNames[][] =
{
    "Torso", "Cabelo", "Pernas", "Calcados", "Colares",
    "Relogios", "Oculos", "Chapeus", "Trajes especiais"
};

static gAppearance[MAX_PLAYERS][DRIP_SLOT_COUNT];
static bool:gSpawned[MAX_PLAYERS];
static gCategory[MAX_PLAYERS];
static gPage[MAX_PLAYERS];
// Menus compartilhados; cada jogador guarda apenas a categoria/pagina atual.
static Menu:gSlotsMenu = INVALID_MENU;
static Menu:gPreviewMenu = INVALID_MENU;
static bool:gMenusReady;
static Menu:gItemMenus[sizeof gSlots][MAX_ITEM_PAGES];
static gPageCount[sizeof gSlots];
static gRows[MAX_MENUS][12];
static gRowCount[MAX_MENUS];
static Menu:gActiveMenu[MAX_PLAYERS];
static bool:gFitting[MAX_PLAYERS];
static Float:gReturnPos[MAX_PLAYERS][4];
static gReturnInterior[MAX_PLAYERS];
static gPreviewSlot[MAX_PLAYERS];
static gPreviewItem[MAX_PLAYERS];

main() {}

public OnGameModeInit()
{
    SetGameModeText("omp-drip: CJ default");
    SetWorldTime(12);
    SetWeather(2);
    AddPlayerClass(0, SPAWN_X, SPAWN_Y, SPAWN_Z, FITTING_ANGLE, WEAPON_FIST, 0, WEAPON_FIST, 0, WEAPON_FIST, 0);
    gMenusReady = bool:CreateClothingMenus();
    if (!gMenusReady)
    {
        print("[CJ demo] Nao foi possivel criar os menus de roupas.");
        return 0;
    }
    printf("[CJ demo] Catalogo %s: %d itens", DRIP_CATALOG_VERSION, DRIP_CATALOG_COUNT);
    return 1;
}

public OnGameModeExit()
{
    for (new playerid = 0; playerid < MAX_PLAYERS; playerid++)
    {
        if (!IsPlayerConnected(playerid) || !gFitting[playerid]) continue;
        CancelPreview(playerid);
        EndFitting(playerid);
    }
    if (gSlotsMenu != INVALID_MENU) DestroyMenu(gSlotsMenu);
    if (gPreviewMenu != INVALID_MENU) DestroyMenu(gPreviewMenu);
    for (new category = 0; category < sizeof gSlots; category++)
        for (new page = 0; page < gPageCount[category]; page++)
            DestroyMenu(gItemMenus[category][page]);
    return 1;
}

public OnPlayerConnect(playerid)
{
    gSpawned[playerid] = false;
    gPreviewSlot[playerid] = -1;
    gPreviewItem[playerid] = 0;
    gCategory[playerid] = 0;
    gPage[playerid] = 0;
    gActiveMenu[playerid] = INVALID_MENU;
    gFitting[playerid] = false;
    ResetOutfit(playerid);
    SendClientMessage(playerid, COLOR_INFO, "CJ demo: /roupas, /confirmar, /cancelar, /cj e /resync.");
    return 1;
}

public OnPlayerRequestClass(playerid, classid)
{
    SetPlayerInterior(playerid, SHOP_INTERIOR);
    SetPlayerPos(playerid, SPAWN_X, SPAWN_Y, SPAWN_Z);
    SetPlayerFacingAngle(playerid, FITTING_ANGLE);
    SetFrontCamera(playerid, SPAWN_X, SPAWN_Y, SPAWN_Z, FITTING_ANGLE);
    return 1;
}

public OnPlayerSpawn(playerid)
{
    gSpawned[playerid] = true;
    SetPlayerSkin(playerid, 0);
    EndFitting(playerid, false);
    SetPlayerInterior(playerid, SHOP_INTERIOR);
    SetPlayerPos(playerid, SPAWN_X, SPAWN_Y, SPAWN_Z);
    SetPlayerFacingAngle(playerid, FITTING_ANGLE);
    SetCameraBehindPlayer(playerid);
    CancelPreview(playerid);
    // Tambem cobre desenvolvimento sem manifesto, quando o ready pode nao ocorrer.
    if (Drip_IsClientReady(playerid)) ApplyOutfit(playerid);
    return 1;
}

public OnPlayerDeath(playerid, killerid, WEAPON:reason)
{
    gSpawned[playerid] = false;
    CancelPreview(playerid);
    EndFitting(playerid, false);
    return 1;
}

public OnPlayerDisconnect(playerid, reason)
{
    gSpawned[playerid] = false;
    gPreviewSlot[playerid] = -1;
    gActiveMenu[playerid] = INVALID_MENU;
    gFitting[playerid] = false;
    return 1;
}

public OnDripClientReady(playerid, protocol_version)
{
    printf("[CJ demo] Jogador %d pronto, protocolo %d", playerid, protocol_version);
    // O handshake pode terminar antes ou depois do spawn.
    if (gSpawned[playerid]) ApplyOutfit(playerid);
    return 1;
}

public OnDripClientRejected(playerid, reason)
{
    printf("[CJ demo] Pacote rejeitado: jogador %d, motivo %d", playerid, reason);
    return 1; // O componente informa o motivo e desconecta o cliente.
}

public OnPlayerCommandText(playerid, cmdtext[])
{
    if (strcmp(cmdtext, "/roupas", true) && strcmp(cmdtext, "/confirmar", true)
        && strcmp(cmdtext, "/cancelar", true) && strcmp(cmdtext, "/cj", true)
        && strcmp(cmdtext, "/resync", true)) return 0;
    if (!CanChangeClothes(playerid)) return 1;

    if (!strcmp(cmdtext, "/roupas", true))
    {
        CancelPreview(playerid);
        BeginFitting(playerid);
        ShowSlots(playerid);
    }
    else if (!strcmp(cmdtext, "/confirmar", true))
    {
        ConfirmPreview(playerid);
    }
    else if (!strcmp(cmdtext, "/cancelar", true))
    {
        CancelPreview(playerid);
        EndFitting(playerid);
        SendClientMessage(playerid, COLOR_INFO, "Previa cancelada. Seu visual confirmado foi restaurado.");
    }
    else if (!strcmp(cmdtext, "/cj", true))
    {
        CancelPreview(playerid);
        EndFitting(playerid);
        SetPlayerSkin(playerid, 0);
        ResetOutfit(playerid);
        ApplyOutfit(playerid);
        SendClientMessage(playerid, COLOR_INFO, "Visual inicial do CJ restaurado.");
    }
    else
    {
        CancelPreview(playerid);
        EndFitting(playerid);
        Drip_ResyncPlayer(playerid);
        SendClientMessage(playerid, COLOR_INFO, "Aparencias em streaming reenviadas para seu cliente.");
    }
    return 1;
}

public OnPlayerSelectedMenuRow(playerid, row)
{
    new Menu:menu = GetPlayerMenu(playerid);
    if (!gFitting[playerid] || menu == INVALID_MENU || menu != gActiveMenu[playerid]) return 1;
    if (!CanChangeClothes(playerid))
    {
        CancelPreview(playerid);
        EndFitting(playerid);
        return 1;
    }

    if (menu == gSlotsMenu)
    {
        if (row == sizeof gSlots)
        {
            CancelPreview(playerid);
            EndFitting(playerid);
            return 1;
        }
        if (row < 0 || row >= sizeof gSlots) return 1;
        gCategory[playerid] = row;
        gPage[playerid] = 0;
        return ShowItems(playerid);
    }
    if (menu == gPreviewMenu)
    {
        switch (row)
        {
            case 0: ConfirmPreview(playerid);
            case 1:
            {
                CancelPreview(playerid);
                ShowItems(playerid);
            }
            case 2:
            {
                CancelPreview(playerid);
                EndFitting(playerid);
            }
        }
        return 1;
    }
    if (menu != gItemMenus[gCategory[playerid]][gPage[playerid]]) return 1;
    if (row < 0 || row >= gRowCount[menu]) return 1;

    new index = gRows[menu][row];
    if (index == ROW_BACK) return ShowSlots(playerid);
    if (index == ROW_PREVIOUS)
    {
        if (gPage[playerid] > 0) gPage[playerid]--;
        return ShowItems(playerid);
    }
    if (index == ROW_NEXT)
    {
        if (gPage[playerid] + 1 < gPageCount[gCategory[playerid]]) gPage[playerid]++;
        return ShowItems(playerid);
    }

    new slot = gSlots[gCategory[playerid]], item;
    if (index == ROW_DEFAULT) item = DripDefaultItem[slot];
    else
    {
        if (index < 0 || index >= DRIP_CATALOG_COUNT) return 1;
        if (!DripCatalog[index][DripItemEnabled] || DripCatalog[index][DripItemSlot] != slot) return 1;
        item = DripCatalog[index][DripItemID];
    }
    if (!Drip_SetPlayerPreview(playerid, slot, item))
    {
        SendClientMessage(playerid, COLOR_ERROR, "Nao foi possivel abrir a previa.");
        return ShowItems(playerid);
    }

    gPreviewSlot[playerid] = slot;
    gPreviewItem[playerid] = item;
    SendClientMessage(playerid, COLOR_INFO, "Previa so para voce. Confirme a roupa ou escolha outra peca no menu.");
    return ShowClothingMenu(playerid, gPreviewMenu);
}

public OnPlayerExitedMenu(playerid)
{
    if (!gFitting[playerid]) return 1;
    CancelPreview(playerid);
    EndFitting(playerid);
    return 1;
}

stock CanChangeClothes(playerid)
{
    if (!gMenusReady)
    {
        SendClientMessage(playerid, COLOR_ERROR, "Os menus de roupas nao estao disponiveis.");
        return 0;
    }
    if (!gSpawned[playerid] || GetPlayerSkin(playerid) != 0)
    {
        SendClientMessage(playerid, COLOR_ERROR, "Entre no jogo com a skin 0 (CJ) para testar as roupas.");
        return 0;
    }
    if (!Drip_IsClientReady(playerid))
    {
        SendClientMessage(playerid, COLOR_ERROR, "Aguarde a validacao do pacote omp-drip.");
        return 0;
    }
    return 1;
}

stock ResetOutfit(playerid)
{
    for (new slot = 0; slot < DRIP_SLOT_COUNT; slot++)
        gAppearance[playerid][slot] = DripDefaultItem[slot];
}

stock ApplyOutfit(playerid)
{
    // IDs estaveis, um por slot; musculo e gordura entre 0 e 1000.
    // Persista este array no seu gamemode para manter o visual apos reconectar.
    return Drip_SetPlayerAppearance(playerid, gAppearance[playerid], 0, 0);
}

stock CancelPreview(playerid)
{
    gPreviewSlot[playerid] = -1;
    if (Drip_IsClientReady(playerid)) Drip_ClearPlayerPreview(playerid);
}

stock ShowSlots(playerid)
{
    return ShowClothingMenu(playerid, gSlotsMenu);
}

stock ShowItems(playerid)
{
    return ShowClothingMenu(playerid, gItemMenus[gCategory[playerid]][gPage[playerid]]);
}

stock ShowClothingMenu(playerid, Menu:menu)
{
    if (gActiveMenu[playerid] != INVALID_MENU) HideMenuForPlayer(gActiveMenu[playerid], playerid);
    gActiveMenu[playerid] = menu;
    return ShowMenuForPlayer(menu, playerid);
}

stock AddClothingRow(Menu:menu, const label[], action)
{
    new row = AddMenuItem(menu, 0, label);
    gRows[menu][row] = action;
    gRowCount[menu]++;
}

stock CreateClothingMenus()
{
    gSlotsMenu = CreateMenu("Didier Sachs", 1, 20.0, 130.0, 190.0);
    gPreviewMenu = CreateMenu("Experimentar roupa", 1, 20.0, 130.0, 190.0);
    if (gSlotsMenu == INVALID_MENU || gPreviewMenu == INVALID_MENU) return 0;
    for (new category = 0; category < sizeof gSlots; category++)
        AddMenuItem(gSlotsMenu, 0, gSlotNames[category]);
    AddMenuItem(gSlotsMenu, 0, "Sair do provador");
    AddMenuItem(gPreviewMenu, 0, "Confirmar roupa");
    AddMenuItem(gPreviewMenu, 0, "Experimentar outra");
    AddMenuItem(gPreviewMenu, 0, "Cancelar e sair");

    for (new category = 0; category < sizeof gSlots; category++)
    {
        new count, slot = gSlots[category];
        for (new index = 0; index < DRIP_CATALOG_COUNT; index++)
            if (DripCatalog[index][DripItemEnabled] && DripCatalog[index][DripItemSlot] == slot) count++;
        new pages = (count + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
        if (pages == 0) pages = 1;
        for (new page = 0; page < pages; page++)
        {
            new title[32];
            format(title, sizeof title, "%s %d/%d", gSlotNames[category], page + 1, pages);
            new Menu:menu = CreateMenu(title, 1, 20.0, 130.0, 190.0);
            if (menu == INVALID_MENU) return 0;
            gItemMenus[category][page] = menu;
            gPageCount[category]++;
            AddClothingRow(menu, "Restaurar padrao / remover", ROW_DEFAULT);

            new seen, shown, skip = page * ITEMS_PER_PAGE;
            for (new index = 0; index < DRIP_CATALOG_COUNT; index++)
            {
                if (!DripCatalog[index][DripItemEnabled] || DripCatalog[index][DripItemSlot] != slot) continue;
                if (seen++ < skip) continue;
                if (shown == ITEMS_PER_PAGE) break;
                // Texturas identificam cada peca e cabem no limite de 31 caracteres.
                AddClothingRow(menu, DripCatalog[index][DripItemTexture], index);
                shown++;
            }
            if (page > 0) AddClothingRow(menu, "<< Pagina anterior", ROW_PREVIOUS);
            if (page + 1 < pages) AddClothingRow(menu, "Proxima pagina >>", ROW_NEXT);
            AddClothingRow(menu, "Voltar as categorias", ROW_BACK);
        }
    }
    return 1;
}

stock SetFrontCamera(playerid, Float:x, Float:y, Float:z, Float:angle)
{
    // Em GTA, 90 graus olha para -X: a camera fica 3.2 metros a frente.
    SetPlayerCameraPos(playerid, x - 3.2 * floatsin(angle, degrees),
        y + 3.2 * floatcos(angle, degrees), z + 0.65);
    SetPlayerCameraLookAt(playerid, x, y, z + 0.15, CAMERA_CUT);
}

stock BeginFitting(playerid)
{
    if (!gFitting[playerid])
    {
        GetPlayerPos(playerid, gReturnPos[playerid][0], gReturnPos[playerid][1], gReturnPos[playerid][2]);
        GetPlayerFacingAngle(playerid, gReturnPos[playerid][3]);
        gReturnInterior[playerid] = GetPlayerInterior(playerid);
        gFitting[playerid] = true;
    }
    SetPlayerInterior(playerid, SHOP_INTERIOR);
    SetPlayerPos(playerid, FITTING_X, FITTING_Y, FITTING_Z);
    SetPlayerFacingAngle(playerid, FITTING_ANGLE);
    TogglePlayerControllable(playerid, false);
    SetFrontCamera(playerid, FITTING_X, FITTING_Y, FITTING_Z, FITTING_ANGLE);
}

stock EndFitting(playerid, bool:restorePosition = true)
{
    if (gActiveMenu[playerid] != INVALID_MENU)
        HideMenuForPlayer(gActiveMenu[playerid], playerid);
    gActiveMenu[playerid] = INVALID_MENU;
    if (!gFitting[playerid]) return;
    gFitting[playerid] = false;
    if (restorePosition)
    {
        SetPlayerInterior(playerid, gReturnInterior[playerid]);
        SetPlayerPos(playerid, gReturnPos[playerid][0], gReturnPos[playerid][1], gReturnPos[playerid][2]);
        SetPlayerFacingAngle(playerid, gReturnPos[playerid][3]);
    }
    TogglePlayerControllable(playerid, true);
    SetCameraBehindPlayer(playerid);
}

stock ConfirmPreview(playerid)
{
    new slot = gPreviewSlot[playerid];
    if (slot == -1)
        return SendClientMessage(playerid, COLOR_ERROR, "Abra /roupas e escolha uma peca primeiro.");
    new previous = gAppearance[playerid][slot];
    gAppearance[playerid][slot] = gPreviewItem[playerid];
    // Cancelar tambem ao confirmar uma peca ja equipada (sem StateUpdate).
    CancelPreview(playerid);
    if (!ApplyOutfit(playerid))
    {
        gAppearance[playerid][slot] = previous;
        SendClientMessage(playerid, COLOR_ERROR, "Nao foi possivel aplicar a roupa.");
    }
    else SendClientMessage(playerid, COLOR_INFO, "Roupa confirmada e sincronizada com os outros jogadores.");
    if (gFitting[playerid]) ShowItems(playerid);
    return 1;
}
