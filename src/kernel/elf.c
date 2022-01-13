#include <arch/memory.h>
#include <elf.h>
#include <lib/print.h>

uint64_t elf_load_program(uint64_t elf_base, uintptr_t base, Task *task, Auxval *auxval, char **ld_path)
{
    Elf64Header *elf_header = (Elf64Header *)elf_base;

    Elf64ProgramHeader *prog_header = (Elf64ProgramHeader *)(elf_base + elf_header->program_header_table_file_offset);

    auxval->at_phdr = 0;
    auxval->at_phent = sizeof(Elf64ProgramHeader);
    auxval->at_phnum = elf_header->program_header_table_entry_count;

    for (size_t i = 0; i < elf_header->program_header_table_entry_count; i++)
    {

        if (prog_header->type == ELF_PROGRAM_HEADER_INTERPRET)
        {

            if (!ld_path)
            {
                return 0;
            }

            *ld_path = malloc(prog_header->file_size + 1);

            memcpy(*ld_path, (void *)(elf_base + prog_header->file_offset), prog_header->file_size);

            (*ld_path)[prog_header->file_size] = 0;
        }

        if (prog_header->type == 6)
        {
            auxval->at_phdr = base + prog_header->virtual_address;
        }

        if (prog_header->type == ELF_PROGRAM_HEADER_LOAD)
        {
            void *new_addr = pmm_allocate_zero(ALIGN_UP(prog_header->memory_size, PAGE_SIZE) / 4096);

            for (size_t j = 0; j < ALIGN_UP(prog_header->memory_size, PAGE_SIZE) / 4096; j++)
            {
                vmm_map(task->pagemap, j * PAGE_SIZE + (uint64_t)new_addr, base + j * PAGE_SIZE + prog_header->virtual_address, 0b111);
            }

            memcpy((void *)((uint64_t)new_addr + MMAP_IO_BASE), (void *)(elf_base + prog_header->file_offset), prog_header->file_size);
            memset((void *)((uint64_t)new_addr + MMAP_IO_BASE + prog_header->file_size), 0, prog_header->memory_size - prog_header->file_size);
        }

        prog_header = (Elf64ProgramHeader *)((uint8_t *)prog_header + elf_header->program_header_table_entry_size);
    }

    auxval->at_entry = base + elf_header->entry;

    Elf64SectionHeader *shdr = (Elf64SectionHeader *)(elf_header + elf_header->section_header_table_file_offset);

    for (size_t i = 0; i < elf_header->section_header_table_entry_count; i++)
    {
        Elf64SectionHeader *section = &shdr[i];

        if (section->type == ELF_SECTION_HEADER_NOBITS)
        {
            if (!section->entry_size)
                continue;
            if (section->flags & 0x02)
            {
                void *mem = malloc(section->entry_size);

                memset(mem, 0, section->entry_size);

                section->file_offset = (uintptr_t)mem - (uintptr_t)elf_header;
            }
        }
    }

    return base + elf_header->entry;
}
