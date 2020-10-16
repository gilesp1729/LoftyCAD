#pragma once

#define MAX_SLICERS             8

// What kind of slicer we are dealing with
typedef enum
{
    SLIC_SLIC3R,                                // Original slic3r command set and ini files
    SLIC_PRUSA,                                 // PrusaSlicer 2+ command set and ini files
    MAX_TYPES                                   // Must be last
} SLICER;

typedef struct SlicerExe
{
    char        exe[MAX_PATH];                      // Fully qualified executable
} SlicerExe;

typedef struct SlicerConfig
{
    char        dir[MAX_PATH];                      // Config directory
    char        ini[MAX_PATH];                      // INI file name in that directory
    SLICER      type;                               // What kind of slicer this is
} SlicerConfig;


extern char slicer_cmd[MAX_TYPES][80];
extern SlicerExe slicer_exe[MAX_SLICERS];
extern SlicerConfig slicer_config[MAX_SLICERS];
extern int slicer_index;
extern int config_index;
extern int num_slicers;
extern int num_configs;
extern BOOL print_octo;
extern char printer_port[64];
extern char octoprint_server[128];
extern char octoprint_apikey[128];
extern BOOL explicit_gcode;


