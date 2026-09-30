import re

with open('src/custom_team_builder.c', 'r') as f:
    text = f.read()

old_func = '''static void CTB_ScrolledLine(u8 winId, u8 fontId, const u8 *str, u8 lineIdx, bool8 selected)
{
    u8 y = lineIdx * LIST_LINE_H;
    if (selected)
    {
        // Draw a highlight bar
        FillWindowPixelRect(winId, PIXEL_FILL(2), 0, y, 0x88, LIST_LINE_H);
        AddTextPrinterParameterized(winId, fontId, str, 4, y + 2, TEXT_SKIP_DRAW, NULL);
    }
    else
    {
        AddTextPrinterParameterized(winId, fontId, str, 4, y + 2, TEXT_SKIP_DRAW, NULL);
    }
}'''

new_func = '''static void CTB_ScrolledLine(u8 winId, u8 fontId, const u8 *str, u8 lineIdx, bool8 selected)
{
    u8 color[3];
    u8 y = lineIdx * LIST_LINE_H;
    
    if (selected)
    {
        color[0] = TEXT_COLOR_TRANSPARENT;
        color[1] = TEXT_COLOR_RED;
        color[2] = TEXT_COLOR_LIGHT_RED;
    }
    else
    {
        color[0] = TEXT_COLOR_TRANSPARENT;
        color[1] = TEXT_COLOR_DARK_GRAY;
        color[2] = TEXT_COLOR_LIGHT_GRAY;
    }
    AddTextPrinterParameterized4(winId, fontId, 4, y + 2, 0, 0, color, TEXT_SKIP_DRAW, str);
}'''

text = text.replace(old_func, new_func)

with open('src/custom_team_builder.c', 'w') as f:
    f.write(text)
