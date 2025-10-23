# Backend and Frontend Integration Guide

## Short Answer

The backend exposes everything you need. The frontend can poll the binary and render live views. Press "Cmd + Shift + V" to view this document in a formatted manner.

---

## What the Backend Already Provides

The backend provides the following data and functionality, which is suitable for both "Download JSON" and live polling:

* **CPU**:

  * `collect_cpu_info` uses:

    * `get_cpu_usage`
    * `get_cpu_frequency`
    * `get_no_logical_processors`
    * `get_per_core_usage`
* **Memory**:

  * `collect_memory_info` uses:

    * `get_memory_usage`
    * `get_mem_value`
* **Processes**:

  * `collect_process_info` returns an array from `get_all_processes`, serialized by `process_to_json`
* **Disk**:

  * `collect_disk_info` uses:

    * `get_disk_stats`
    * `disk_to_json`
* **Network**:

  * `collect_network_info` uses:

    * `get_network_rates`
    * `network_to_json`
* **Battery**:

  * `collect_battery_info` uses:

    * `get_battery_info`
    * `battery_to_json`

---

## Processes Tab Requirements

### Common Columns:

The following columns are present in the JSON returned by `process_to_json`:

* `process_id`
* `process_name`
* `status`
* `threads`
* `type`
* `user`

### Filters:

* **CPU Filter**: Available as `cpu.cpu_time` and `cpu.cpu_usage` per process (computed over a sampling window) from `get_all_processes`.
* **Memory Filter**: Available as `memory.memory_usage_kb` and `memory.memory_percent` from `get_all_processes`.
* **"App" vs "Background Process"**: Identified as `p.type` in `get_all_processes`.

---

## Network Tab (Line Graph)

* Per-interface upload/download rates (in bytes/sec) are available in the JSON via `network_to_json`.
  The frontend can select the active interface and append points to a line graph over time.

---

## Process Killing

Process killing is implemented via the CLI.

* See `cli::run` and `kill_process`.
* The frontend can invoke the binary with the `kill` subcommand and read the JSON result.

---

## Downloading JSON

The frontend can run the binary `system_monitor` and save `stdout` verbatim.

* Pretty-printed JSON output is provided in `main.cpp`.

---

## Caveats for "Real-Time" Refresh

### Minimum Effective Polling Interval:

* The backend internally samples the following data:

  * **CPU% and Network Rates**:

    * `get_network_rates` sleeps for 1 second to compute rates.
    * `get_all_processes` sleeps for ~500ms to compute per-process CPU usage.
  * **Total Time for One Poll**:
    A single sampling cycle typically takes ~1.5 seconds.
    If the frontend refresh interval is set below 1.5 seconds, the backend will still complete at this cadence.
    **Recommendation**: Set the refresh interval to ≥ 1500–2000 ms for smooth updates.

## Final Reminder

Things to implement:

* **Live Processes Table** with CPU and memory filters (using data from `get_all_processes`).
* **Live Network Line Graph** (using data from `get_network_rates`).
* **Adjustable Refresh** by frontend polling cadence.
* **Download JSON** button to capture stdout from `system_monitor`.

**Expectation**: The backend's internal sampling rate of ~1.5 seconds means "real-time" refreshes should have a minimum refresh interval of ~1.5 seconds to reflect new data with each tick.