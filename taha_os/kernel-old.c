
#include <stdint.h>

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t attributes;
} __attribute__((packed)) memory_map_entry_t;

// ==========================================
// VGA CONFIGURATION
// ==========================================

#define VGA_WIDTH       80
#define VGA_HEIGHT      25
#define VGA_MEMORY      ((volatile uint16_t*)0xB8000)

// ==========================================
// VGA COLORS
// ==========================================

#define BLACK           0
#define BLUE            1
#define GREEN           2
#define CYAN            3
#define RED             4
#define MAGENTA         5
#define BROWN           6
#define LIGHT_GREY      7
#define DARK_GREY       8
#define LIGHT_BLUE      9
#define LIGHT_GREEN     10
#define LIGHT_CYAN      11
#define LIGHT_RED       12
#define LIGHT_MAGENTA   13
#define YELLOW          14
#define WHITE           15

// ==========================================
// TERMINAL VARIABLES
// ==========================================

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;

static uint8_t terminal_color = 0x0F;

// ==========================================
// VGA ENTRY
// ==========================================

static uint16_t vga_entry(char c, uint8_t color)
{
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

// ==========================================
// SET COLOR
// ==========================================

void set_color(uint8_t foreground, uint8_t background)
{
    terminal_color = (uint8_t)((foreground & 0x0F) | ((background & 0x0F) << 4));
}

// ==========================================
// CLEAR SCREEN
// ==========================================

void clear_screen(void)
{
    uint16_t blank = vga_entry(' ', terminal_color);
    for (uint32_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
    {
        VGA_MEMORY[i] = blank;
    }
    cursor_x = 0;
    cursor_y = 0;
}

// ==========================================
// SCROLL SCREEN
// ==========================================

static void scroll_screen(void)
{
    for (uint32_t y = 1; y < VGA_HEIGHT; y++)
    {
        for (uint32_t x = 0; x < VGA_WIDTH; x++)
        {
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }

    uint16_t blank = vga_entry(' ', terminal_color);

    for (uint32_t x = 0; x < VGA_WIDTH; x++)
    {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    }

    cursor_y = VGA_HEIGHT - 1;
}

// ==========================================
// PUTCHAR
// ==========================================

void putchar(char c)
{
    if (c == '\n')
    {
        cursor_x = 0;
        cursor_y++;
    }
    else if (c == '\r')
    {
        cursor_x = 0;
    }
    else if (c == '\b')
    {
        if (cursor_x > 0)
        {
            cursor_x--;

            VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', terminal_color);
        }
    }
    else if (c == '\t')
    {
        cursor_x = (cursor_x + 4) & ~3U;
    }
    else
    {
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(c, terminal_color);

        cursor_x++;
    }

    // Handle line wrapping
    if (cursor_x >= VGA_WIDTH)
    {
        cursor_x = 0;
        cursor_y++;
    }

    // Handle scrolling
    while (cursor_y >= VGA_HEIGHT)
    {
        scroll_screen();
    }
}

// ==========================================
// PRINT STRING
// ==========================================

void print(const char* str)
{
    if (str == 0)
        return;

    while (*str != '\0')
    {
        putchar(*str);
        str++;
    }
}

// ==========================================
// INITIALIZE SCREEN
// ==========================================

void initialize_screen(void)
{
    cursor_x = 0;
    cursor_y = 0;

    terminal_color = 0x0F;

    clear_screen();
}

// ==========================================
// TEST COLORS
// ==========================================


void test_colors(void)
{
    for (uint8_t bg = 0; bg < 16; bg++)
    {
        for (uint8_t fg = 0; fg < 16; fg++)
        {
            set_color(fg, bg);

            print(" FG ");
        }

        print("\n");
    }

    set_color(WHITE, BLACK);
}

// ==========================================
// PRINT HEXADECIMAL (64-BIT)
// ==========================================

void print_hex64(uint64_t value)
{
    const char hex[] = "0123456789ABCDEF";

    print("0x");

    for (int i = 60; i >= 0; i -= 4)
    {
        putchar(hex[(value >> i) & 0xF]);
    }
}

// ==========================================
// PRINT HEXADECIMAL (32-BIT)
// ==========================================

void print_hex32(uint32_t value)
{
    print_hex64((uint64_t)value);
}

// ==========================================
// PRINT DECIMAL (64-BIT)
// ==========================================

void print_dec64(uint64_t value)
{
    char buffer[21];
    int i = 0;

    if (value == 0)
    {
        putchar('0');
        return;
    }

    while (value > 0)
    {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0)
    {
        putchar(buffer[--i]);
    }
}

// ==========================================
// MEMORY TYPE NAME
// ==========================================

const char* get_memory_type(uint32_t type)
{
    switch (type)
    {
        case 1:
            return "USABLE";

        case 2:
            return "RESERVED";

        case 3:
            return "ACPI RECLAIMABLE";

        case 4:
            return "ACPI NVS";

        case 5:
            return "BAD MEMORY";

        default:
            return "UNKNOWN";
    }
}

// ==========================================
// KERNEL ENTRY
// ==========================================

void kernel_main(memory_map_entry_t* memory_map, uint64_t entry_count)
{
    // initialize_screen();

    // for (uint64_t i = 0; i < entry_count; i++)
    // {
    //     memory_map_entry_t* entry = &memory_map[i];

    //     uint64_t base = entry->base;
    //     uint64_t length = entry->length;
    //     uint32_t type = entry->type;

    //     // Process memory regions
    // }

    initialize_screen();

    set_color(LIGHT_GREEN, BLACK);

    print("KERNEL MAIN REACHED!");

    set_color(LIGHT_CYAN, BLACK);

    print("========================================");
    print("\n");
    print("          BIOS MEMORY MAP");
    print("\n");
    print("========================================");
    print("\n\n");

    set_color(WHITE, BLACK);

    print("Total Entries: ");
    print_dec64(entry_count);

    print("\n\n");

    for (uint64_t i = 0; i < entry_count; i++)
    {
        memory_map_entry_t* entry = &memory_map[i];

        set_color(YELLOW, BLACK);

        print("ENTRY ");
        print_dec64(i);
        print("\n");

        set_color(WHITE, BLACK);

        print("  Base Address : ");
        print_hex64(entry->base);

        print("\n");

        print("  Length       : ");
        print_hex64(entry->length);

        print("\n");

        print("  Type         : ");
        print_dec64(entry->type);

        print(" (");

        print(get_memory_type(entry->type));

        print(")");

        print("\n");

        print("  Attributes   : ");
        print_hex32(entry->attributes);

        print("\n\n");
    }

    set_color(LIGHT_GREEN, BLACK);

    print("Memory map displayed successfully!");

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}
