#include "parser.h"
#include <stddef.h>

elf_hdr* elf_get_header(const uint8_t* content)
{
    return (elf_hdr*)content;
}

bool elf_is_valid_and_loadable(const uint8_t* content, uint32_t content_len)
{
    elf_hdr* hdr = (elf_hdr*)content;
    if (content_len < sizeof(elf_hdr))
    {
        return false;
    }
    
    if (hdr->e_entry == 0)
    {
        return false;
    }

    if (hdr->e_ident[0] != 0x7f || hdr->e_ident[1] != 'E' || hdr->e_ident[2] != 'L' || hdr->e_ident[3] != 'F')
    {
        return false;
    }

    return true;
}

elf_Phdr* elf_get_program_header(const uint8_t* content, uint32_t content_len)
{
    elf_hdr* header = elf_get_header(content);

    if (header->e_phoff >= content_len)
    {
        return NULL;
    }
    return (elf_Phdr*)(content + header->e_phoff);
}

elf_Shdr* elf_get_indexed_section_header(const uint8_t* content, uint32_t content_len, uint8_t index)
{
    elf_hdr* header = elf_get_header(content);

    if (header == NULL || header->e_shoff + (index * sizeof(elf_Shdr)) >= content_len)
    {
        return NULL;
    }
    return (elf_Shdr*)(content + header->e_shoff + (index * sizeof(elf_Shdr)));
}

uint32_t elf_get_size_in_mem(const uint8_t* content, uint32_t content_len)
{
    elf_hdr* header = elf_get_header(content);
    uint32_t size = 0;
    
    for (uint32_t i = 0; i < header->e_shnum; i++)
    {
        elf_Shdr *section_header = elf_get_indexed_section_header(content, content_len, i);
        if (section_header == NULL)
        {
            return size;
        }
        if (section_header->virtual_address != 0)
        {
            size += section_header->size;
        }
    }

    return size;
}