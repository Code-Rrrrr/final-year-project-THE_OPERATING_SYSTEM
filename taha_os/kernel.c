
#include <stdint.h>


// **********************************************
// GLOBAL VARIABLES
// **********************************************

static int left_shift_pressed = 0;
static int right_shift_pressed = 0;

// **********************************************
// STRUCTURE DECLARATIONS
// **********************************************

struct interrupt_frame{
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

// **********************************************
// FUNCTION PROTOTYPES
// **********************************************

static uint16_t character(char c, uint8_t color);
void set_color(uint8_t foreground, uint8_t background);
void clear_screen(void);
void scroll_screen(void);
void put_char(char c);
void print(const char *str);
void initialize_screen(void);
void test_colors(void);
void print_hex64(uint64_t value);
void print_hex32(uint32_t value);
void print_dec64(uint64_t value);

static inline void outb(uint16_t port, uint8_t value);
static inline uint8_t inb(uint16_t port);

uint8_t cmos_read(uint8_t reg);
uint8_t bcd_to_bin(uint8_t value);
void get_rtc_time(void);

void delay(void);

void idt_set_gate(int vector, uint64_t handler);
__attribute__((interrupt)) void exception_handler(struct interrupt_frame *frame);
__attribute__((interrupt)) void exception_error_handler(struct interrupt_frame *frame, uint64_t error_code);
void idt_init(void);
void test_divide_by_zero(void);

static inline void io_wait(void);
void pic_remap(void);
void pic_send_eoi(uint8_t irq);
void keyboard_enable_irq(void);
__attribute__((interrupt)) void keyboard_handler(struct interrupt_frame *frame);
int keyboard_get_scancode(uint8_t *scancode);
int ps2_wait_input(void);
int ps2_wait_output(void);
void keyboard_init(void);
char scancode_to_ascii(uint8_t scancode);

void input(char *buffer, uint64_t max_length);

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, uint64_t n);

void about(void);

void wait_rtc_update(void);
void get_date(void);
void get_time(void);

void calculate(char *command);

void splash_screen(void);

// **********************************************
// MEMORY MAP
// **********************************************

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t attributes;
} __attribute__((packed)) memory_map_entry;

typedef struct {
    memory_map_entry *memory_map;
    uint8_t entry_count;
} __attribute__((packed)) boot_info;

// **********************************************
// VGA
// **********************************************

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t*)0xb8000)

#define BLACK 0
#define BLUE 1
#define GREEN 2
#define CYAN 3
#define RED 4
#define MAGENTA 5
#define BROWN 6
#define LIGHT_GREY 7
#define DARK_GREY 8
#define LIGHT_BLUE 9
#define LIGHT_GREEN 10
#define LIGHT_CYAN 11
#define LIGHT_RED 12
#define LIGHT_MAGENTA 13
#define YELLOW 14
#define WHITE 15

static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;
static uint8_t terminal_color = 0x0f;

static uint16_t character(char c, uint8_t color){
    return (uint16_t)((uint8_t)c | ((uint16_t)color << 8));
}

void set_color(uint8_t foreground, uint8_t background){
    terminal_color = (uint8_t)((foreground & 0x0f) | ((background & 0x0f) << 4));
}

void clear_screen(void){
    uint16_t blank = character(' ', terminal_color);
    for(uint16_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++){
        VGA_MEMORY[i] = blank;
    }
    cursor_x = 0;
    cursor_y = 0;
}

