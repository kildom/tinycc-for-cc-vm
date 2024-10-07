
#include "ccvm-reloc.h"


static void g8(int c)
{
    int ind1;
    if (nocode_wanted)
        return;
    ind1 = ind + 1;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    cur_text_section->data[ind] = c;
    ind = ind1;
}

static void g16(int c)
{
    int ind1;
    if (nocode_wanted)
        return;
    ind1 = ind + 2;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    cur_text_section->data[ind++] = c;
    cur_text_section->data[ind++] = c >> 8;
}

static void g24(int c)
{
    int ind1;
    if (nocode_wanted)
        return;
    ind1 = ind + 3;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    cur_text_section->data[ind++] = c;
    cur_text_section->data[ind++] = c >> 8;
    cur_text_section->data[ind++] = c >> 16;
}

static void g32(int c)
{
    int ind1;
    if (nocode_wanted)
        return;
    ind1 = ind + 4;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    cur_text_section->data[ind++] = c;
    cur_text_section->data[ind++] = c >> 8;
    cur_text_section->data[ind++] = c >> 16;
    cur_text_section->data[ind++] = c >> 24;
}
