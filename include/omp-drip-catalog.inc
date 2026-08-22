#if defined _omp_drip_catalog_included
    #endinput
#endif
#define _omp_drip_catalog_included

// Placeholder catalog so projects can resolve the include before generating
// their own data. Replace this file with build/catalog/generated/omp-drip-catalog.inc.
#define DRIP_CATALOG_VERSION "unconfigured"
#define DRIP_CATALOG_COUNT (0)
#define DRIP_ITEM_NAME_MAX (48)
#define DRIP_ITEM_CATEGORY_MAX (24)
#define DRIP_ASSET_NAME_MAX (32)

enum E_DRIP_CATALOG_ITEM
{
    DripItemID,
    DripItemSlot,
    DripItemPrice,
    bool:DripItemEnabled,
    DripItemName[DRIP_ITEM_NAME_MAX],
    DripItemCategory[DRIP_ITEM_CATEGORY_MAX],
    DripItemTexture[DRIP_ASSET_NAME_MAX],
    DripItemModel[DRIP_ASSET_NAME_MAX]
};

new const DripCatalog[1][E_DRIP_CATALOG_ITEM];
new const DripDefaultItem[DRIP_SLOT_COUNT];