void scroll_screen(void){
    // Move all lines up by one row
    for(uint16_t y = 1; y < VGA_HEIGHT; y++){
        for(uint16_t x = 0; x < VGA_WIDTH; x++){
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }
    // Clear the last row
    uint16_t blank = character(' ', terminal_color);
    for(uint16_t x = 0; x < VGA_WIDTH; x++){
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    }
    cursor_y = VGA_HEIGHT - 1;
}

void put_char(char c){
    if(c == '\b'){
        if(cursor_x > 0){
            cursor_x--;
        }
        else if(cursor_y > 0){
            cursor_y--;
            cursor_x = VGA_WIDTH - 1;
        }
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = character(' ', terminal_color);
    }
    else if(c == '\n'){
        cursor_x = 0;
        cursor_y++;
    }
    else{
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = character(c, terminal_color);
        cursor_x++;
    }
    // Handle line wrapping
    if (cursor_x >= VGA_WIDTH){
        cursor_x = 0;
        cursor_y++;
    }
    // Handle scrolling
    while (cursor_y >= VGA_HEIGHT){
        scroll_screen();
    }
}

void print(const char *str){
    if(str == 0){
        return;
    }
    while(*str != '\0'){
        put_char(*str);
        str++;
    }
}

void initialize_screen(void){
    cursor_x = 0;
    cursor_y = 0;
    terminal_color = 0x0f;
    clear_screen();
}

void test_colors(void){
    for (uint8_t bg = 0; bg < 16; bg++){
        for (uint8_t fg = 0; fg < 16; fg++){
            set_color(fg, bg);
            print("+");
        }
        print("\n");
    }
    set_color(WHITE, BLACK);
}

void print_hex64(uint64_t value){
    const char hex[] = "0123456789ABCDEF";
    print("0x");
    for (int i = 60; i >= 0; i -= 4){
        put_char(hex[(value >> i) & 0xF]);
    }
}

void print_hex32(uint32_t value){
    print_hex64((uint64_t)value);
}

void print_dec64(uint64_t value){
    char buffer[21];
    int i = 0;
    if (value == 0){
        put_char('0');
        return;
    }
    while (value > 0){
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }
    while (i > 0){
        put_char(buffer[--i]);
    }
}

// **********************************************
// CALCULATOR
// **********************************************

// calc NUMBER OPERATOR NUMBER
void calculate(char *command){
    uint32_t num1 = 0;
    uint32_t num2 = 0;
    char op = 0;

    uint8_t i = 5; // Skip "calc "

    // Read first number
    while (command[i] >= '0' && command[i] <= '9') {
        num1 = num1 * 10 + (command[i] - '0');
        i++;
    }

    // Read operator
    op = command[i];
    i++;

    // Read second number
    while (command[i] >= '0' && command[i] <= '9') {
        num2 = num2 * 10 + (command[i] - '0');
        i++;
    }

    int result = 0;

    switch (op) {
        case '+':
            result = num1 + num2;
            break;

        case '-':
            result = num1 - num2;
            break;

        case '*':
            result = num1 * num2;
            break;

        case '/':
            if (num2 == 0) {
                set_color(LIGHT_RED,BLACK);
                print("Error: ");
                set_color(LIGHT_GREY,BLACK);
                print("Division by zero\n");
                return;
            }
            result = num1 / num2;
            break;

        default:
            set_color(LIGHT_RED,BLACK);
            print("Error: ");
            set_color(LIGHT_GREY,BLACK);
            print("Invalid operator\n");
            return;
    }

    // Convert result to string
    char output[20];
    int j = 0;

    if (result == 0) {
        output[j++] = '0';
    } else {
        if (result < 0) {
            print("-");
            result = -result;
        }

        // int start = j;

        while (result > 0) {
            output[j++] = '0' + (result % 10);
            result /= 10;
        }

        // Reverse digits
        for (int k = 0; k < j / 2; k++) {
            char temp = output[k];
            output[k] = output[j - k - 1];
            output[j - k - 1] = temp;
        }
    }

    output[j] = '\0';

    set_color(LIGHT_GREY,BLACK);

    print(output);
    print("\n");
}

// **********************************************
// SPLASH SCREEN
// **********************************************

void splash_screen(void){

print("\n");
print("\n");
print("\n");

set_color(LIGHT_CYAN, BLACK); print("          .o.       ooooooooo.   ooooooooooooo  "); set_color(LIGHT_GREEN, BLACK);  print("   .oooooo.    .oooooo..o \n");
set_color(LIGHT_CYAN, BLACK); print("         .888.      '888   'Y88. 8'   888   '8  "); set_color(LIGHT_GREEN, BLACK);  print("  d8P'  'Y8b  d8P'    'Y8 \n");
set_color(LIGHT_CYAN, BLACK); print("        .8'888.      888   .d88'      888       "); set_color(LIGHT_GREEN, BLACK);  print(" 888      888 Y88bo.      \n");
set_color(LIGHT_CYAN, BLACK); print("       .8' '888.     888ooo88P'       888       "); set_color(LIGHT_GREEN, BLACK);  print(" 888      888  ''Y8888o.  \n");
set_color(LIGHT_CYAN, BLACK); print("      .88ooo8888.    888'88b.         888       "); set_color(LIGHT_GREEN, BLACK);  print(" 888      888      ''Y88b \n");
set_color(LIGHT_CYAN, BLACK); print("     .8'     '888.   888  '88b.       888       "); set_color(LIGHT_GREEN, BLACK);  print(" '88b    d88' oo     .d8P \n");
set_color(LIGHT_CYAN, BLACK); print("    o88o     o8888o o888o  o888o     o888o      "); set_color(LIGHT_GREEN, BLACK);  print("  'Y8bood8P'  8''88888P'  \n");

set_color(WHITE, BLACK);

print("\n");
print("\n");
print("\n");
print("                                Copyright (c) 2026");
print("\n            Mohammad Taha    Mohammad Rayyan    Mohammed Anas Ansari");
print("\n                               All rights reserved.");

}

// **********************************************
// ABOUT
// **********************************************

void about(void) {
print("\n");
// print("-------------------- \n");
print("   System Information  \n");
//print("-------------------- \n");

set_color(LIGHT_GREY,BLACK);

print("Operating System : ART OS \n");
print("Version          : 0.1.0 \n");
print("Architecture     : x86_64 \n");

set_color(WHITE,BLACK);

print("\n");

// print("------------ \n");
print("   Developers  \n");
//print("------------ \n");

set_color(LIGHT_GREY,BLACK);

print("Mohammad Taha \n");
print("Mohammad Rayyan \n");
print("Mohammed Anas Ansari \n");

set_color(WHITE,BLACK);

}
                                                                                                                        
// **********************************************
// STRINGS
// **********************************************

// Compare two strings
int strcmp(const char *s1, const char *s2){
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0'){
        if (s1[i] != s2[i])
            return 1;  // Strings are different
        i++;
    }
    if (s1[i] == '\0' && s2[i] == '\0')
        return 0;      // Strings are equal
    return 1;          // Different lengths
}

