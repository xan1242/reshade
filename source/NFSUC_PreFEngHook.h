#pragma once
// NFS Undercover - ReShade Pre FENg Hook
#define FEMANAGER_RENDER_HOOKADDR1 0x007AE5D8
#define NFSUC_EXIT1 0x007AE5DE
#define NFSUC_EXIT2 0x07AE635
#define NFS_D3D9_DEVICE_ADDRESS 0x00EA0110
#define DRAW_FENG_BOOL_ADDR 0x00D52ECA

void ReShade_EntryPoint();
