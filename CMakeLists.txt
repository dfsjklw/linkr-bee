cmake_minimum_required(VERSION 3.20.0)

# ---------------------------------------------------------------------------
# Patches applied to the west-managed Zephyr tree.
#
# They live here rather than in the checkout so that `west update` cannot drop
# them silently and so that everyone building this repository gets the same
# image. Each patch is applied once and only if it is not already present,
# which keeps repeat builds and in-place checkouts idempotent.
# ---------------------------------------------------------------------------
function(linkr_apply_zephyr_patch patch_rel)
  if(NOT EXISTS "${ZEPHYR_BASE}/.git")
    message(WARNING "Zephyr tree at ${ZEPHYR_BASE} is not a git checkout; "
                    "cannot verify ${patch_rel}. Applying unconditionally.")
    execute_process(
      COMMAND git -C "${ZEPHYR_BASE}" apply "${CMAKE_CURRENT_SOURCE_DIR}/${patch_rel}"
      RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
    if(NOT rc EQUAL 0)
      message(FATAL_ERROR "Failed to apply ${patch_rel}: ${out}")
    endif()
    return()
  endif()

  execute_process(
    COMMAND git -C "${ZEPHYR_BASE}" apply --reverse --check
            "${CMAKE_CURRENT_SOURCE_DIR}/${patch_rel}"
    RESULT_VARIABLE already OUTPUT_QUIET ERROR_QUIET)
  if(already EQUAL 0)
    message(STATUS "Zephyr patch already applied: ${patch_rel}")
    return()
  endif()

  execute_process(
    COMMAND git -C "${ZEPHYR_BASE}" apply "${CMAKE_CURRENT_SOURCE_DIR}/${patch_rel}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR
      "Failed to apply ${patch_rel} to ${ZEPHYR_BASE}: ${out}\n"
      "Restore the tree (git -C <zephyr> checkout .) and rebuild.")
  endif()
  message(STATUS "Applied Zephyr patch: ${patch_rel}")
endfunction()

# Serialize shared_multi_heap: sys_heap has no locking of its own and the
# Espressif WiFi stack calls the PSRAM allocator from both ISR and thread
# context.
set(LINKR_ZEPHYR_PATCHES
  patches/zephyr/0001-shared-multi-heap-serialize-access-with-a-spinlock.patch
)
foreach(patch IN LISTS LINKR_ZEPHYR_PATCHES)
  linkr_apply_zephyr_patch("${patch}")
endforeach()

# The module runs after the SoC is configured, before Zephyr collects its
# post-build commands.
list(APPEND EXTRA_ZEPHYR_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/esp_simpleboot")
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(linkr_ble_bridge)

if(CONFIG_SOC_SERIES_ESP32C3)
  include(cmake/esp32c3_hci_order.cmake)
endif()

# ESP32 (Xtensa) uses the same HCI driver; apply the same ordering fix.
if(CONFIG_SOC_SERIES_ESP32 AND NOT CONFIG_SOC_SERIES_ESP32C3)
  include(cmake/esp32c3_hci_order.cmake)
endif()

target_sources(app PRIVATE
  src/serial_session.c
  src/ble_peers.c
  src/target_binding.c
  src/ble_security.c
  src/ble_nus.c
  src/ble_mgmt.c
  src/ble_uart_reliable.c
  src/main.c
  src/uart_autobaud.c
  src/uart_rmt_span.c
  src/uart_bulk_tx.c
  src/wifi.c
  src/ws_bridge.c
)
target_sources_ifdef(CONFIG_LINKR_BLE_BRIDGE_WATCHDOG app PRIVATE src/watchdog.c)

# HTTP service resources live in named iterable sections (WebSocket bridge).
zephyr_linker_sources(SECTIONS sections-rom.ld)
