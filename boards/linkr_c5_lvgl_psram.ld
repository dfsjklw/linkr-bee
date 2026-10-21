/*
 * Relocate the LVGL rendering buffers (.lvgl_buf) and the LVGL object heap
 * (.lvgl_heap) into the ESP32-C5 PSRAM.
 *
 * Why a hand-written fragment instead of a devicetree memory-region:
 * soc/espressif/esp32c5/default.ld defines its own MEMORY block and never
 * calls LINKER_DT_REGIONS(), so 'zephyr,memory-region' nodes are ignored and
 * their sections become orphans (which the linker folds into IDT_LIST).
 *
 * Why 0x42700000:
 *   - PSRAM is mapped at 0x420E0000 for the current image (the linker's
 *     .ext_ram.dummy placeholder covers the XIP windows).
 *   - The shared multi heap (CONFIG_ESP_SPIRAM_HEAP_SIZE = 4 MB) starts at
 *     that same mapped base and therefore ends at 0x424E0000.
 *   - The linker's .ext_ram.bss sits at 0x420E0000 as well, i.e. it overlaps
 *     the heap, so LVGL must not use it.
 *   - 0x42700000 leaves ~2 MB of headroom above the heap and still ends well
 *     inside the 8 MB PSRAM window (0x428E0000).
 *
 * Keep this address in sync with CONFIG_ESP_SPIRAM_HEAP_SIZE: with a larger
 * heap the window below 0x42700000 grows and may collide with it.
 *
 * This file is included through
 *   zephyr_linker_sources(SECTIONS boards/linkr_c5_lvgl_psram.ld)
 * which lands (snippets-sections.ld) *before* the .ext_ram.data output
 * section in the ESP32-C5 script, so the KEEP() rules there find nothing and
 * that section stays empty.
 */

.lvgl_psram 0x42700000 (NOLOAD) :
{
	__lvgl_psram_start = .;

	KEEP(*(.lvgl_buf))
	KEEP(*(.lvgl_buf.*))
	KEEP(*(.lvgl_heap))
	KEEP(*(.lvgl_heap.*))

	/* Linkr's own PSRAM scratch (terminal scrollback). Internal SRAM cannot
	 * hold it next to the BLE/WiFi stacks.
	 */
	KEEP(*(.linkr_psram))
	KEEP(*(.linkr_psram.*))

	. = ALIGN(16);
	__lvgl_psram_end = .;
}

/*
 * Guard against future code growth: .ext_ram.dummy (which shifts the mapped
 * PSRAM base, and therefore the shared multi heap, upwards) is derived from
 * the XIP window sizes, so a large enough image would eventually collide with
 * this fixed window.
 */
ASSERT(_ext_ram_heap_end <= 0x42700000,
       "shared multi heap overlaps the LVGL PSRAM window; move the address in linkr_c5_lvgl_psram.ld")
