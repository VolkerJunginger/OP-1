# Pulse Sync 1.1.9

Sync the original OP-1 to an external pulse clock through Line In. Requires firmware #246 and **Opus core 2.58 or newer**.

[Download plugin](../../../../pulsesync.opmod) · [Manual](../../../../MANUAL.txt) · [Changelog](../../../../CHANGELOG.md) · [Complete package and source](../../../../downloads/Pulse-Sync-1.1.9.zip)

Select **L+R**, **L**, or **R** for the clock. The chosen channels are muted before the input mix, so you can send clock on one side and audio on the other. The default is **L+R**. The blue label on Shift + Mic shows the selection.

The build and host tests passed. Hardware verification of 1.1.9 is pending.

The ZIP includes the pinned Opus SDK and source. Build with Blackfin GCC using `python3 Source/plugin-kit/plugin_tool.py build Source/plugin-kit/plugins/esc.pulsesync`. See [LICENSE](LICENSE) and [NOTICE](NOTICE.md).