// Compare first N characters of two strings
int strncmp(const char *s1, const char *s2, uint64_t n){
    for (uint64_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return 1;
        }
        if (s1[i] == '\0') {
            return 0;
        }
    }
    return 0;
}

// **********************************************
// I/O PORT FUNCTIONS
// **********************************************

static inline void outb(uint16_t port, uint8_t value){
    __asm__ volatile ("outb %0, %1"
                  :
                  : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port){
    uint8_t value;
    __asm__ volatile ("inb %1, %0"
                  : "=a"(value)
                  : "Nd"(port));
    return value;
}

// **********************************************
// CLOCK (Gives UTC Time)
// **********************************************

uint8_t cmos_read(uint8_t reg){
    outb(0x70, reg);
    return inb(0x71);
}

uint8_t bcd_to_bin(uint8_t value){
    return (value & 0x0F) + ((value >> 4) * 10);
}

void get_rtc_time(void){
    uint8_t second = cmos_read(0x00);
    uint8_t minute = cmos_read(0x02);
    uint8_t hour   = cmos_read(0x04);
    uint8_t day    = cmos_read(0x07);
    uint8_t month  = cmos_read(0x08);
    uint8_t year   = cmos_read(0x09);
    uint8_t weekday = cmos_read(0x06);

    second = bcd_to_bin(second);
    minute = bcd_to_bin(minute);
    hour   = bcd_to_bin(hour);
    day    = bcd_to_bin(day);
    month  = bcd_to_bin(month);
    year   = bcd_to_bin(year);
    weekday = bcd_to_bin(weekday);

    print_dec64(day);
    print("\n");
    print_dec64(month);
    print("\n");
    print_dec64(year);
    print("\n");
    print_dec64(hour);
    print("\n");
    print_dec64(minute);
    print("\n");
    print_dec64(second);
    print("\n");
    print_dec64(weekday);
    print("\n");

}

void wait_rtc_update(void) {
    while (cmos_read(0x0A) & 0x80) {
        // Wait until RTC finishes updating
    }
}

void get_date(void) {

    uint8_t day1, month1, year1;
    uint8_t day2, month2, year2;

    // Read RTC until two consecutive readings match
    do {

        wait_rtc_update();

        day1   = cmos_read(0x07);
        month1 = cmos_read(0x08);
        year1  = cmos_read(0x09);

        wait_rtc_update();

        day2   = cmos_read(0x07);
        month2 = cmos_read(0x08);
        year2  = cmos_read(0x09);

    } while (day1 != day2 ||
             month1 != month2 ||
             year1 != year2);

    // Convert BCD to binary
    uint8_t day = bcd_to_bin(day2);
    uint8_t month = bcd_to_bin(month2);
    uint8_t year = bcd_to_bin(year2);

    uint16_t full_year = 2000 + year;

    // Calculate weekday using Zeller's congruence
    uint16_t y = full_year;
    uint8_t m = month;

    if (m < 3) {
        m += 12;
        y--;
    }

    uint8_t weekday =
        (day + (13 * (m + 1)) / 5 +
        y + y / 4 - y / 100 + y / 400) % 7;

    const char *days[] = {
        "Saturday",
        "Sunday",
        "Monday",
        "Tuesday",
        "Wednesday",
        "Thursday",
        "Friday"
    };

    set_color(LIGHT_GREY,BLACK);

    print(days[weekday]);
    print(", ");

    print_dec64(day / 10);
    print_dec64(day % 10);
    print("-");

    print_dec64(month / 10);
    print_dec64(month % 10);
    print("-");

    print_dec64(full_year);

    print("\n");
}

void get_time(void){

    uint8_t sec1, min1, hour1;
    uint8_t sec2, min2, hour2;

    uint8_t status_b = cmos_read(0x0B);

    // Read RTC twice to get a consistent snapshot
    do {

        wait_rtc_update();

        sec1  = cmos_read(0x00);
        min1  = cmos_read(0x02);
        hour1 = cmos_read(0x04);

        wait_rtc_update();

        sec2  = cmos_read(0x00);
        min2  = cmos_read(0x02);
        hour2 = cmos_read(0x04);

    } while (sec1 != sec2 ||
             min1 != min2 ||
             hour1 != hour2);

    // Check RTC format
    int binary_mode = status_b & 0x04;
    int mode_24h = status_b & 0x02;

    // Handle 12-hour PM flag
    int pm = hour2 & 0x80;
    hour2 &= 0x7F;

    // Convert BCD to binary if required
    if (!binary_mode) {
        sec2  = bcd_to_bin(sec2);
        min2  = bcd_to_bin(min2);
        hour2 = bcd_to_bin(hour2);
    }

    // Convert 12-hour format to 24-hour format
    if (!mode_24h) {

        if (hour2 == 12) {
            hour2 = 0;
        }

        if (pm) {
            hour2 += 12;
        }
    }

    set_color(LIGHT_GREY,BLACK);

    // Print HH:MM:SS
    print_dec64(hour2 / 10);
    print_dec64(hour2 % 10);
    print(":");

    print_dec64(min2 / 10);
    print_dec64(min2 % 10);
    print(":");

    print_dec64(sec2 / 10);
    print_dec64(sec2 % 10);

    print(" UTC\n");
}

// **********************************************
// DELAY
// **********************************************

void delay(){
    uint64_t start = __rdtsc();
    uint64_t duration = 9000000000;
    while (__rdtsc() - start < duration){
        __asm__ volatile ("pause");
    }
}

// **********************************************
// IDT
// **********************************************

#define IDT_ENTRIES 256

typedef struct{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) IDTEntry;

typedef struct{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) IDTR;

IDTEntry idt[IDT_ENTRIES] __attribute__((aligned(16)));

void idt_set_gate(int vector, uint64_t handler){
    idt[vector].offset_low  = handler & 0xFFFF;
    idt[vector].selector    = 0x18;
    idt[vector].ist         = 0;
    idt[vector].type_attr   = 0x8E;
    idt[vector].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[vector].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[vector].reserved    = 0;
};

__attribute__((interrupt)) void exception_handler(struct interrupt_frame *frame){
    (void)frame;
    print("EXCEPTION OCCURRED!");
    while (1)
        __asm__ volatile ("hlt");
}

__attribute__((interrupt)) void exception_error_handler(struct interrupt_frame *frame, uint64_t error_code){
    (void)frame;
    (void)error_code;
    print("EXCEPTION WITH ERROR CODE OCCURRED!");
    while (1)
        __asm__ volatile ("hlt");
}

void idt_init(void){
    // Default handler for vectors without error codes.
    for (int i = 0; i < IDT_ENTRIES; i++){
        idt_set_gate(i, (uint64_t)exception_handler);
    }
    // Exceptions that push error codes.
    int error_vectors[] = {
        8,      // Double Fault
        10,     // Invalid TSS
        11,     // Segment Not Present
        12,     // Stack-Segment Fault
        13,     // General Protection Fault
        14,     // Page Fault
        17,     // Alignment Check
        21,     // Control Protection Exception
        29,     // VMM Communication Exception
        30      // Security Exception
    };

    for (unsigned int i = 0; i < sizeof(error_vectors) / sizeof(error_vectors[0]); i++){
        idt_set_gate(error_vectors[i], (uint64_t)exception_error_handler);
    }

    // Register keyboard interrupt handler
    idt_set_gate(0x21,(uint64_t)keyboard_handler);

    IDTR idtr;

    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;

    __asm__ volatile ("lidt %0" : : "m"(idtr) : "memory");
}

// This function is for testing IDT divide by zero exception
void test_divide_by_zero(void){
    volatile int a = 10;
    volatile int b = 0;
    int result = a / b;
    (void)result;
}

// **********************************************
// KEYBOARD
// **********************************************

static inline void io_wait(void){
    outb(0x80, 0);
}

// **********************************************
// PIC
// **********************************************

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIC_EOI      0x20

void pic_remap(void){

    // Start initialization
    outb(PIC1_COMMAND, 0x11);
    io_wait();

    outb(PIC2_COMMAND, 0x11);
    io_wait();

    // Set interrupt offsets
    outb(PIC1_DATA, 0x20);
    io_wait();

    outb(PIC2_DATA, 0x28);
    io_wait();

    // Tell master PIC about slave
    outb(PIC1_DATA, 0x04);
    io_wait();

    outb(PIC2_DATA, 0x02);
    io_wait();

    // 8086 mode
    outb(PIC1_DATA, 0x01);
    io_wait();

    outb(PIC2_DATA, 0x01);
    io_wait();

    // Mask everything initially
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(uint8_t irq){
    if(irq >= 8){
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

// **********************************************
// ENABLE KEYBOARD IRQ
// **********************************************

void keyboard_enable_irq(void){
    // Unmask IRQ1
    outb(PIC1_DATA, 0xFD);
    // Mask slave PIC
    outb(PIC2_DATA, 0xFF);
}

// **********************************************
// PS/2 KEYBOARD
// **********************************************

#define KEYBOARD_DATA    0x60
#define KEYBOARD_STATUS  0x64

#define KEYBOARD_BUFFER_SIZE 128

volatile uint8_t keyboard_buffer[KEYBOARD_BUFFER_SIZE];

volatile uint32_t keyboard_head = 0;
volatile uint32_t keyboard_tail = 0;

// Called from keyboard interrupt
__attribute__((interrupt)) void keyboard_handler(struct interrupt_frame *frame) {
    (void)frame;
    // Read keyboard scan code
    uint8_t scancode = inb(KEYBOARD_DATA);

    // Left Shift pressed
    if (scancode == 0x2A) {
        left_shift_pressed = 1;
    }
    // Left Shift released
    else if (scancode == 0xAA) {
        left_shift_pressed = 0;
    }
    // Right Shift pressed
    else if (scancode == 0x36) {
        right_shift_pressed = 1;
    }
    // Right Shift released
    else if (scancode == 0xB6) {
        right_shift_pressed = 0;
    }
    else {
        // Save it in the buffer
        uint32_t next = (keyboard_head + 1) % KEYBOARD_BUFFER_SIZE;

        if (next != keyboard_tail) {
            keyboard_buffer[keyboard_head] = scancode;
            keyboard_head = next;
        }
    }
    // Tell PIC that interrupt is handled
    pic_send_eoi(1);
}

// Get a scan code from the buffer
int keyboard_get_scancode(uint8_t *scancode){
    if(keyboard_head == keyboard_tail){
        return 0;
    }
    *scancode = keyboard_buffer[keyboard_tail];
    keyboard_tail = (keyboard_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return 1;
}

// **********************************************
// KEYBOARD INITIALIZATION
// **********************************************

int ps2_wait_input(void){
    for(int i = 0; i < 100000; i++){
        if(!(inb(KEYBOARD_STATUS) & 0x02)){
            return 1;
        }
    }
    return 0;
}

int ps2_wait_output(void){
    for(int i = 0; i < 100000; i++){
        if(inb(KEYBOARD_STATUS) & 0x01){
            return 1;
        }
    }

    return 0;
}

void keyboard_init(void){
    // Disable first PS/2 port
    ps2_wait_input();
    outb(KEYBOARD_STATUS, 0xAD);
    // Flush pending data
    if(inb(KEYBOARD_STATUS) & 0x01){
        (void)inb(KEYBOARD_DATA);
    }
    // Read controller configuration
    ps2_wait_input();
    outb(KEYBOARD_STATUS, 0x20);
    uint8_t config = 0;
    if(ps2_wait_output()){
        config = inb(KEYBOARD_DATA);
    }
    // Enable IRQ1 and enable keyboard clock
    config |= 0x01;
    config &= ~(1 << 4);
    // Write configuration
    ps2_wait_input();
    outb(KEYBOARD_STATUS, 0x60);
    ps2_wait_input();
    outb(KEYBOARD_DATA, config);
    // Enable first PS/2 port
    ps2_wait_input();
    outb(KEYBOARD_STATUS, 0xAE);
    // Enable keyboard scanning
    ps2_wait_input();
    outb(KEYBOARD_DATA, 0xF4);
    // Consume keyboard ACK if available
    if(ps2_wait_output()){
        (void)inb(KEYBOARD_DATA);
    }
}

// **********************************************
// SCANCODE TO ASCII
// **********************************************

char scancode_to_ascii(uint8_t scancode) {

    static const char normal[128] = {
        [0x02] = '1', [0x03] = '2',
        [0x04] = '3', [0x05] = '4',
        [0x06] = '5', [0x07] = '6',
        [0x08] = '7', [0x09] = '8',
        [0x0A] = '9', [0x0B] = '0',

        [0x0C] = '-',
        [0x0D] = '=',
        [0x0E] = '\b',
        [0x0F] = '\t',

        [0x10] = 'q', [0x11] = 'w',
        [0x12] = 'e', [0x13] = 'r',
        [0x14] = 't', [0x15] = 'y',
        [0x16] = 'u', [0x17] = 'i',
        [0x18] = 'o', [0x19] = 'p',

        [0x1A] = '[', [0x1B] = ']',
        [0x1C] = '\n',

        [0x1E] = 'a', [0x1F] = 's',
        [0x20] = 'd', [0x21] = 'f',
        [0x22] = 'g', [0x23] = 'h',
        [0x24] = 'j', [0x25] = 'k',
        [0x26] = 'l',

        [0x27] = ';', [0x28] = '\'',
        [0x29] = '`', [0x2B] = '\\',

        [0x2C] = 'z', [0x2D] = 'x',
        [0x2E] = 'c', [0x2F] = 'v',
        [0x30] = 'b', [0x31] = 'n',
        [0x32] = 'm',

        [0x33] = ',', [0x34] = '.',
        [0x35] = '/', [0x39] = ' '
    };

    static const char shifted[128] = {
        [0x02] = '!', [0x03] = '@',
        [0x04] = '#', [0x05] = '$',
        [0x06] = '%', [0x07] = '^',
        [0x08] = '&', [0x09] = '*',
        [0x0A] = '(', [0x0B] = ')',

        [0x0C] = '_',
        [0x0D] = '+',

        [0x1A] = '{',
        [0x1B] = '}',

        [0x27] = ':',
        [0x28] = '"',
        [0x29] = '~',
        [0x2B] = '|',

        [0x33] = '<',
        [0x34] = '>',
        [0x35] = '?'
    };

    if (scancode >= 128)
        return 0;

    if (left_shift_pressed || right_shift_pressed) {
        if (shifted[scancode])
            return shifted[scancode];

        char c = normal[scancode];

        if (c >= 'a' && c <= 'z')
            return c - 'a' + 'A';

        return c;
    }

    return normal[scancode];
}

// **********************************************
// INPUT FUNCTION
// **********************************************

void input(char *buffer, uint64_t max_length){
    uint64_t index = 0;
    uint8_t scancode;
    while(1){
        // Wait for keyboard input
        if(!keyboard_get_scancode(&scancode)){
            __asm__ volatile ("hlt");
            continue;
        }
        // Ignore key release events
        if(scancode & 0x80){
            continue;
        }
        char c = scancode_to_ascii(scancode);
        // Ignore unsupported keys
        if(c == 0){
            continue;
        }
        // ENTER
        if(c == '\n'){
            put_char('\n');
            buffer[index] = '\0';
            return;
        }
        // BACKSPACE
        if(c == '\b'){
            if(index > 0){
                index--;
                put_char('\b');
            }
            continue;
        }
        // Normal character
        if(index < max_length - 1){
            buffer[index++] = c;
            put_char(c);
        }
    }
}

// **********************************************
// KERNEL MAIN
// **********************************************

void kernel_main(boot_info *info){

    // uint64_t entry_count = info->entry_count;
    // uint64_t base = info->memory_map->base;
    
    (void)info;    // Explicitly mark info as unused to suppress compiler warnings

    __asm__ volatile ("cli");
    idt_init();
    pic_remap();
    idt_set_gate(0x21, (uint64_t)keyboard_handler);
    keyboard_init();
    keyboard_enable_irq();
    __asm__ volatile ("sti");

    initialize_screen();

    // Splash Screen
    splash_screen();
    delay();
    clear_screen();

    char command[100];

    // Terminal

    print("ART OS [Version 0.1.0] \n");
    print("Copyright (c) 2026 Mohammad Taha, Mohammad Rayyan, Mohammed Anas Ansari. \n");
    print("All rights reserved.");
    print("\n");

    while (1) {

        print("\n");
        set_color(LIGHT_GREEN, BLACK);
        print("root");
        set_color(LIGHT_GREY, BLACK);
        print("@");
        set_color(LIGHT_CYAN, BLACK);
        print("ART");
        set_color(WHITE, BLACK);
        print(" > ");

        input(command, 100);

        if (strcmp(command, "") == 0) {
            // Do nothing
        }
        else if (strcmp(command, "about") == 0) {
            about();
        }
        else if (strcmp(command, "clear") == 0) {
            clear_screen();
        }
        else if (strncmp(command, "calc ", 5) == 0) {
            calculate(command);
        }
        else if (strcmp(command, "date") == 0) {
            get_date();
        }
        else if (strcmp(command, "echo") == 0) {
            print("\n");
        }
        else if (strncmp(command, "echo ", 5) == 0) {
            set_color(LIGHT_GREY,BLACK);
            print(command + 5);
            print("\n");
        }
        else if (strcmp(command, "hello") == 0) {
            set_color(LIGHT_GREY,BLACK);
            print("Hello from myOS!\n");
        }
        else if (strcmp(command, "help") == 0) {
                print("\n");
                print("List of available commands\n");
                print("\n");
                set_color(LIGHT_GREY,BLACK);
                print("about - Display OS information\n");
                print("calc  - Perform calculations\n");
                print("clear - Clear terminal screen\n");
                print("date  - Display current date\n");
                print("echo  - Print text to terminal\n");
                print("hello - Display greeting message\n");
                print("help  - Display available commands\n");
                print("time  - Display current time\n");
        }
        else if (strcmp(command, "time") == 0) {
            get_time();
        }
        else {
            set_color(LIGHT_RED,BLACK);
            print("Error: ");
            set_color(LIGHT_GREY, BLACK);
            print("Unknown command\n");
        }
    }

    while(1){
        __asm__ volatile ("hlt");
    }
}
