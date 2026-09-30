with open('src/rental_teams.c', 'r') as f:
    text = f.read()

sprite_code = '''
#define TAG_RENTAL_ARROW_CURSOR 2002

static const u16 sRedInterface_Pal[]    = INCGFX_U16("graphics/interface/red.pal", ".gbapal");
static const u32 sArrowCursor_Gfx[]     = INCGFX_U32("graphics/interface/arrow_cursor.png", ".4bpp.smol");

static const struct SpritePalette sArrowCursorSpritePal = { sRedInterface_Pal, TAG_RENTAL_ARROW_CURSOR };
static const struct CompressedSpriteSheet sArrowCursorSpriteSheet = { (const u8 *)sArrowCursor_Gfx, 0x80, TAG_RENTAL_ARROW_CURSOR };

static const struct OamData sOamData_ArrowCursor =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(8x8),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(8x8),
    .tileNum = 0,
    .priority = 0,
    .paletteNum = 0,
    .affineParam = 0,
};

static const struct SpriteTemplate sSpriteTemplate_ArrowCursor =
{
    .tileTag = TAG_RENTAL_ARROW_CURSOR,
    .paletteTag = TAG_RENTAL_ARROW_CURSOR,
    .oam = &sOamData_ArrowCursor,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};
'''

text = text.replace('static const u8 sText_Cancel[]', sprite_code + '\nstatic const u8 sText_Cancel[]')
with open('src/rental_teams.c', 'w') as f:
    f.write(text)
